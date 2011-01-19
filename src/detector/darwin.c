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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/serial/IOSerialKeys.h>

#include <monome.h>

#include "serialosc.h"


typedef struct {
	IONotificationPortRef notify;
	io_iterator_t iter;

	const char *exec_path;
} notify_state_t;


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

static int wait_on_parent_usbdevice(io_service_t device) {
	io_registry_entry_t parent;

	/* walk up the device tree looking for the IOUSBDevice */
	for( ;; ) {
		/* return an error if we've walked off the end of the tree,
		   i.e. if this tty isn't a USB device. */
		if( IORegistryEntryGetParentEntry(device, kIOServicePlane, &parent) )
			return 1;
		device = parent;

		if( IOObjectConformsTo(device, kIOUSBDeviceClassName) )
			break;
	}

	/* wait until the device is ready to be opened */
	IOServiceWaitQuiet(device, NULL);
	return 0;
}

static void iterate_devices(void *context, io_iterator_t iter) {
	notify_state_t *state = context;

	io_service_t device;
	io_struct_inband_t devnode;
	unsigned int len = 256;

	while( (device = IOIteratorNext(iter)) ) {
		IORegistryEntryGetProperty(device, kIODialinDeviceKey, devnode, &len);

		if( !wait_on_parent_usbdevice(device) )
			spawn_server(state->exec_path, devnode);

		IOObjectRelease(device);
	}

	return;
}

static int init_iokitlib(notify_state_t *state) {
	CFMutableDictionaryRef matching;

	if( !(state->notify = IONotificationPortCreate((mach_port_t) 0)) ) {
		fprintf(stderr, "couldn't allocate notification port, aieee!\n");
		return 1;
	}

	CFRunLoopAddSource(
		/* run loop */  CFRunLoopGetCurrent(),
		/* source   */  IONotificationPortGetRunLoopSource(state->notify),
		/* mode     */  kCFRunLoopDefaultMode);

	matching = IOServiceMatching(kIOSerialBSDServiceValue);
	CFDictionarySetValue(matching,
		CFSTR(kIOSerialBSDTypeKey),
		CFSTR(kIOSerialBSDRS232Type));

	IOServiceAddMatchingNotification(
		/* notify port       */  state->notify,
		/* notification type */  kIOMatchedNotification,
		/* matching dict     */  matching,
		/* callback          */  iterate_devices,
		/* callback context  */  state,
		/* iterator          */  &state->iter);

	return 0;
}

static void fini_iokitlib(notify_state_t *state) {
	IOObjectRelease(state->iter);
	IONotificationPortDestroy(state->notify);
}

int detector_run(const char *exec_path) {
	notify_state_t state;

	assert(exec_path);
	state.exec_path = exec_path;

	disable_subproc_waiting();
	if( init_iokitlib(&state) )
		return 1;

	iterate_devices(&state, state.iter);
	CFRunLoopRun();

	fini_iokitlib(&state);
	return 0;
}
