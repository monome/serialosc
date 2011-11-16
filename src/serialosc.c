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
#include <string.h>
#include <signal.h>
#include <poll.h>

#include <monome.h>

#include "serialosc.h"
#include "ipc.h"

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(*x))

static void disable_subproc_waiting() {
	struct sigaction s;

	memset(&s, 0, sizeof(struct sigaction));
	s.sa_flags = SA_NOCLDWAIT;
	s.sa_handler = SIG_IGN;

	if( sigaction(SIGCHLD, &s, NULL) < 0 ) {
		perror("disable_subproc_waiting");
		exit(EXIT_FAILURE);
	}
}

static int spawn_server(const char *exec_path, const char *devnode)
{
	int pipefds[2];

	if (pipe(pipefds) < 0) {
		perror("spawn_server() pipe");
		return -1;
	}

	switch (fork()) {
	case 0:
		close(pipefds[0]);
		dup2(pipefds[1], STDOUT_FILENO);
		break;

	case -1:
		perror("spawn_server() fork");
		return -1;

	default:
		close(pipefds[1]);
		return pipefds[0];
	}

	execlp(exec_path, exec_path, devnode, NULL);

	/* only get here if an error occurs */
	perror("spawn_server execlp");
	return -1;
}

static void read_detector_msgs(const char *progname, int fd)
{
	int child_fd, children, i;
	struct pollfd fds[33];
	sosc_ipc_msg_t msg;

	disable_subproc_waiting();

	fds[0].fd = fd;
	fds[0].events = POLLIN;

	children = 0;

	do {
		if (poll(fds, children + 1, -1) < 0) {
			perror("read_detector_msgs() poll");
			break;
		}

		for (i = 0; i < children + 1; i++) {
			if (!(fds[i].revents & POLLIN))
				continue;

			if (sosc_ipc_msg_read(fds[i].fd, &msg) < 0)
				continue;

			switch (msg.type) {
			case SOSC_DEVICE_CONNECTION:
				if (children >= ARRAY_LENGTH(fds)) {
					s_free((char *) msg.connection.devnode);
					fprintf(stderr,
							"read_detector_msgs(): too many monomes\n");
					continue;
				}

				child_fd = spawn_server(progname, msg.connection.devnode);
				printf(" - new device %s (#%d)\n", msg.connection.devnode,
					   children + 1);

				s_free(msg.connection.devnode);

				children++;
				fds[children].fd = child_fd;
				fds[children].events = POLLIN;

				break;

			case SOSC_DEVICE_INFO:
				printf(" - devinfo\n"
					   "   - serial: %s\n"
					   "   - friendly: %s\n",
					   msg.device_info.serial, msg.device_info.friendly);

				s_free(msg.device_info.serial);
				s_free(msg.device_info.friendly);
				break;

			case SOSC_DEVICE_DISCONNECTION:
				/* close the fd */
				close(fds[i].fd);
				/* shift everything in the array down by one */
				memmove(&fds[i], &fds[i + 1], children - i + 1);
				children--;

				/* and since fds[i + 1] has become fds[i], we'll
				   repeat this iteration of the for() loop */
				i--;

				printf(" - disconnection\n");
				break;

			case SOSC_OSC_PORT_CHANGE:
				printf(" - port change %d\n", msg.port_change.port);
				break;
			}
		}
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

	progname[strlen(progname) - 1] = 'm';

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

	argv[0][strlen(argv[0]) - 1] = 0;

	/* run as a device OSC server */
	if (!(device = monome_open(argv[1])))
		return EXIT_FAILURE;

	server_run(device);
	monome_close(device);

	return EXIT_SUCCESS;
}
