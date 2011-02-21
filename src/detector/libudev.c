/**
 * Copyright (c) 2010 William Light <wrl@illest.net>
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

#define _XOPEN_SOURCE 600

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include <libudev.h>
#include <monome.h>

#include "serialosc.h"


typedef struct {
	struct udev *u;
	struct udev_monitor *um;

	const char *exec_path;
} detector_state_t;


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

static int spawn_server(const char *exec_path, const char *devnode) {
	switch( fork() ) {
	case 0:  break;
	case -1: perror("spawn_server fork"); return 1;
	default: return 0;
	}

	execlp(exec_path, exec_path, devnode, NULL);

	/* only get here if an error occurs */
	perror("spawn_server execlp");
	return 1;
}

static monome_t *monitor_attach(detector_state_t *state) {
	struct udev_device *ud;
	struct pollfd fds[1];

	fds[0].fd = udev_monitor_get_fd(state->um);
	fds[0].events = POLLIN;

	do {
		if( poll(fds, 1, -1) < 0 )
			switch( errno ) {
			case EINVAL:
				perror("error in poll()");
				exit(1);

			case EINTR:
			case EAGAIN:
				continue;
			}

		ud = udev_monitor_receive_device(state->um);

		/* check if this was an add event.
		   "add"[0] == 'a' */
		if( *(udev_device_get_action(ud)) == 'a' )
			spawn_server(state->exec_path, udev_device_get_devnode(ud));

		udev_device_unref(ud);
	} while( 1 );
}

int scan_connected_devices(detector_state_t *state) {
	struct udev_list_entry *cursor;
	struct udev_enumerate *ue;
	struct udev_device *ud;

	const char *devnode = NULL;

	if( !(ue = udev_enumerate_new(state->u)) )
		return 1;

	udev_enumerate_add_match_subsystem(ue, "tty");
	udev_enumerate_add_match_property(ue, "ID_BUS", "usb");
	udev_enumerate_scan_devices(ue);
	cursor = udev_enumerate_get_list_entry(ue);

	do {
		ud = udev_device_new_from_syspath(
			state->u, udev_list_entry_get_name(cursor));

		if( (devnode = udev_device_get_devnode(ud)) )
			spawn_server(state->exec_path, devnode);

		udev_device_unref(ud);
	} while( (cursor = udev_list_entry_get_next(cursor)) );

	udev_enumerate_unref(ue);
	return 0;
}

int detector_run(const char *exec_path) {
	detector_state_t state = {
		.exec_path = exec_path
	};

	assert(exec_path);
	disable_subproc_waiting();
	state.u = udev_new();

	if( scan_connected_devices(&state) )
		return 1;

	if( !(state.um = udev_monitor_new_from_netlink(state.u, "udev")) )
		return 2;

	udev_monitor_filter_add_match_subsystem_devtype(state.um, "tty", NULL);
	udev_monitor_enable_receiving(state.um);

	if( monitor_attach(&state) )
		return 3;

	udev_monitor_unref(state.um);
	udev_unref(state.u);

	return 0;
}
