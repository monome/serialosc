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

#include "serialosc.h"

struct device_info {
	int ready;
	int port;

	char *serial;
	char *friendly;

	uv_process_t process;
};

struct supervisor_state {
	uv_loop_t *loop;

	VECTOR(devices, struct device_info) devices;

	struct {
		uv_process_t proc;
		int pipe_fd;
		uv_poll_t pipe_poll;
	} detector;
};

static void
detector_pipe_cb(uv_poll_t *handle, int status, int events)
{
	struct supervisor_state *self = container_of(handle,
			struct supervisor_state, detector.pipe_poll);

	char buf[256];

	printf("%d %d %zd\n", status, events,
			read(self->detector.pipe_fd, buf, sizeof(buf)));
}

static int
launch_detector(struct supervisor_state *self)
{
	struct uv_process_options_s options;
	char path_buf[1024];
	int pipefds[2], err;
	size_t len;

	len = sizeof(path_buf);
	err = uv_exepath(path_buf, &len);
	if (err < 0) {
		fprintf(stderr, "launch_detector() failed in uv_exepath(): %s\n",
				uv_strerror(err));

		return err;
	}

	err = pipe(pipefds);
	if (err < 0) {
		perror("launch_detector() failed in pipe()");
		return err;
	}

	options = (struct uv_process_options_s) {
		.exit_cb = NULL,

		.file    = path_buf,
		.args    = (char *[]) {path_buf, "-d", NULL},
		.flags   = UV_PROCESS_WINDOWS_HIDE,

		.stdio_count = 2,
		.stdio = (struct uv_stdio_container_s []) {
			[STDIN_FILENO] = {
				.flags = UV_IGNORE
			},

			[STDOUT_FILENO] = {
				.flags = UV_INHERIT_FD,
				.data.fd = pipefds[1]
			}
		},
	};

	err = uv_spawn(self->loop, &self->detector.proc, &options);
	if (err < 0) {
		fprintf(stderr, "launch_detector() failed in uv_spawn(): %s\n",
				uv_strerror(err));
		return err;
	}

	self->detector.pipe_fd = pipefds[0];
	close(pipefds[1]);
	return 0;
}

int
sosc_supervisor_run(char *progname)
{
	struct supervisor_state self;

	sosc_config_create_directory();

	self.loop = uv_default_loop();
	if (launch_detector(&self))
		goto err_detector;

	uv_set_process_title("serialosc [supervisor]");

	uv_poll_init(self.loop, &self.detector.pipe_poll, self.detector.pipe_fd);
	uv_poll_start(&self.detector.pipe_poll, UV_READABLE, detector_pipe_cb);

	uv_run(self.loop, UV_RUN_DEFAULT);

	uv_loop_close(self.loop);
	return 0;

err_detector:
	uv_loop_close(self.loop);
	return -1;
}
