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

#define ARRAY_LENGTH(x)  (sizeof(x) / sizeof(*x))

#define PIPE_BUF         64
#define SOSC_DEVICE_PIPE (SOSC_PIPE_PREFIX "devices")

#define MAX_DEVICES      32
#define DETECTOR_EVENT   (MAX_DEVICES)
#define OSC_EVENT        (MAX_DEVICES + 1)
#define EVENT_COUNT      (MAX_DEVICES + 2) /* 1 for osc, 1 for detector */

#include "serialosc.h"
#include "ipc.h"

struct device_info {
	uint16_t port;
	char *serial;
	char *friendly;
};

struct incoming_pipe {
	OVERLAPPED ov;
	HANDLE pipe;

	struct {
		char buf[PIPE_BUF];
		DWORD nbytes;
	} read;

	enum {
		UNCONNECTED,
		NOT_READY,
		READY
	} status;

	struct device_info info;
};

struct supervisor_state {
	HANDLE reaper_job;
	struct incoming_pipe detector;

	char *exec_path;
	const char *quoted_exec_path;
};

struct supervisor_state state;

/*************************************************************************
 * overlapped i/o helpers
 *************************************************************************/

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

static int overlapped_connect_pipe(HANDLE p, OVERLAPPED *ov)
{
	DWORD err;

	if (!ConnectNamedPipe(p, ov)) {
		switch ((err = GetLastError())) {
		case ERROR_PIPE_CONNECTED:
			SetEvent(ov->hEvent);
			return 1;

		case ERROR_IO_PENDING:
			break;

		default:
			fprintf(stderr, "overlapped_connect_pipe(): connect failed (%ld)\n",
					err);
			return -1;
		}
	}

	return 0;
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

int pipe_read(HANDLE pipe)
{
	sosc_ipc_msg_t *msg;
	char buf[PIPE_BUF];
	ssize_t read;

	read = overlapped_read(pipe, (uint8_t *) buf, sizeof(buf));

	if (read <= 0) {
		fprintf(stderr, "[-] read failed\n");
		return read;
	}

	fprintf(stderr, "[+] read %d bytes\n", read);
	if (sosc_ipc_msg_from_buf((uint8_t *) buf, read, &msg))
		fprintf(stderr, "[-] bad message\n");

	switch (msg->type) {
	case SOSC_DEVICE_CONNECTION:
		fprintf(stderr, "connection, port %s\n", msg->connection.devnode);
		break;

	case SOSC_DEVICE_INFO:
		fprintf(stderr, "info\n");
		break;

	default:
		break;
	}

	return read;
}

/*************************************************************************
 * osc stuff
 *************************************************************************/

HANDLE events[EVENT_COUNT];
struct incoming_pipe pipe_info[MAX_DEVICES] = {
	[0 ... (MAX_DEVICES - 1)] = {
		.ov     = {0, 0, {{0, 0}}},
		.status = UNCONNECTED,
		.info   = {
			.port   = 0,
			.serial = NULL,
			.friendly = NULL
		}
	}
};

/*************************************************************************
 * supervisor read loop
 *************************************************************************/

static int handle_read(struct incoming_pipe *p)
{
	sosc_ipc_msg_t *msg;

	fprintf(stderr, "[+] read complete, got %d bytes\n", p->read.nbytes);

	if (sosc_ipc_msg_from_buf((uint8_t *) p->read.buf, p->read.nbytes, &msg)) {
		fprintf(stderr, "[-] bad message\n");
		return -1;
	}

	switch (msg->type) {
	case SOSC_DEVICE_CONNECTION:
		fprintf(stderr, "connection, port %s\n", msg->connection.devnode);
		break;

	case SOSC_DEVICE_INFO:
		fprintf(stderr, "info\n");
		break;

	default:
		break;
	}

	return 0;
}

static int handle_pipe(struct incoming_pipe *p)
{
	int res;

	res = GetOverlappedResult(p->pipe, &p->ov, &p->read.nbytes, FALSE);

	switch (p->status) {
	case UNCONNECTED:
		if (!res)
			return -1;

		p->status = NOT_READY;
		break;

	case NOT_READY:
	case READY:
		if (!res || !p->read.nbytes) {
			/* disconnect, reconnect */
			return 0;
		}

		handle_read(p);

		break;
	}

	fprintf(stderr, "[+] queueing a read...\n");

queue_read:
	res = ReadFile(
		p->pipe,
		p->read.buf,
		sizeof(p->read.buf),
		&p->read.nbytes,
		&p->ov);

	if (res && p->read.nbytes > 0) {
		fprintf(stderr, "[!] immediate return\n");
		handle_read(p);
		goto queue_read;
	}

	if (!res) {
		switch ((res = GetLastError())) {
		case ERROR_IO_PENDING:
			return 0;

		default:
			fprintf(stderr, "supervisor: handle_pipe(): error %ld\n", res);
			return -1;
		}
	}

	/* disconnect, reconnect? */
	return 0;
}

static void read_loop()
{
	int which;

	Sleep(100);

	fprintf(stderr, "\n[+] supervisor: read_loop() start\n");

	for (;;) {
		which = WaitForMultipleObjects(
			EVENT_COUNT,
			events,
			FALSE,
			INFINITE);

		if (which == WAIT_FAILED) {
			fprintf(stderr,
				"supervisor: read_loop(): WaitForMultipleObjects failed (%ld)\n",
				GetLastError());
			return;
		}

		which -= WAIT_OBJECT_0;
		fprintf(stderr, "[!] object %d ready\n", which);

		switch (which) {
		case DETECTOR_EVENT:
			if (handle_pipe(&state.detector))
				fprintf(stderr,
					"supervisor: read_loop(): failure in handle_pipe "
					"(detector pipe: %ld)\n", GetLastError());
			break;

		case OSC_EVENT:
			break;

		default:
			if (handle_pipe(&pipe_info[which]))
				fprintf(stderr,
					"supervisor: read_loop(): failure in handle_pipe "
					"(device pipe %d: %ld)\n", which, GetLastError());
			break;
		}
	}
}

/*************************************************************************
 * pipe initialization
 *************************************************************************/

static int init_device_events()
{
	struct incoming_pipe *p;
	int i;

	for (i = 0; i < MAX_DEVICES; i++) {
		p = &pipe_info[i];

		if (!(events[i] = CreateEvent(NULL, TRUE, TRUE, NULL))) {
			fprintf(stderr, "supervisor: couldn't allocate event %d\n", i);
			return -1;
		}

		p->ov.hEvent = events[i];

		p->pipe = CreateNamedPipe(
			SOSC_DEVICE_PIPE,
			PIPE_ACCESS_INBOUND
				| FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE
				| PIPE_WAIT,
			MAX_DEVICES,
			PIPE_BUF,
			PIPE_BUF,
			0,
			NULL);

		if (p->pipe == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "supervisor: can't create device pipe for %d\n", i);
			return -1;
		}

		switch (overlapped_connect_pipe(p->pipe, &p->ov)) {
		case 0:
			p->status = UNCONNECTED;
			break;

		case 1:
			p->status = NOT_READY;
			break;

		default:
			/* error already printed by overlapped_connect_pipe() */
			return -1;
		}
	}

	return 0;
}

static int init_detector_pipe()
{
	struct incoming_pipe *p = &state.detector;

	if (!(events[DETECTOR_EVENT] = CreateEvent(NULL, TRUE, TRUE, NULL))) {
		fprintf(stderr, "supervisor: couldn't allocate detector event\n");
		return -1;
	}

	p->ov.hEvent = events[DETECTOR_EVENT];

	p->pipe = CreateNamedPipe(
		SOSC_DETECTOR_PIPE,
		PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_MESSAGE,
		1,
		PIPE_BUF,
		PIPE_BUF,
		0,
		NULL);

	if (p->pipe == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "supervisor: can't create detector pipe\n");
		return -1;
	}

	switch (overlapped_connect_pipe(p->pipe, &p->ov)) {
	case 0:
		p->status = UNCONNECTED;
		break;

	case 1:
		p->status = NOT_READY;
		break;

	default:
		/* error already printed by overlapped_connect_pipe() */
		return -1;
	}

	return 0;
}

static int init_osc_server()
{
	if (!(events[OSC_EVENT] = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		fprintf(stderr, "supervisor: couldn't allocate osc event\n");
		return -1;
	}

	return 0;
}

static int init_events()
{
	if (init_osc_server() ||
		init_device_events() ||
		init_detector_pipe())
		return -1;

	return 0;
}

/*************************************************************************
 * windows bullshit
 *************************************************************************/

static int setup_reaper_job()
{
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
	memset(&jeli, '\0', sizeof(jeli));

	if( !(state.reaper_job = CreateJobObject(NULL, NULL)) )
		return -1;

	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if( !SetInformationJobObject(
			state.reaper_job, JobObjectExtendedLimitInformation, &jeli,
			sizeof(jeli)) )
		return -1;

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

/*************************************************************************
 * entry point from detector
 *************************************************************************/

int sosc_supervisor_run(char *progname)
{
	if (init_events())
		goto err_ev_init;

	if (!(state.exec_path = get_service_binpath()))
		goto err_get_binpath;

	if (!(state.quoted_exec_path = s_asprintf("\"%s\"", state.exec_path))) {
		fprintf(stderr, "sosc_supervisor_run() error: couldn't allocate memory\n");
		goto err_asprintf;
	}

	if (setup_reaper_job())
		goto err_reaper;

	read_loop();

	return 0;

err_reaper:
err_asprintf:
	s_free(state.exec_path);
err_get_binpath:
err_ev_init:
	return -1;
}
