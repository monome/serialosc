/**
 * Copyright (c) 2011 William Light <wrl@illest.net>
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

#include <assert.h>
#include <stdio.h>

#include <windows.h>
#include <process.h>
#include <Winreg.h>

#include "serialosc.h"


#define FTDI_REG_PATH "SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS"
#define SERVICE_NAME "serialosc"

typedef struct {
	SERVICE_STATUS svc_status;
	SERVICE_STATUS_HANDLE hstatus;

	char *exec_path;
	const char *quoted_exec_path;
} detector_state_t;

detector_state_t state = {
	.svc_status = {
		.dwServiceType = SERVICE_WIN32,
		.dwCurrentState = SERVICE_START_PENDING,
		.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN,
		.dwWin32ExitCode = 0,
		.dwServiceSpecificExitCode = 0,
		.dwCheckPoint = 0,
		.dwWaitHint = 0
	}
};

void debug(char *fmt, ...) {
	va_list ap;
	FILE *log = fopen("C:/serialosc.txt", "a+");
	if( !log )
		return;

	va_start(ap, fmt);
	vfprintf(log, fmt, ap);
	va_end(ap);

	fclose(log);
}

static int spawn_server(detector_state_t *state, const char *devnode) {
	if( _spawnlp(_P_NOWAIT, state->exec_path, state->quoted_exec_path,
	             devnode, NULL) < 0 ) {
		perror("dang");
		return 1;
	}

	return 0;
}

int scan_connected_devices(detector_state_t *state) {
	HKEY key, subkey;
	char subkey_name[MAX_PATH], *subkey_path, *serial;
	unsigned char port_name[64];
	DWORD klen, plen, ptype;
	int i = 0;

	serial = NULL;

	switch( RegOpenKeyEx(
			HKEY_LOCAL_MACHINE, FTDI_REG_PATH,
			0, KEY_READ, &key) ) {
	case ERROR_SUCCESS:
		/* ERROR: request was (unexpectedly) successful */
		break;

	case ERROR_FILE_NOT_FOUND:
		/* print message about needing the FTDI driver maybe? */
		/* fall through also */
	default:
		return 1;
	}

	do {
		klen = sizeof(subkey_name) / sizeof(char);
		switch( RegEnumKeyEx(key, i++, subkey_name, &klen,
							 NULL, NULL, NULL, NULL) ) {
		case ERROR_MORE_DATA:
		case ERROR_SUCCESS:
			break;

		default:
			goto done;
		}

		subkey_path = s_asprintf("%s\\%s\\0000\\Device Parameters",
								 FTDI_REG_PATH, subkey_name);

		switch( RegOpenKeyEx(
				HKEY_LOCAL_MACHINE, subkey_path,
				0, KEY_READ, &subkey) ) {
		case ERROR_SUCCESS:
			break;

		default:
			continue;
		}

		free(subkey_path);

		plen = sizeof(port_name) / sizeof(char);
		ptype = REG_SZ;
		switch( RegQueryValueEx(subkey, "PortName", 0, &ptype,
								port_name, &plen) ) {
		case ERROR_SUCCESS:
			port_name[plen] = '\0';
			break;

		default:
			goto next;
		}

		spawn_server(state, (char *) port_name);

next:
		RegCloseKey(subkey);
	} while( 1 );

done:
	RegCloseKey(key);
	return 0;
}

char *get_service_binpath() {
	SC_HANDLE manager, service;
	LPQUERY_SERVICE_CONFIG config;
	DWORD config_size;
	char *bin_path;

	if( !(manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE)) )
		goto err_manager;

	if( !(service = OpenService(manager, SERVICE_NAME, SERVICE_QUERY_CONFIG)) )
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

DWORD WINAPI control_handler(DWORD ctrl, DWORD type, LPVOID data, LPVOID ctx) {
	switch( ctrl ) {
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		state.svc_status.dwWin32ExitCode = 0;
		state.svc_status.dwCurrentState  = SERVICE_STOPPED;
		SetServiceStatus(state.hstatus, &state.svc_status);
		return NO_ERROR;

	case SERVICE_CONTROL_INTERROGATE:
		SetServiceStatus(state.hstatus, &state.svc_status);
		return NO_ERROR;

	default:
		break;
	}

	return ERROR_CALL_NOT_IMPLEMENTED;
}

void WINAPI service_main(DWORD argc, LPTSTR *argv) {
	state.hstatus = RegisterServiceCtrlHandlerEx(
		SERVICE_NAME, control_handler, NULL);

	if( !state.hstatus )
		return;

	state.svc_status.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(state.hstatus, &state.svc_status);

	if( !(state.exec_path = get_service_binpath()) )
		goto err;

	if( !(state.quoted_exec_path = s_asprintf("\"%s\"", state.exec_path)) ) {
		fprintf(stderr, "detector_run() error: couldn't allocate memory\n");
		goto err_asprintf;
	}

	scan_connected_devices(&state);
	return;

err_asprintf:
	s_free(state.exec_path);
err:
	state.svc_status.dwCurrentState = SERVICE_STOPPED;
	state.svc_status.dwWin32ExitCode = 1;
	SetServiceStatus(state.hstatus, &state.svc_status);
	return;
}

int detector_run(const char *exec_path) {
	SERVICE_TABLE_ENTRY services[] = {
		{SERVICE_NAME, service_main},
		{NULL, NULL}
	};

	/* your APIs suck, fuck you. */
	StartServiceCtrlDispatcher(services);

	return 0;
}
