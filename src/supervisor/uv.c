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
};

struct detector_state {
	VECTOR(devices, struct device_info) devices;
};

static void
supervisor_main(int detector_fd)
{
	uv_pipe_t from_detector;
	uv_loop_t *loop;

	loop = uv_default_loop();

	uv_pipe_init(loop, &from_detector, 0);
	uv_pipe_open(&from_detector, detector_fd);

	uv_read_start((uv_stream_t *) &from_detector, 

	uv_loop_close(loop);
}

int
sosc_supervisor_run(char *progname)
{
	int pipefds[2];

	sosc_config_create_directory();

	if (pipe(pipefds) < 0) {
		perror("sosc_supervisor_run() pipe");
		return 0;
	}

	switch (fork()) {
	case 0:
		close(pipefds[0]);
		dup2(pipefds[1], STDOUT_FILENO);
		break;

	case -1:
		perror("sosc_supervisor_run() fork");
		return 1;

	default:
		close(pipefds[1]);
		supervisor_main(pipefds[0]);
		return 0;
	}

	/* XXX: use uv_set_process_title() */
	progname[strlen(progname) - 1] = 'm';

	/* run as the detector process */
	if (sosc_detector_run(progname))
		return 1;

	return 0;
}
