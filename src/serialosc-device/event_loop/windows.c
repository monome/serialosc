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
#include <stdbool.h>

#include <winsock2.h>
#include <windows.h>
#include <io.h>

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>

static int
recv_ipc_msg(struct sosc_state *state, int ipc_fd)
{
	struct sosc_ipc_msg msg;

	if (sosc_ipc_msg_read(ipc_fd, &msg) <= 0) {
		return 0;
	}

	switch (msg.type) {
	case SOSC_PROCESS_SHOULD_EXIT:
		state->running = 0;
		return 0;

	default:
		return -1;
	}
}

int
wait_for_serial_input(HANDLE hres, LPOVERLAPPED ov, DWORD timeout)
{
	DWORD event_mask;
	int result = 0;

	SetCommMask(hres, EV_RXCHAR);

	if (!WaitCommEvent(hres, &event_mask, ov)) {
		if (GetLastError() == ERROR_IO_PENDING) {
			switch (WaitForSingleObject(ov->hEvent, timeout)) {
			case WAIT_OBJECT_0:
				result = 0;
				break;
			case WAIT_TIMEOUT:
				result = 1;
				break;
			default:
				result = -1;
				break;
			}
		} else {
			result = -1;
		}
	} else {
		result = 0;
	}

	return result;
}

static DWORD WINAPI
stdin_poll_thread(LPVOID arg)
{
	struct sosc_state *state = arg;

	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_PROCESSED_INPUT);

	while (state->running) {
		recv_ipc_msg(state, STDIN_FILENO);
	}

	return 0;
}

static DWORD WINAPI
serial_poll_thread(LPVOID arg)
{
	struct sosc_state *state = arg;

	OVERLAPPED ov = {0, 0, {{0, 0}}};
	ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	int status;

	HANDLE monome_handle = (HANDLE) _get_osfhandle(monome_get_fd(state->monome));

	while (state->running) {
		if (wait_for_serial_input(monome_handle, &ov, INFINITE) == 0) {
			do {
				status = monome_event_handle_next(state->monome);
			} while (status > 0);

			if (status < 0) {
				goto err;
			}
		} else {
			goto err;
		}
	}

err:
	CloseHandle(ov.hEvent);
	return 0;
}

static DWORD WINAPI
osc_poll_thread(LPVOID arg)
{
	struct sosc_state *state = arg;

	while (state->running) {
		lo_server_recv(state->server);
	}

	return 0;
}

int
sosc_event_loop(struct sosc_state *state)
{
	HANDLE threads[3];

	state->running = true;

	threads[0] = CreateThread(NULL, 0, serial_poll_thread, state, 0, NULL);
	threads[1] = CreateThread(NULL, 0, osc_poll_thread, state, 0, NULL);

	if (state->ipc_in_fd > -1) {
		threads[2] = CreateThread(NULL, 0, stdin_poll_thread, state, 0, NULL);
	} else {
		// wait for a dummy event if no ipc
		threads[2] = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	WaitForMultipleObjects(3, threads, FALSE, INFINITE);

	return 0;
}
