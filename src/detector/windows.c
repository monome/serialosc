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

#include <stdio.h>

#include <windows.h>
#include <process.h>
#include <Winreg.h>

#include "serialosc.h"


#define FTDI_REG_PATH "SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS"

typedef struct {
	const char *exec_path;
} detector_state_t;

static int spawn_server(const char *exec_path, const char *devnode) {
	if( _spawnlp(_P_NOWAIT, exec_path, exec_path, devnode, NULL) < 0 ) {
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

		spawn_server(state->exec_path, (char *) port_name);

next:
		RegCloseKey(subkey);
	} while( 1 );

done:
	RegCloseKey(key);
	return 0;
}

int detector_run(const char *exec_path) {
	MSG msg;
	detector_state_t state = {
		.exec_path = exec_path
	};

	scan_connected_devices(&state);

	do {
		if( GetMessage(&msg, NULL, 0, 0) < 0 ) {
			printf("detector_run() error: %ld\n", GetLastError());
			return 0;
		}

		puts("message");
	} while( 1 );

	return 0;
}
