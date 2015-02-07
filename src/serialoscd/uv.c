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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <uv.h>
#include <wwrl/vector_stdlib.h>

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>
#include <serialosc/osc.h>

#define SELF_FROM(p, member) struct sosc_supervisor *self = container_of(p,	\
		struct sosc_supervisor, member)

#define DEV_FROM(p, member) struct sosc_device_subprocess *dev = \
	container_of(p, struct sosc_device_subprocess, member);

/*************************************************************************
 * datastructures and utilities
 *************************************************************************/

/* we set the `data` member (which is a void*) of the libuv
 * sosc_subprocess.proc structure to either of these values to indicate
 * what kind of subprocess it is. this is so that we can send responses to
 * a /serialosc/list by doing a uv_walk() on our run loop. */
int detector_type = 0;
int device_type = 0;

typedef enum {
	SERIALOSC_DISABLED = 0,
	SERIALOSC_ENABLED  = 1
} sosc_supervisor_state_t;

struct sosc_subprocess {
	uv_process_t proc;
	int incoming_pipe_fd;
	int outgoing_pipe_fd;
	uv_poll_t poll;
};

struct sosc_notification_endpoint {
	char host[128];
	char port[6];
};

struct sosc_supervisor {
	uv_loop_t *loop;
	sosc_supervisor_state_t state;

	struct sosc_subprocess detector;

	struct {
		uv_idle_t check;
		int pending;
	} state_change;

	struct {
		lo_server *server;
		uv_poll_t poll;
	} osc;

	VECTOR(sosc_notifications, struct sosc_notification_endpoint)
		notifications;

	uv_check_t drain_notifications;
};

struct sosc_device_subprocess {
	struct sosc_subprocess subprocess;
	struct sosc_supervisor *supervisor;

	int ready;
	int port;

	char *serial;
	char *friendly;
};

static int
launch_subprocess(struct sosc_supervisor *self, struct sosc_subprocess *proc,
		uv_exit_cb exit_cb, char *arg)
{
	struct uv_process_options_s options;
	char path_buf[1024];
	int pipefds[2][2], err;
	size_t len;

	len = sizeof(path_buf);
	err = uv_exepath(path_buf, &len);
	if (err < 0) {
		fprintf(stderr, "launch_subprocess() failed in uv_exepath(): %s\n",
				uv_strerror(err));

		goto err_exepath;
	}

	err = pipe(pipefds[0]);
	if (err < 0) {
		perror("launch_subprocess() failed in pipe()");
		goto err_pipe0;
	}

	err = pipe(pipefds[1]);
	if (err < 0) {
		perror("launch_subprocess() failed in pipe()");
		goto err_pipe1;
	}

	options = (struct uv_process_options_s) {
		.exit_cb = exit_cb,

		.file    = path_buf,
		.args    = (char *[]) {path_buf, arg, NULL},
		.flags   = UV_PROCESS_WINDOWS_HIDE,

		.stdio_count = 2,
		.stdio = (struct uv_stdio_container_s []) {
			[STDIN_FILENO] = {
				.flags   = UV_INHERIT_FD,
				.data.fd = pipefds[0][0]
			},

			[STDOUT_FILENO] = {
				.flags   = UV_INHERIT_FD,
				.data.fd = pipefds[1][1]
			}
		},
	};

	err = uv_spawn(self->loop, &proc->proc, &options);
	close(pipefds[0][0]);
	close(pipefds[1][1]);

	if (err < 0) {
		fprintf(stderr, "launch_subprocess() failed in uv_spawn(): %s\n",
				uv_strerror(err));
		goto err_spawn;
	}

	proc->outgoing_pipe_fd = pipefds[0][1];
	proc->incoming_pipe_fd = pipefds[1][0];
	uv_poll_init(self->loop, &proc->poll, proc->incoming_pipe_fd);
	return 0;

err_spawn:
	close(pipefds[1][0]);
	close(pipefds[1][1]);
err_pipe1:
	close(pipefds[0][0]);
	close(pipefds[0][1]);
err_pipe0:
err_exepath:
	return err;
}

/*************************************************************************
 * supervisor state changes
 *************************************************************************/

static void detector_exit_cb(uv_process_t *, int64_t, int);
static void detector_pipe_cb(uv_poll_t *, int, int);

static void
remaining_subprocesses_walk_cb(uv_handle_t *handle, void *args)
{
	int *subprocs_remaining = args;

	if (handle->data == &device_type || handle->data == &detector_type)
		(*subprocs_remaining)++;
}

static void
state_change_check_cb(uv_idle_t *handle)
{
	SELF_FROM(handle, state_change.check);
	int subprocs_remaining;

	switch (self->state_change.pending) {
	case SERIALOSC_ENABLED:
		/* nothing async to check, would have caught any issues in
		 * supervisor__change_state(). only doing this async for the shared
		 * code path with SERIALOSC_DISABLED. */
		goto done;

	case SERIALOSC_DISABLED:
		subprocs_remaining = 0;
		uv_walk(self->loop, remaining_subprocesses_walk_cb,
				&subprocs_remaining);

		/* only finish the transition into SERIALOSC_DISABLED if all of
		 * our subprocesses have terminated. */
		if (!subprocs_remaining)
			goto done;
		break;
	}

	return;

done:
	self->state = self->state_change.pending;
	uv_idle_stop(handle);
}

static void
kill_devices_walk_cb(uv_handle_t *handle, void *_args)
{
	struct sosc_device_subprocess *dev;
	struct sosc_ipc_msg msg = {
		.type = SOSC_PROCESS_SHOULD_EXIT
	};

	if (handle->data != &device_type)
		return;

	dev = container_of(handle, struct sosc_device_subprocess, subprocess.proc);
	sosc_ipc_msg_write(dev->subprocess.outgoing_pipe_fd, &msg);
}

static int
supervisor__change_state(struct sosc_supervisor *self,
		sosc_supervisor_state_t new_state)
{
	if (self->state == new_state
			|| uv_is_active((void *) &self->state_change.check))
		return -1;

	switch (new_state) {
	case SERIALOSC_ENABLED:
		if (launch_subprocess(self, &self->detector, detector_exit_cb, "-d"))
			return -1;

		self->detector.proc.data = &detector_type;
		uv_poll_start(&self->detector.poll, UV_READABLE, detector_pipe_cb);
		break;

	case SERIALOSC_DISABLED:
		uv_process_kill(&self->detector.proc, SIGTERM);
		uv_walk(self->loop, kill_devices_walk_cb, NULL);
		break;

	default:
		return -1;
	}

	/* we do this async check thing to prevent race conditions in which
	 * a supervisor_enable() happens before we've finished tearing down
	 * from a recent supervisor_disable(). */
	self->state_change.pending = new_state;
	uv_idle_start(&self->state_change.check, state_change_check_cb);
	return 0;
}

static int
supervisor_enable(struct sosc_supervisor *self)
{
	return supervisor__change_state(self, SERIALOSC_ENABLED);
}

static int
supervisor_disable(struct sosc_supervisor *self)
{
	return supervisor__change_state(self, SERIALOSC_DISABLED);
}

/*************************************************************************
 * osc
 *************************************************************************/

static int
portstr(char *dest, int src)
{
	return snprintf(dest, 6, "%d", src);
}

struct walk_cb_args {
	struct sosc_supervisor *self;
	lo_address *dst;
};

static void
list_devices_walk_cb(uv_handle_t *handle, void *_args)
{
	struct walk_cb_args *args = _args;
	struct sosc_supervisor *self = args->self;
	lo_address *dst = args->dst;
	struct sosc_device_subprocess *dev;

	if (handle->data != &device_type)
		return;

	dev = container_of(handle, struct sosc_device_subprocess, subprocess.proc);

	lo_send_from(dst, self->osc.server, LO_TT_IMMEDIATE, "/serialosc/device",
			"ssi", dev->serial, dev->friendly, dev->port);
}

OSC_HANDLER_FUNC(osc_list_devices)
{
	struct sosc_supervisor *self = user_data;
	struct walk_cb_args args;
	char port[6];

	portstr(port, argv[1]->i);

	args.self = self;
	args.dst  = lo_address_new(&argv[0]->s, port);

	if (!args.dst)
		return 1;

	uv_walk(self->loop, list_devices_walk_cb, &args);

	lo_address_free(args.dst);
	return 0;
}

OSC_HANDLER_FUNC(osc_add_notification_endpoint)
{
	struct sosc_supervisor *self = user_data;
	struct sosc_notification_endpoint n;

	portstr(n.port, argv[1]->i);
	sosc_strlcpy(n.host, &argv[0]->s, sizeof(n.host));

	VECTOR_PUSH_BACK(&self->notifications, n);
	return 0;
}

OSC_HANDLER_FUNC(osc_handle_enable)
{
	struct sosc_supervisor *self = user_data;
	supervisor_enable(self);
	return 0;
}

OSC_HANDLER_FUNC(osc_handle_disable)
{
	struct sosc_supervisor *self = user_data;
	supervisor_disable(self);
	return 0;
}

OSC_HANDLER_FUNC(osc_report_status)
{
	struct sosc_supervisor *self = user_data;
	lo_address *dst;
	char port[6];

	portstr(port, argv[1]->i);
	if (!(dst = lo_address_new(&argv[0]->s, port)))
		return 1;

	lo_send_from(dst, self->osc.server, LO_TT_IMMEDIATE,
			"/serialosc/status", "i", self->state);

	lo_address_free(dst);
	return 0;
}

OSC_HANDLER_FUNC(osc_report_version)
{
	struct sosc_supervisor *self = user_data;
	lo_address *dst;
	char port[6];

	portstr(port, argv[1]->i);
	if (!(dst = lo_address_new(&argv[0]->s, port)))
		return 1;

	lo_send_from(dst, self->osc.server, LO_TT_IMMEDIATE,
			"/serialosc/version", "ss", VERSION, GIT_COMMIT);

	lo_address_free(dst);
	return 0;
}

static void
osc_poll_cb(uv_poll_t *handle, int status, int events)
{
	SELF_FROM(handle, osc.poll);
	lo_server_recv_noblock(self->osc.server, 0);
}

static int
init_osc_server(struct sosc_supervisor *self)
{
	if (!(self->osc.server = lo_server_new(SOSC_SUPERVISOR_OSC_PORT, NULL)))
		return -1;

	lo_server_add_method(self->osc.server,
			"/serialosc/list", "si", osc_list_devices, self);
	lo_server_add_method(self->osc.server,
			"/serialosc/notify", "si", osc_add_notification_endpoint, self);

	lo_server_add_method(self->osc.server,
			"/serialosc/enable", "", osc_handle_enable, self);
	lo_server_add_method(self->osc.server,
			"/serialosc/disable", "", osc_handle_disable, self);
	lo_server_add_method(self->osc.server,
			"/serialosc/status", "si", osc_report_status, self);

	lo_server_add_method(self->osc.server,
			"/serialosc/version", "si", osc_report_version, self);

	uv_poll_init_socket(self->loop, &self->osc.poll,
			lo_server_get_socket_fd(self->osc.server));

	return 0;
}

static void
drain_notifications_cb(uv_check_t *handle)
{
	SELF_FROM(handle, drain_notifications);

	VECTOR_CLEAR(&self->notifications);
	uv_check_stop(handle);
}

static int
osc_notify(struct sosc_supervisor *self, struct sosc_device_subprocess *dev,
		sosc_ipc_type_t type)
{
	struct sosc_notification_endpoint *n;
	const char *path;
	lo_address dst;
	int i;

	switch (type) {
	case SOSC_DEVICE_CONNECTION:
		path = "/serialosc/add";
		break;

	case SOSC_DEVICE_DISCONNECTION:
		path = "/serialosc/remove";
		break;

	default:
		return -1;
	}

	for (i = 0; i < self->notifications.size; i++) {
		n = &self->notifications.data[i];

		if (!(dst = lo_address_new(n->host, n->port))) {
			fprintf(stderr, "notify(): couldn't allocate lo_address\n");
			continue;
		}

		lo_send_from(dst, self->osc.server, LO_TT_IMMEDIATE, path, "ssi",
		             dev->serial, dev->friendly, dev->port);

		lo_address_free(dst);
	}

	uv_check_start(&self->drain_notifications, drain_notifications_cb);
	return 0;
}

/*************************************************************************
 * device lifecycle
 *************************************************************************/

static void device_exit_cb(uv_process_t *, int64_t, int);

static int
device_init(struct sosc_supervisor *self, struct sosc_device_subprocess *dev,
		char *devnode)
{
	if (launch_subprocess(self, &dev->subprocess, device_exit_cb, devnode))
		return -1;

	dev->supervisor = self;
	dev->subprocess.proc.data = &device_type;
	return 0;
}

static void
device_fini(struct sosc_device_subprocess *dev)
{
	s_free(dev->serial);
	s_free(dev->friendly);
}

static void
device_proc_close_cb(uv_handle_t *handle)
{
	DEV_FROM(handle, subprocess.proc);

	device_fini(dev);
	free(dev);
}

static void
device_poll_close_cb(uv_handle_t *handle)
{
	DEV_FROM(handle, subprocess.poll);

	close(dev->subprocess.incoming_pipe_fd);
	close(dev->subprocess.outgoing_pipe_fd);

	uv_close((void *) &dev->subprocess.proc, device_proc_close_cb);
}

static void
device_exit_cb(uv_process_t *process, int64_t exit_status, int term_signal)
{
	DEV_FROM(process, subprocess.proc);

	if (dev->ready) {
		fprintf(stderr, "serialosc [%s]: disconnected, exiting\n",
				dev->serial);

		osc_notify(dev->supervisor, dev, SOSC_DEVICE_DISCONNECTION);
	}

	uv_close((void *) &dev->subprocess.poll, device_poll_close_cb);
}

/*************************************************************************
 * device communication
 *************************************************************************/

static int
handle_device_msg(struct sosc_supervisor *self,
		struct sosc_device_subprocess *dev, struct sosc_ipc_msg *msg)
{
	switch (msg->type) {
	case SOSC_DEVICE_CONNECTION:
	case SOSC_PROCESS_SHOULD_EXIT:
		return -1;

	case SOSC_OSC_PORT_CHANGE:
		dev->port = msg->port_change.port;
		return 0;

	case SOSC_DEVICE_INFO:
		dev->serial   = msg->device_info.serial;
		dev->friendly = msg->device_info.friendly;
		return 0;

	case SOSC_DEVICE_READY:
		dev->ready = 1;

		osc_notify(self, dev, SOSC_DEVICE_CONNECTION);
		fprintf(stderr, "serialosc [%s]: connected, server running on port %d\n",
				dev->serial, dev->port);
		return 0;

	case SOSC_DEVICE_DISCONNECTION:
		return 0;
	}

	return 0;
}

static void
device_pipe_cb(uv_poll_t *handle, int status, int events)
{
	DEV_FROM(handle, subprocess.poll);
	struct sosc_ipc_msg msg;

	if (sosc_ipc_msg_read(dev->subprocess.incoming_pipe_fd, &msg) > 0)
		handle_device_msg(dev->supervisor, dev, &msg);
}

/*************************************************************************
 * detector communication
 *************************************************************************/

static void device_pipe_cb(uv_poll_t *, int status, int events);

static int
handle_connection(struct sosc_supervisor *self, struct sosc_ipc_msg *msg)
{
	struct sosc_device_subprocess *dev;

	if (!(dev = calloc(1, sizeof(*dev))))
		goto err_calloc;

	if (device_init(self, dev, msg->connection.devnode))
		goto err_init;

	uv_poll_start(&dev->subprocess.poll, UV_READABLE, device_pipe_cb);
	return 0;

err_init:
	free(dev);
err_calloc:
	return -1;
}

static int
handle_detector_msg(struct sosc_supervisor *self, struct sosc_ipc_msg *msg)
{
	int ret;

	switch (msg->type) {
	case SOSC_DEVICE_CONNECTION:
		ret = handle_connection(self, msg);
		s_free(msg->connection.devnode);
		return ret;

	case SOSC_OSC_PORT_CHANGE:
	case SOSC_DEVICE_INFO:
	case SOSC_DEVICE_READY:
	case SOSC_DEVICE_DISCONNECTION:
	case SOSC_PROCESS_SHOULD_EXIT:
		return -1;
	}

	return 0;
}

static void
detector_poll_close_cb(uv_handle_t *handle)
{
	SELF_FROM(handle, detector.poll);
	close(self->detector.incoming_pipe_fd);
	close(self->detector.outgoing_pipe_fd);

	uv_close((void *) &self->detector.proc, NULL);
}

static void
detector_exit_cb(uv_process_t *process, int64_t exit_status, int term_signal)
{
	SELF_FROM(process, detector.proc);

	uv_close((void *) &self->detector.poll, detector_poll_close_cb);
}

static void
detector_pipe_cb(uv_poll_t *handle, int status, int events)
{
	SELF_FROM(handle, detector.poll);
	struct sosc_ipc_msg msg;

	if (sosc_ipc_msg_read(self->detector.incoming_pipe_fd, &msg) > 0)
		handle_detector_msg(self, &msg);
}

/*************************************************************************
 * entry point
 *************************************************************************/

int
main(int argc, char **argv)
{
	struct sosc_supervisor self = {NULL};

	uv_setup_args(argc, argv);

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	sosc_config_create_directory();

	self.loop  = uv_default_loop();
	VECTOR_INIT(&self.notifications, 32);

	uv_idle_init(self.loop, &self.state_change.check);
	self.state = SERIALOSC_DISABLED;

	if (init_osc_server(&self))
		goto err_osc_server;

	if (supervisor_enable(&self))
		goto err_enable;

	uv_poll_start(&self.osc.poll, UV_READABLE, osc_poll_cb);
	uv_check_init(self.loop, &self.drain_notifications);

	uv_run(self.loop, UV_RUN_DEFAULT);

	uv_close((void *) &self.drain_notifications, NULL);
	uv_close((void *) &self.state_change.check, NULL);

	/* run once more to make sure libuv cleans up any internal resources. */
	uv_run(self.loop, UV_RUN_NOWAIT);

	VECTOR_FREE(&self.notifications);
	uv_loop_close(self.loop);

	return 0;

err_enable:
	lo_server_free(self.osc.server);
err_osc_server:
	uv_loop_close(self.loop);
	return -1;
}
