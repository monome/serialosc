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
#include <Winsock.h>
#include <io.h>

#include "serialosc.h"

static DWORD WINAPI lo_thread(LPVOID param) {
	sosc_state_t *state = param;

	while( 1 )
		lo_server_recv(state->server);

	return 0;
}

int event_loop(const sosc_state_t *state) {
	HANDLE srl_res, lo_thd_res;
	DWORD evt_mask;

	srl_res = (HANDLE) _get_osfhandle(monome_get_fd(state->monome));
	lo_thd_res = CreateThread(NULL, 0, lo_thread, (void *) state, 0, NULL);

	do {
		SetCommMask(srl_res, EV_RXCHAR | EV_RLSD);

		if( !WaitCommEvent(srl_res, &evt_mask, NULL) ) {
			if( GetLastError() == ERROR_OPERATION_ABORTED )
				/* monome was unplugged */
				break;

			printf("event_loop() error: %d\n", GetLastError());
			break;
		}

		switch( evt_mask ) {
		case EV_RXCHAR:
			monome_event_handle_next(state->monome);
			break;

		default:
			break;
		}
	} while ( 1 );

	return 0;
}
