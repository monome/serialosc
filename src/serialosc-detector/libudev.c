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

#define _XOPEN_SOURCE 600

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include <libudev.h>
#include <monome.h>

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>

typedef struct {
	struct udev *u;
	struct udev_monitor *um;
} detector_state_t;


static void
send_connect(const char *devnode)
{
	sosc_ipc_msg_t msg = {
		.type = SOSC_DEVICE_CONNECTION,
		.connection = {.devnode = (char *) devnode}
	};

	sosc_ipc_msg_write(STDOUT_FILENO, &msg);
}

static int
has_usb_serial_parent(struct udev_device *ud)
{
	if (udev_device_get_parent_with_subsystem_devtype(ud, "usb-serial", NULL))
		return 1;
	else
		return 0;
}

static char
test_cdc_driver(struct udev_device *ud) { 
	const char *p = udev_device_get_driver(ud);
	if (!p) return 0;
	char *drv = strdup(p);
	char is_cdc = strncmp(drv, "cdc", 3) == 0;
	free(drv); 
	return is_cdc;
}

static char
test_monome_props(struct udev_device *ud)
{
	char *vendor, *model;
	const char *tmp;
	tmp = udev_device_get_property_value(ud, "ID_VENDOR");
	if (!tmp) return 0;

	vendor = strdup(tmp);

	tmp = udev_device_get_property_value(ud, "ID_MODEL");
	if (!tmp) { 
		free(vendor);
		return 0;
	}

	model = strdup(tmp);

	printf("vendor: %s; model: %s\n\n", vendor, model);

	char res = 0;
	if (vendor != NULL && model != NULL) { 
		res = (strcmp(vendor,"monome")==0) && (strcmp(model,"grid")==0);
	}

	free(model);
	free(vendor);
	return res;
}


static char
test_monome_serial(struct udev_device *ud)
{
	const char *tmp;
	char *serial;
	int num;

	char res = 0;
	// search pattern for mext clones
	static const char match[] = "m%d";

	tmp = udev_device_get_property_value(ud, "ID_SERIAL_SHORT");
	if (!tmp) return 0;
	
	serial = strdup(tmp);
	printf("serial: %s;\n", serial);
	if( sscanf(serial, match, &num) )
		res = 1;

	free(serial);
	return res;
}


static char
has_usb_cdc_parent(struct udev_device *ud) {
	/// FIXME: pretty bad hack:
	/// assuming immediate parent in device tree uses "cdc_acm" driver
	return test_cdc_driver(udev_device_get_parent(ud));
}

static int
is_device_compatible(struct udev_device *ud) {
	int ok = has_usb_serial_parent(ud);
	if (ok) { 
		fprintf(stderr, "USB-serial device; OK\n");
		return 1;
	} 
	ok = has_usb_cdc_parent(ud);
	if (!ok) {
		return 0;
	}
	ok = test_monome_props(ud);
	if (ok) { 
		fprintf(stderr, "CDC device with monome properties; OK\n");
		return 1;
	}
	ok = test_monome_serial(ud);
	if (ok) { 
		fprintf(stderr, "CDC device with monome serial pattern; OK\n");
		return 1;
	}
	return 0;
}

static monome_t *
monitor_attach(detector_state_t *state)
{
	struct udev_device *ud;
	struct pollfd fds[1];

	fds[0].fd = udev_monitor_get_fd(state->um);
	fds[0].events = POLLIN;

	for (;;) {
		if (poll(fds, 1, -1) < 0)
			switch (errno) {
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
		if (*(udev_device_get_action(ud)) == 'a' && is_device_compatible(ud))
			send_connect(udev_device_get_devnode(ud));

		udev_device_unref(ud);
	}
}

static int
scan_connected_devices(detector_state_t *state)
{
	struct udev_list_entry *cursor;
	struct udev_enumerate *ue;
	struct udev_device *ud;

	const char *devnode = NULL;

	if (!(ue = udev_enumerate_new(state->u)))
		return 1;

	udev_enumerate_add_match_subsystem(ue, "tty");
	udev_enumerate_scan_devices(ue);
	cursor = udev_enumerate_get_list_entry(ue);

	do {
		ud = udev_device_new_from_syspath(
			state->u, udev_list_entry_get_name(cursor));

		if (is_device_compatible(ud) && (devnode = udev_device_get_devnode(ud)))
			send_connect(devnode);

		udev_device_unref(ud);
	} while ((cursor = udev_list_entry_get_next(cursor)));

	udev_enumerate_unref(ue);
	return 0;
}

int
main(int argc, char **argv)
{
	detector_state_t state;

	state.u = udev_new();

	if (scan_connected_devices(&state))
		return 1;

	if (!(state.um = udev_monitor_new_from_netlink(state.u, "udev")))
		return 2;

	udev_monitor_filter_add_match_subsystem_devtype(state.um, "tty", NULL);
	udev_monitor_enable_receiving(state.um);

	if (monitor_attach(&state))
		return 3;

	udev_monitor_unref(state.um);
	udev_unref(state.u);

	return 0;
}
