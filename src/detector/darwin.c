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

#include <mach/mach.h>
#include <mach/mach_interface.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>

#include <monome.h>

typedef struct {
	mach_msg_header_t hdr;
	OSNotificationHeader notify_hdr;
	IOServiceInterestContent payload;
	mach_msg_trailer_t trailer;
} notify_msg_t;


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

static int init_iokitlib(mach_port_t *port, io_iterator_t *iter) {
	CFMutableDictionaryRef matching;

	if( mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, port) ) {
		fprintf(stderr, "couldn't allocate mach_port_t!\n");
		return 1;
	}

	matching = IOServiceMatching(kIOSerialBSDServiceValue);
	CFDictionarySetValue(matching,
		CFSTR(kIOSerialBSDTypeKey),
		CFSTR(kIOSerialBSDRS232Type));

	IOServiceAddNotification(
		/* master port       */  (mach_port_t) NULL,
		/* notification type */  kIOMatchedNotification,
		/* matching dict     */  matching,
		/* wake port         */  *port,
		/* reference (???)   */  42424242,
		/* iterator          */  iter);

	return 0;
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

static int wait_for_connection(mach_port_t *wait_port) {
	notify_msg_t msg;

	if( mach_msg(
			&msg.hdr, MACH_RCV_MSG, 0, sizeof(msg), 
			*wait_port, 0, MACH_PORT_NULL) ) {
		fprintf(stderr, "mach_msg() failed, aieee!\n");
		return 1;
	}

	return 0;
}

monome_t *next_device() {
	io_iterator_t iter;
	mach_port_t wait_port;
	monome_t *monome = NULL;

	disable_subproc_waiting();
	if( init_iokitlib(&wait_port, &iter) )
		return NULL;

	for(;; monome = NULL) {
		/* main detection loop:
		     monome = iterate_devices(NULL, iter) only returns in a subprocess,
			 and wait_for_connection(&wait_port) chills until a monome gets
			 plugged in. */

		if( (monome = iterate_devices(NULL, iter))
			|| wait_for_connection(&wait_port) )
			break;
	}

	IOObjectRelease(iter);
	mach_port_deallocate(mach_task_self(), wait_port);

	return monome;
}
