/**
 * Copyright (c) 2010 William Light <will@illest.net>
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
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include <libudev.h>

#include "serialosc.h"
#include "monitor.h"

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

static monome_t *event_loop(struct udev_monitor *um) {
	struct udev_device *ud;
	struct pollfd fds[1];
	const char *device;

	fds[0].fd = udev_monitor_get_fd(um);
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

		ud = udev_monitor_receive_device(um);

		/* check if this was an add event.
		   "add"[0] == 'a' */
		if( *(udev_device_get_action(ud)) != 'a' ) {
			udev_device_unref(ud);
			continue;
		}

		device = strdup(udev_device_get_devnode(ud));
		udev_device_unref(ud);

		if( !fork() )
			return monome_open(device);
	} while( 1 );
}

monome_t *next_connected_device(struct udev *u) {
	struct udev_list_entry *cursor;
	struct udev_enumerate *ue;
	struct udev_device *ud;
	const char *devnode = NULL;
	monome_t *device = NULL;

	if( !(ue = udev_enumerate_new(u)) )
		return NULL;

	udev_enumerate_add_match_subsystem(ue, "tty");
	udev_enumerate_scan_devices(ue);
	cursor = udev_enumerate_get_list_entry(ue);

	do {
		ud = udev_device_new_from_syspath(u, udev_list_entry_get_name(cursor));
		devnode = strdup(udev_device_get_devnode(ud));
		udev_device_unref(ud);

		if( !(device = monome_open(devnode)) )
			continue;

		if( !fork() )
			break;
	} while( (cursor = udev_list_entry_get_next(cursor)) );

	udev_enumerate_unref(ue);
	return device;
}

monome_t *next_device() {
	struct udev *u = udev_new();
	struct udev_monitor *um;
	monome_t *device = NULL;

	disable_subproc_waiting();

	if( !(um = udev_monitor_new_from_netlink(u, "udev")) )
		return NULL;

	udev_monitor_filter_add_match_subsystem_devtype(um, "tty", NULL);
	udev_monitor_enable_receiving(um);

	if( !(device = next_connected_device(u)) )
		device = event_loop(um);

	udev_monitor_unref(um);
	udev_unref(u);

	return device;
}
