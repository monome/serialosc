/**
 * Copyright (c) 2015 William Light <wrl@illest.net>
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
#include <serialosc/serialosc.h>
#include <windows.h>

static SERVICE_STATUS svc_status;
static SERVICE_STATUS_HANDLE hstatus;

static int s_argc;
static char **s_argv;

extern int supervisor_main(int argc, char **argv);

static DWORD WINAPI
supervisor_thread(LPVOID param)
{
	supervisor_main(s_argc, s_argv);
	return 0;
}

static DWORD WINAPI
control_handler(DWORD ctrl, DWORD type, LPVOID data, LPVOID ctx)
{
	switch (ctrl) {
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		svc_status.dwWin32ExitCode = 0;
		svc_status.dwCurrentState  = SERVICE_STOPPED;
		break;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		return ERROR_CALL_NOT_IMPLEMENTED;
	}

	SetServiceStatus(hstatus, &svc_status);
	return NO_ERROR;
}

static void WINAPI
service_main(DWORD argc, LPTSTR *argv)
{
	svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	svc_status.dwCurrentState = SERVICE_START_PENDING;
	svc_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	hstatus = RegisterServiceCtrlHandlerEx(
		SOSC_WIN_SERVICE_NAME, control_handler, NULL);

	if (!hstatus)
		return;

	svc_status.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(hstatus, &svc_status);

	CreateThread(NULL, 0, supervisor_thread, NULL, 0, NULL);
	return;
}

int
main(int argc, char **argv)
{
	SERVICE_TABLE_ENTRY services[] = {
		{SOSC_WIN_SERVICE_NAME, service_main},
		{NULL, NULL}
	};

	s_argc = argc;
	s_argv = argv;

	if (!StartServiceCtrlDispatcher(services)) {
		switch (GetLastError()) {
		case ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
			fprintf(stderr, " [!] running as a non-service\n");
			supervisor_main(argc, argv);
			break;

		default:
			break;
		}
	}

	return 0;
}
