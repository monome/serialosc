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
#include "osc.h"

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(*x))
#define MAX_DEVICES 32

typedef struct sosc_device_info {
	int ready;

	uint16_t port;
	char *serial;
	char *friendly;
} sosc_device_info_t;

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

typedef struct {
	int count;
	sosc_device_info_t *info[MAX_DEVICES];
} sosc_dev_datastore_t;

#define MAX_NOTIFICATION_ENDPOINTS 32

typedef struct {
	char host[256];
	char port[6];
} sosc_notification_endpoint_t;

typedef struct {
	int count;
	sosc_notification_endpoint_t endpoints[MAX_NOTIFICATION_ENDPOINTS];
} sosc_notifications_t;

sosc_notifications_t notifications = {0};

static lo_server *srv;

static int portstr(char *dest, int src) {
	return snprintf(dest, 6, "%d", src);
}

OSC_HANDLER_FUNC(dsc_list_devices)
{
	sosc_dev_datastore_t *devs = user_data;
	lo_address *dst;
	char port[6];
	int i;

	portstr(port, argv[1]->i);

	if (!(dst = lo_address_new(&argv[0]->s, port))) {
		fprintf(stderr, "dsc_list_devices(): error in lo_address_new()\n");
		return 1;
	}

	for (i = 0; i < devs->count; i++)
		lo_send_from(dst, srv, LO_TT_IMMEDIATE, "/serialosc/device", "ssi",
					 devs->info[i]->serial,
					 devs->info[i]->friendly,
					 devs->info[i]->port);

	lo_address_free(dst);

	return 0;
}

OSC_HANDLER_FUNC(add_notification_endpoint)
{
	sosc_notification_endpoint_t *n;

	if (notifications.count >= MAX_NOTIFICATION_ENDPOINTS)
		return 1;

	n = &notifications.endpoints[notifications.count];

	portstr(n->port, argv[1]->i);
	strncpy(n->host, &argv[0]->s, sizeof(n->host));
	n->host[sizeof(n->host) - 1] = '\0';

	notifications.count++;
	return 0;
}

static lo_server *setup_osc_server(sosc_dev_datastore_t *devs)
{
	lo_server *srv;

	if (!(srv = lo_server_new("12002", NULL)))
		return NULL;

	lo_server_add_method(srv, "/serialosc/list", "si", dsc_list_devices, devs);
	lo_server_add_method(srv, "/serialosc/update", "si", add_notification_endpoint, devs);

	return srv;
}

static int holla(sosc_ipc_type_t type, sosc_device_info_t *dev)
{
	lo_address dst;
	char *path;
	int i;

	switch (type) {
	case SOSC_DEVICE_CONNECTION:
		path = "/serialosc/add";
		break;

	case SOSC_DEVICE_DISCONNECTION:
		path = "/serialosc/remove";
		break;

	default:
		return 1;
	}

	for (i = 0; i < notifications.count; i++) {
		if (!(dst = lo_address_new(
		            notifications.endpoints[i].host,
		            notifications.endpoints[i].port))) {
			fprintf(stderr, "holla(): couldn't allocate lo_address\n");
			continue;
		}

		lo_send_from(dst, srv, LO_TT_IMMEDIATE, path, "ssi",
		             dev->serial, dev->friendly, dev->port);

		lo_address_free(dst);
	}

	return 0;
}

static void read_detector_msgs(const char *progname, int fd)
{
	sosc_dev_datastore_t devs = {
		0, {[0 ... MAX_DEVICES - 1] = NULL}
	};
	struct pollfd fds[MAX_DEVICES + 2];
	sosc_ipc_msg_t msg;
	int child_fd, i, notified;

#define FD_COUNT (devs.count + 2)
#define DEVINDEX(x) (x + 2)

	disable_subproc_waiting();

	if (!(srv = setup_osc_server(&devs))) {
		perror("couldn't init OSC server");
		return;
	}

	fds[0].fd = lo_server_get_socket_fd(srv);
	fds[0].events = POLLIN;

	fds[1].fd = fd;
	fds[1].events = POLLIN;

	do {
		notified = 0;

		if (poll(fds, FD_COUNT, -1) < 0) {
			perror("read_detector_msgs() poll");
			break;
		}

		if (fds[0].revents & POLLIN )
			lo_server_recv_noblock(srv, 0);

		for (i = 1; i < FD_COUNT; i++) {
			if (!(fds[i].revents & POLLIN))
				continue;

			if (sosc_ipc_msg_read(fds[i].fd, &msg) < 0)
				continue;

			switch (msg.type) {
			case SOSC_DEVICE_CONNECTION:
				if (devs.count >= ARRAY_LENGTH(fds)) {
					s_free((char *) msg.connection.devnode);
					fprintf(stderr,
							"read_detector_msgs(): too many monomes\n");
					continue;
				}

				child_fd = spawn_server(progname, msg.connection.devnode);
				printf(" - new device %s (#%d)\n", msg.connection.devnode,
					   devs.count + 1);

				s_free(msg.connection.devnode);

				if (!(devs.info[devs.count] = s_calloc(1, sizeof(*devs.info[devs.count])))) {
					fprintf(stderr, "calloc failed!\n");
					continue;
				}

				fds[DEVINDEX(devs.count)].fd = child_fd;
				fds[DEVINDEX(devs.count)].events = POLLIN;

				devs.count++;

				break;

			case SOSC_OSC_PORT_CHANGE:
				devs.info[i - 2]->port = msg.port_change.port;
				break;

			case SOSC_DEVICE_INFO:
				devs.info[i - 2]->serial = msg.device_info.serial;
				devs.info[i - 2]->friendly = msg.device_info.friendly;
				break;

			case SOSC_DEVICE_READY:
				devs.info[i - 2]->ready = 1;

				holla(SOSC_DEVICE_CONNECTION, devs.info[i - 2]);
				notified = 1;
				break;

			case SOSC_DEVICE_DISCONNECTION:
				printf(" - disconnection\n"
				       "   - devinfo\n"
					   "     - serial: %s\n"
					   "     - friendly: %s\n",
					  devs.info[i - 2]->serial, devs.info[i - 2]->friendly);

				holla(SOSC_DEVICE_DISCONNECTION, devs.info[i - 2]);
				notified = 1;

				/* close the fd and free the devinfo struct */
				close(fds[i].fd);
				s_free(devs.info[i - 2]->serial);
				s_free(devs.info[i - 2]->friendly);
				s_free(devs.info[i - 2]);

				/* shift everything in the array down by one */
				memmove(&fds[i], &fds[i + 1], devs.count - i + 1);
				memmove(&devs.info[i - 2], &devs.info[i - 1], devs.count - i + 1);
				devs.count--;

				/* and since fds[i + 1] has become fds[i], we'll
				   repeat this iteration of the for() loop */
				i--;

				break;
			}
		}

		if (notified)
			notifications.count = 0;
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
