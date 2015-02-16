/**
 * Copyright (c) 2010-2011 William Light <wrl@illest.net>
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
#include <errno.h>
#include <poll.h>

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>

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

int
sosc_event_loop(struct sosc_state *state)
{
	struct pollfd fds[3];
	int nfds;

	fds[0].fd = monome_get_fd(state->monome);
	fds[0].events = POLLIN;

	fds[1].fd = lo_server_get_socket_fd(state->server);
	fds[1].events = POLLIN;

	fds[2].fd = state->ipc_in_fd;
	fds[2].events = POLLIN;
	fds[2].revents = 0;

	nfds = (state->ipc_in_fd > -1) ? 3 : 2;

	for (state->running = 1; state->running;) {
		/* block until either the monome or liblo have data */
		if (poll(fds, nfds, -1) < 0)
			switch (errno) {
			case EINVAL:
				perror("error in poll()");
				return 1;

			case EINTR:
			case EAGAIN:
				continue;
			}

		/* is the monome still connected? */
		if (fds[0].revents & (POLLHUP | POLLERR))
			break;

		/* is there data available for reading from the monome? */
		if (fds[0].revents & POLLIN)
			monome_event_handle_next(state->monome);

		/* how about from OSC? */
		if (fds[1].revents & POLLIN)
			lo_server_recv_noblock(state->server, 0);

		/* how about from the supervisor? */
		if (fds[2].revents & POLLIN)
			recv_msg(state, state->ipc_in_fd);
	}

	return 1;
}
