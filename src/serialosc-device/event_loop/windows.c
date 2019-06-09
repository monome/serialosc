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

#include <winsock2.h>
#include <windows.h>
#include <io.h>

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>

#ifdef _LP64
#define PRIdword  "d"
#define PRIudword "u"
#else
#define PRIdword  "ld"
#define PRIudword "lu"
#endif

static int
recv_msg(struct sosc_state *state, int ipc_fd)
{
	struct sosc_ipc_msg msg;

	if (sosc_ipc_msg_read(ipc_fd, &msg) <= 0)
		return 0;

	switch (msg.type) {
	case SOSC_PROCESS_SHOULD_EXIT:
		state->running = 0;
		return 0;

	default:
		return -1;
	}
}

struct poll_thread_ctx {
	struct sosc_state *state;
	HANDLE wakeup_handle;
};

static DWORD WINAPI
stdin_poll_thread(LPVOID _ctx)
{
	struct poll_thread_ctx *ctx = _ctx;

	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_PROCESSED_INPUT);

	for (;;) {
		recv_msg(ctx->state, STDIN_FILENO);
		SetEvent(ctx->wakeup_handle);
	}

	return 0;
}

int
sosc_event_loop(struct sosc_state *state)
{
	OVERLAPPED ov = {0, 0, {{0, 0}}};
	HANDLE hres, wait_handles[3];
	WSANETWORKEVENTS network_events;
	struct poll_thread_ctx ctx;
	DWORD evt_mask;
	ssize_t nbytes;
	int status;

	wait_handles[0] = ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	wait_handles[1] = WSACreateEvent();
	WSAEventSelect(lo_server_get_socket_fd(state->server),
			wait_handles[1], FD_READ);

	wait_handles[2] = CreateEvent(NULL, TRUE, FALSE, NULL);

	state->running = 1;

	if (state->ipc_in_fd > -1 || 1) {
		ctx.state = state;
		ctx.wakeup_handle = wait_handles[2];

		CreateThread(NULL, 0, stdin_poll_thread, &ctx, 0, NULL);
	}

	hres = (HANDLE) _get_osfhandle(monome_get_fd(state->monome));

	while (state->running) {
		SetCommMask(hres, EV_RXCHAR);

		if (!WaitCommEvent(hres, &evt_mask, &ov))
			switch (GetLastError()) {
			case ERROR_IO_PENDING:
				break;

			case ERROR_ACCESS_DENIED:
				/* evidently we get this when the monome is unplugged? */
				return 1;

			default:
				fprintf(stderr, "event_loop() error: %"PRIdword"\n",
						GetLastError());
				return 1;
			}

		switch (WaitForMultipleObjects(3, wait_handles, FALSE, INFINITE)) {
		case WAIT_OBJECT_0:
			do {
				status = monome_event_handle_next(state->monome);

				if (status < 0) {
					return 1;
				}
			} while (status > 0);

			break;

		case WAIT_OBJECT_0 + 1:
			WSAEnumNetworkEvents(lo_server_get_socket_fd(state->server),
				wait_handles[1],
				&network_events);
			do {
				nbytes = lo_server_recv_noblock(state->server, 0);
			} while (nbytes > 0);

			break;

		case WAIT_OBJECT_0 + 2:
			ResetEvent(wait_handles[2]);
			break;

		case WAIT_TIMEOUT:
			break;

		case WAIT_ABANDONED_0:
		case WAIT_FAILED:
			fprintf(stderr, "event_loop(): wait failed: %"PRIdword"\n",
			        GetLastError());
			return 1;
		}
	}

	return 0;
}
