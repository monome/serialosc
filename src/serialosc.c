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

/* for setenv */
#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <monome.h>

#include "serialosc.h"
#include "ipc.h"


static int spawn_server(const char *exec_path, const char *devnode)
{
	switch (fork()) {
	case 0:  break;
	case -1: perror("spawn_server() fork"); return 1;
	default: return 0;
	}

	execlp(exec_path, exec_path, devnode, NULL);

	/* only get here if an error occurs */
	perror("spawn_server execlp");
	return 1;
}

static void read_detector_msgs(const char *progname, int fd)
{
	sosc_ipc_msg_t msg;

	do {
		if (sosc_ipc_msg_read(fd, &msg) < 0)
			continue;

		spawn_server(progname, msg.connection.devnode);
		s_free((char *) msg.connection.devnode);
	} while (1);
}

static int detector_party(char *progname)
{
	int pipefds[2];

	sosc_config_create_directory();

	if (pipe(pipefds) < 0) {
		perror("detector_loop() pipe");
		return 0;
	}

	switch (fork()) {
	case 0: 
		close(pipefds[0]);
		dup2(pipefds[1], STDOUT_FILENO);
		break;

	case -1:
		perror("detector_party() fork");
		return 1;

	default:
		close(pipefds[1]);
		read_detector_msgs(progname, pipefds[0]);
		return 0;
	}

	/* run as the detector process */
	if (detector_run(progname))
		return 1;

	return 0;
}

int main(int argc, char **argv)
{
	monome_t *device;

	if (argc < 2) {
		if (detector_party(argv[0]))
			return EXIT_FAILURE;
		else
			return EXIT_SUCCESS;
	}

	/* run as a device OSC server */
	if (!(device = monome_open(argv[1])))
		return EXIT_FAILURE;

#ifndef WIN32
	setenv("AVAHI_COMPAT_NOWARN", "shut up", 1);
#endif

	server_run(device);
	monome_close(device);

	return EXIT_SUCCESS;
}
