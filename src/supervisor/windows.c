/**
 * Copyright (c) 2011-2012 William Light <wrl@illest.net>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>

#define WINVER 0x501

#include <windows.h>
#include <process.h>
#include <Winreg.h>
#include <Dbt.h>
#include <io.h>

/* damnit mingw */
#ifndef JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
#define JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE 0x00002000
#endif

#include "serialosc.h"
#include "ipc.h"

struct supervisor_state {
	HANDLE reaper_job;
	HANDLE detector_pipe;

	char *exec_path;
	const char *quoted_exec_path;
};

struct supervisor_state state;

static ssize_t overlapped_read(HANDLE h, uint8_t *buf, size_t nbyte)
{
	OVERLAPPED ov = {0, 0, {{0, 0}}};
	DWORD read = 0;

	if( !(ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) ) {
		fprintf(stderr,
				"overlapped_read(): could not allocate event (%ld)\n",
				GetLastError());
		return -1;
	}

	if( !ReadFile(h, buf, nbyte, &read, &ov) ) {
		if( GetLastError() != ERROR_IO_PENDING ) {
			fprintf(stderr, "overlapped_read(): read failed (%ld)\n",
					GetLastError());
			return -1;
		}

		GetOverlappedResult(h, &ov, &read, TRUE);
	}

	CloseHandle(ov.hEvent);
	return read;
}

int spawn_server(const char *devnode) {
	intptr_t proc;

	proc = _spawnlp(_P_NOWAIT, state.exec_path, state.quoted_exec_path,
                    devnode, NULL);

	if( proc < 0 ) {
		perror("dang");
		return 1;
	}

	AssignProcessToJobObject(state.reaper_job, (HANDLE) proc);

	return 0;
}

static int setup_reaper_job()
{
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
	memset(&jeli, '\0', sizeof(jeli));

	if( !(state.reaper_job = CreateJobObject(NULL, NULL)) )
		return 1;

	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if( !SetInformationJobObject(
			state.reaper_job, JobObjectExtendedLimitInformation, &jeli,
			sizeof(jeli)) )
		return 1;

	return 0;
}

static char *get_service_binpath()
{
	SC_HANDLE manager, service;
	LPQUERY_SERVICE_CONFIG config;
	DWORD config_size;
	char *bin_path;

	if( !(manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE)) )
		goto err_manager;

	if( !(service = OpenService(manager, SOSC_WIN_SERVICE_NAME, SERVICE_QUERY_CONFIG)) )
		goto err_service;

	QueryServiceConfig(service, NULL, 0, &config_size);

	if( !(config = s_malloc(config_size)) )
		goto err_malloc;

	if( !QueryServiceConfig(service, config, config_size, &config_size) )
		goto err_query;

	bin_path = s_strdup(config->lpBinaryPathName);
	s_free(config);

	CloseServiceHandle(service);
	CloseServiceHandle(manager);
	return bin_path;

err_query:
	s_free(config);
err_malloc:
	CloseServiceHandle(service);
err_service:
	CloseServiceHandle(manager);
err_manager:
	return NULL;
}

static int init_detector_pipe()
{
	state.detector_pipe = CreateNamedPipe(
		SOSC_DETECTOR_PIPE,
		PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE,
		1,
		64,
		64,
		0,
		NULL);

	if (state.detector_pipe == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "supervisor: can't create detector pipe\n");
		return 1;
	}

	return 0;
}

void pipe_read(HANDLE pipe)
{
	char buf[128];
	ssize_t read;

	read = overlapped_read(pipe, (uint8_t *) buf, sizeof(buf));

	if (read > 0) {
		buf[read] = '\0';
		printf("[+] read %d bytes\n", read);
		printf("\"%s\"\n", buf);
	} else
		printf("[-] read failed\n");
}

int overlapped_connect_pipe(HANDLE p)
{
	OVERLAPPED ov = {0, 0, {{0, 0}}};
	DWORD what, err;

	if (!(ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		fprintf(stderr,
				"overlapped_connect_pipe(): could not allocate event (%ld)\n",
				GetLastError());
		return -1;
	}

	if (!ConnectNamedPipe(p, &ov)) {
		switch ((err = GetLastError())) {
		case ERROR_PIPE_CONNECTED:
			what = 0;
			goto done;

		case ERROR_IO_PENDING:
			break;

		default:
			fprintf(stderr, "overlapped_connect_pipe(): connect failed (%ld)\n",
					err);
			return -1;
		}

		GetOverlappedResult(p, &ov, &what, TRUE);
	}

done:
	CloseHandle(ov.hEvent);
	return what;
}

int read_ipc_msg_from(int fd)
{
	sosc_ipc_msg_t msg;
	DWORD available;
	ssize_t nbytes;
	HANDLE h;

	h = (HANDLE) _get_osfhandle(fd);
	((void) h);

	if (!PeekNamedPipe(h, NULL, 0, NULL, &available, NULL) || !available)
		return 0;

	nbytes = sosc_ipc_msg_read(fd, &msg);

	if (nbytes > 0)
		printf("[+] read %d bytes\n", nbytes);
	else {
		printf("[-] read %d bytes\n", nbytes);
		return -1;
	}

	switch (msg.type) {
	case SOSC_DEVICE_CONNECTION:
		fprintf(stderr, "connection, port %s\n", msg.connection.devnode);
		break;

	default:
		break;
	}

	return nbytes;
}

void fuck()
{
	int fd = _open_osfhandle((intptr_t) state.detector_pipe, 0);
	Sleep(250);
	while (read_ipc_msg_from(fd));
}

int sosc_supervisor_run(char *progname)
{
	if (init_detector_pipe())
		goto err_detector_pipe;

	if (!(state.exec_path = get_service_binpath()))
		goto err_get_binpath;

	if (!(state.quoted_exec_path = s_asprintf("\"%s\"", state.exec_path))) {
		fprintf(stderr, "detector_run() error: couldn't allocate memory\n");
		goto err_asprintf;
	}

	if (setup_reaper_job())
		goto err_reaper;

	overlapped_connect_pipe(state.detector_pipe);
	fuck();
	return 0;

err_reaper:
err_asprintf:
	s_free(state.exec_path);
err_get_binpath:
err_detector_pipe:
	return 1;
}
