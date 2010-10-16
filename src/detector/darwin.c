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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <glob.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOBSD.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/serial/IOSerialKeys.h>

#include <monome.h>


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

static monome_t *iterate_devices(void *context, io_iterator_t iter) {
	io_service_t device;
	CFTypeRef dev_path;
	char dev_node[256];

	while( (device = IOIteratorNext(iter)) ) {
		if( !(dev_path = IORegistryEntryCreateCFProperty(device, CFSTR(kIODialinDeviceKey), kCFAllocatorDefault, 0)) )
			continue;

		CFStringGetCString(dev_path, dev_node, sizeof(dev_node), kCFStringEncodingASCII);
		CFRelease(dev_path);

		if( !fork() )
			return monome_open(dev_node);
	}

	return NULL;
}

static int init_iokitlib(IONotificationPortRef notify, io_iterator_t *iter) {
	kern_return_t k;
	CFMutableDictionaryRef matching;

	/* initialize IOKit, tell it we want serial devices */
	if( !(matching = IOServiceMatching(kIOSerialBSDServiceValue)) ) {
		fprintf(stderr, "IOServiceMatching returned NULL.\n");
		return 1;
	}

	CFDictionarySetValue(
		matching,
		CFSTR(kIOSerialBSDTypeKey),
		CFSTR(kIOSerialBSDRS232Type));

	notify = IONotificationPortCreate((mach_port_t) NULL);
	k = IOServiceAddMatchingNotification(
		/* notify port       */  notify,
		/* notification type */  kIOFirstMatchNotification,
		/* matching dict     */  matching,
		/* callback          */  NULL,
		/* callback context  */  NULL,
		/* iterator          */  iter);

	return 0;
}

monome_t *next_device() {
	io_iterator_t iter;
	IONotificationPortRef notify;
	monome_t *device = NULL;
	int stat_loc;

	disable_subproc_waiting();
	init_iokitlib(notify, &iter);

	if( (device = iterate_devices(NULL, iter)) )
		return device;

	wait(&stat_loc);
	return NULL;
}
