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
#include <IOKit/serial/IOSerialKeys.h>

#include <monome.h>


typedef struct {
	IONotificationPortRef notify;
	io_iterator_t iter;
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

static void device_added(void *context, io_iterator_t iter) {
	/* what is it with apple and requiring callbacks all over the place? */
}

static int init_iokitlib(notify_state_t *state) {
	CFMutableDictionaryRef matching;

	if( !(state->notify = IONotificationPortCreate((mach_port_t) NULL)) ) {
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
		/* callback          */  device_added,
		/* callback context  */  NULL,
		/* iterator          */  &state->iter);

	return 0;
}

static void fini_iokitlib(notify_state_t *state) {
	IOObjectRelease(state->iter);
	IONotificationPortDestroy(state->notify);
}

static monome_t *iterate_devices(void *context, io_iterator_t iter) {
	monome_t *monome;

	io_service_t device;
	io_struct_inband_t dev_node;
	unsigned int len = 256;

	while( (device = IOIteratorNext(iter)) ) {
		IORegistryEntryGetProperty(device, kIODialinDeviceKey, dev_node, &len);

		if( !fork() ) {
			monome = monome_open(dev_node);
			IOObjectRelease(device);

			return monome;
		}

		IOObjectRelease(device);
	}

	return NULL;
}

static int wait_for_connection() {
	CFRunLoopRunInMode(
		/*                        mode  */  kCFRunLoopDefaultMode,
		/*                     timeout  */  1.0e10,
		/* return after source handled  */  true);

	return 0;
}

monome_t *next_device() {
	notify_state_t state;
	monome_t *monome = NULL;

	disable_subproc_waiting();
	if( init_iokitlib(&state) )
		return NULL;

	for(;; monome = NULL) {
		/* main detection loop:
		     monome = iterate_devices(NULL, iter) only returns in a subprocess,
		     and wait_for_connection() chills until you plug in a monome. */

		if( (monome = iterate_devices(NULL, state.iter))
			|| wait_for_connection() )
			break;
	}

	fini_iokitlib(&state);
	return monome;
}
