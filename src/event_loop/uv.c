/**
 * Copyright (c) 2010-2015 William Light <wrl@illest.net>
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
#include <uv.h>

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>

#define SELF_FROM(p, member) struct sosc_uv_poll *self = container_of(p,	\
		struct sosc_uv_poll, member)

struct sosc_uv_poll {
	uv_loop_t *loop;
	uv_poll_t monome_poll, osc_poll, supervisor_pipe_poll;
	const struct sosc_state *state;
};

static void
monome_poll_cb(uv_poll_t *handle, int status, int events)
{
	SELF_FROM(handle, monome_poll);

	if (status) {
		uv_stop(self->loop);
		return;
	}

	while (monome_event_handle_next(self->state->monome));
}

static void
osc_poll_cb(uv_poll_t *handle, int status, int events)
{
	SELF_FROM(handle, osc_poll);
	lo_server_recv_noblock(self->state->server, 0);
}

static int
handle_supervisor_message(struct sosc_uv_poll *self, struct sosc_ipc_msg *msg)
{
	switch (msg->type) {
	case SOSC_PROCESS_SHOULD_EXIT:
		uv_stop(self->loop);
		return 0;

	default:
		return -1;
	}
}

static void
supervisor_pipe_poll_cb(uv_poll_t *handle, int status, int events)
{
	SELF_FROM(handle, supervisor_pipe_poll);
	struct sosc_ipc_msg msg;

	if (sosc_ipc_msg_read(self->state->ipc_in_fd, &msg) > 0)
		handle_supervisor_message(self, &msg);
}

int
sosc_event_loop(const struct sosc_state *state)
{
	struct sosc_uv_poll self;

	self.state = state;
	self.loop = uv_default_loop();

	uv_poll_init(self.loop, &self.monome_poll, monome_get_fd(state->monome));
	uv_poll_init_socket(self.loop, &self.osc_poll,
			lo_server_get_socket_fd(state->server));

	uv_poll_start(&self.monome_poll, UV_READABLE, monome_poll_cb);
	uv_poll_start(&self.osc_poll, UV_READABLE, osc_poll_cb);

	if (state->ipc_in_fd > -1) {
		uv_poll_init(self.loop, &self.supervisor_pipe_poll, state->ipc_in_fd);
		uv_poll_start(&self.supervisor_pipe_poll, UV_READABLE,
				supervisor_pipe_poll_cb);
	}

	uv_run(self.loop, UV_RUN_DEFAULT);

	if (state->ipc_in_fd > -1)
		uv_close((void *) &self.supervisor_pipe_poll, NULL);

	uv_close((void *) &self.osc_poll, NULL);
	uv_close((void *) &self.monome_poll, NULL);

	/* run once more to make sure libuv cleans up any internal resources. */
	uv_run(self.loop, UV_RUN_NOWAIT);

	uv_loop_close(self.loop);
	return 0;
}
