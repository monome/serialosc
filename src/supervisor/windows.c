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

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>
#include <serialosc/osc.h>

/* damnit mingw */
#ifndef JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
#define JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE 0x00002000
#endif

#define ARRAY_LENGTH(x)  (sizeof(x) / sizeof(*x))

#define PIPE_BUF         128
#define SOSC_DEVICE_PIPE (SOSC_PIPE_PREFIX "devices")

#define MAX_NOTIFICATION_ENDPOINTS 32

#define MAX_DEVICES      32
#define DETECTOR_EVENT   (MAX_DEVICES)
#define OSC_EVENT        (MAX_DEVICES + 1)
#define EVENT_COUNT      (MAX_DEVICES + 2) /* 1 for osc, 1 for detector */

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

	lo_server *srv;

	char *exec_path;
	const char *quoted_exec_path;
};

struct notification_endpoint {
	char host[256];
	char port[6];
};

struct notifications {
	struct notification_endpoint endpoint[MAX_NOTIFICATION_ENDPOINTS];
	int count;

	int notified;
};

static struct supervisor_state state = {
	.detector = {
		.ov = {0, 0, {{0, 0}}},
	}
};

/*************************************************************************
 * overlapped i/o helpers
 *************************************************************************/

static int
overlapped_connect_pipe(HANDLE p, OVERLAPPED *ov)
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

static HANDLE
open_pipe_to_supervisor(void)
{
	SECURITY_ATTRIBUTES sattr;
	HANDLE p = NULL;
	DWORD pipe_state;

	sattr.nLength = sizeof(SECURITY_ATTRIBUTES);
	sattr.bInheritHandle = TRUE;
	sattr.lpSecurityDescriptor = NULL;

	for (;;) {
		p = CreateFile(
			SOSC_DEVICE_PIPE,
			GENERIC_WRITE,
			0,
			&sattr,
			OPEN_EXISTING,
			0,
			NULL);

		if (p != INVALID_HANDLE_VALUE)
			break;
			
		switch (GetLastError()) {
		case ERROR_FILE_NOT_FOUND:
			Sleep(100);
			continue;

		default:
			return INVALID_HANDLE_VALUE;
		}
	}

	pipe_state = PIPE_READMODE_MESSAGE;
	SetNamedPipeHandleState(p, &pipe_state, NULL, NULL);

	return p;
}

static int
spawn_server(const char *devnode)
{
	STARTUPINFO sinfo = {sizeof(sinfo)};
	PROCESS_INFORMATION pinfo;
	HANDLE pipe_to_supervisor;
	char *cmdline;

	pipe_to_supervisor = open_pipe_to_supervisor();
	if (pipe_to_supervisor == INVALID_HANDLE_VALUE) {
		fprintf(stderr,
			"supervisor: spawn_server(): couldn't open supervisor pipe\n");
		return -1;
	}

	if (!(cmdline = s_asprintf("%s %s", state.quoted_exec_path, devnode))) {
		fprintf(stderr, "supervisor: spawn_server(): asprintf failed\n");
		return -1;
	}

	sinfo.dwFlags    = STARTF_USESTDHANDLES;
	sinfo.hStdInput  = INVALID_HANDLE_VALUE;
	sinfo.hStdError  = INVALID_HANDLE_VALUE;
	sinfo.hStdOutput = pipe_to_supervisor;

	if (!CreateProcess(
			state.exec_path,
			cmdline,
			NULL,
			NULL,
			TRUE,
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			&sinfo,
			&pinfo)) {
		s_free(cmdline);
		fprintf(stderr, "supervisor: spawn_server(): CreateProcess failed (%ld)\n",
				GetLastError());
		return -1;
	}

	s_free(cmdline);

	AssignProcessToJobObject(state.reaper_job, pinfo.hProcess);

	CloseHandle(pinfo.hProcess);
	CloseHandle(pinfo.hThread);
	CloseHandle(pipe_to_supervisor);

	return 0;
}

/*************************************************************************
 * the datastore
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

struct notifications notifications = {
	.count    = 0,
	.notified = 0,
};

/*************************************************************************
 * osc stuff
 *************************************************************************/

static int
portstr(char *dest, int src)
{
	return snprintf(dest, 6, "%d", src);
}

OSC_HANDLER_FUNC(list_devices)
{
	struct incoming_pipe *p;
	lo_address *dst;
	char port[6];
	int i;

	portstr(port, argv[1]->i);

	if (!(dst = lo_address_new(&argv[0]->s, port))) {
		fprintf(stderr, "list_devices(): error in lo_address_new()\n");
		return 1;
	}

	fprintf(stderr, "%s:%s\n", &argv[0]->s, port);

	for (i = 0; i < MAX_DEVICES; i++) {
		p = &pipe_info[i];

		if (p->status != READY)
			continue;

		lo_send_from(dst, state.srv, LO_TT_IMMEDIATE,
					 "/serialosc/device", "ssi",
					 p->info.serial,
					 p->info.friendly,
					 p->info.port);
	}

	lo_address_free(dst);

	return 0;
}

OSC_HANDLER_FUNC(add_notification_endpoint)
{
	struct notification_endpoint *n;

	if (notifications.count >= MAX_NOTIFICATION_ENDPOINTS)
		return 1;

	n = &notifications.endpoint[notifications.count];

	portstr(n->port, argv[1]->i);
	strncpy(n->host, &argv[0]->s, sizeof(n->host));
	n->host[sizeof(n->host) - 1] = '\0';

	notifications.count++;
	return 0;
}

static void
reset_notifications(void)
{
	if (notifications.notified)
		notifications.notified = 
			notifications.count = 0;
}

static void
osc_error(int num, const char *msg, const char *where)
{
	fprintf(stderr, "[!] osc server error %d, \"%s\" (%s)", num, msg, where);
}

static int
init_osc_server(void)
{
	if (!(state.srv = lo_server_new(SOSC_SUPERVISOR_OSC_PORT, osc_error))) {
		fprintf(stderr, "supervisor: couldn't create lo_server\n");
		goto err_server_new;
	}

	if (!(events[OSC_EVENT] = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		fprintf(stderr, "supervisor: couldn't allocate osc event\n");
		goto err_create_event;
	}

	lo_server_add_method(
		state.srv, "/serialosc/list", "si", list_devices, NULL);
	lo_server_add_method(
		state.srv, "/serialosc/notify", "si", add_notification_endpoint, NULL);

	return 0;

err_create_event:
		lo_server_free(state.srv);
err_server_new:
		return -1;
}

/*************************************************************************
 * supervisor read loop
 *************************************************************************/

static int
notify(struct incoming_pipe *p, sosc_ipc_type_t type)
{
	struct notification_endpoint *e;
	lo_address dst;
	char *path;
	int i;

	switch (type) {
	case SOSC_DEVICE_CONNECTION:
	case SOSC_DEVICE_READY:
		path = "/serialosc/add";
		break;

	case SOSC_DEVICE_DISCONNECTION:
		path = "/serialosc/remove";
		break;

	default:
		return 1;
	}

	for (i = 0; i < notifications.count; i++) {
		e = &notifications.endpoint[i];

		if (!(dst = lo_address_new(e->host, e->port))) {
			fprintf(stderr, "notify(): couldn't allocate lo_address\n");
			continue;
		}

		lo_send_from(dst, state.srv, LO_TT_IMMEDIATE, path, "ssi",
					 p->info.serial, p->info.friendly, p->info.port);

		lo_address_free(dst);
	}

	notifications.notified++;
	return 0;
}

static int
handle_disconnect(struct incoming_pipe *p)
{
	DisconnectNamedPipe(p->pipe);

	if (p->info.serial) {
		fprintf(stderr, "serialosc [%s]: disconnected, exiting\n",
				p->info.serial);

		s_free(p->info.serial);
	}

	if (p->info.friendly)
		s_free(p->info.friendly);

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

static int
handle_msg(struct incoming_pipe *p, sosc_ipc_msg_t *msg)
{
	switch (msg->type) {
	case SOSC_DEVICE_CONNECTION:
		spawn_server(msg->connection.devnode);
		break;

	case SOSC_DEVICE_INFO:
		p->info.serial = msg->device_info.serial;
		p->info.friendly = msg->device_info.friendly;
		break;

	case SOSC_DEVICE_READY:
		fprintf(stderr, "serialosc [%s]: connected, server running on port %d\n",
				p->info.serial, p->info.port);

		p->status = READY;
		notify(p, msg->type);
		break;

	case SOSC_DEVICE_DISCONNECTION:
		p->status = NOT_READY;
		notify(p, msg->type);
		break;

	case SOSC_OSC_PORT_CHANGE:
		p->info.port = msg->port_change.port;
		break;
	}

	return 0;
}

static int
handle_read(struct incoming_pipe *p)
{
	ssize_t bytes_left, bytes_handled;
	uint8_t *buf;
	sosc_ipc_msg_t *msg;

	buf = (uint8_t *) p->read.buf;
	bytes_left = p->read.nbytes;

	do {
		bytes_handled = sosc_ipc_msg_from_buf(buf, bytes_left, &msg);

		if (bytes_handled < 0) {
			fprintf(stderr, "[-] bad message\n");
			return -1;
		}

		handle_msg(p, msg);

		buf += bytes_handled;
		bytes_left -= bytes_handled;
	} while (bytes_left > 0);

	return 0;
}

static int
handle_pipe(struct incoming_pipe *p)
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
			handle_disconnect(p);
			return 0;
		}

		handle_read(p);

		break;
	}

queue_read:
	res = ReadFile(
		p->pipe,
		p->read.buf,
		sizeof(p->read.buf),
		&p->read.nbytes,
		&p->ov);

	if (res && p->read.nbytes > 0) {
		handle_read(p);
		goto queue_read;
	}

	if (!res) {
		switch ((res = GetLastError())) {
		case ERROR_IO_PENDING:
			return 0;

		case ERROR_BROKEN_PIPE:
			break;

		default:
			fprintf(stderr, "supervisor: handle_pipe(): error %ld\n", res);
			return -1;
		}
	}

	handle_disconnect(p);
	return 0;
}

static void
read_loop(void)
{
	SOCKET osc_sock = lo_server_get_socket_fd(state.srv);
	WSANETWORKEVENTS dont_care;
	int which;

	WSAEventSelect(osc_sock, events[OSC_EVENT], FD_READ);

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

		switch (which) {
		case DETECTOR_EVENT:
			if (handle_pipe(&state.detector))
				fprintf(stderr,
					"supervisor: read_loop(): failure in handle_pipe "
					"(detector pipe: %ld)\n", GetLastError());
			break;

		case OSC_EVENT:
			while (lo_server_recv_noblock(state.srv, 0));
			WSAEnumNetworkEvents(osc_sock, events[OSC_EVENT], &dont_care);
			break;

		default:
			if (handle_pipe(&pipe_info[which]))
				fprintf(stderr,
					"supervisor: read_loop(): failure in handle_pipe "
					"(device pipe %d: %ld)\n", which, GetLastError());
			break;
		}

		reset_notifications();
	}
}

/*************************************************************************
 * pipe initialization
 *************************************************************************/

static int
init_device_events(void)
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

static int
init_detector_pipe(void)
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

static int
init_events(void)
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

static int
setup_reaper_job(void)
{
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
	memset(&jeli, '\0', sizeof(jeli));

	if (!(state.reaper_job = CreateJobObject(NULL, NULL)))
		return -1;

	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if (!SetInformationJobObject(
			state.reaper_job, JobObjectExtendedLimitInformation, &jeli,
			sizeof(jeli)))
		return -1;

	return 0;
}

static char *
get_service_binpath(void)
{
	SC_HANDLE manager, service;
	LPQUERY_SERVICE_CONFIG config;
	DWORD config_size;
	char *bin_path;

	if (!(manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE)))
		goto err_manager;

	if (!(service = OpenService(manager,
					SOSC_WIN_SERVICE_NAME, SERVICE_QUERY_CONFIG)))
		goto err_service;

	QueryServiceConfig(service, NULL, 0, &config_size);

	if (!(config = s_malloc(config_size)))
		goto err_malloc;

	if (!QueryServiceConfig(service, config, config_size, &config_size))
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

int
sosc_supervisor_run(char *progname)
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
