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

/* for setenv */
#define _XOPEN_SOURCE 600

#include <stdlib.h>
#include <stdio.h>
#include <monome.h>

#include "serialosc.h"


int main(int argc, char **argv) {
	monome_t *device;

	if( argc < 2 ) {
		sosc_config_create_directory();

		/* run as the detector process */
		if( detector_run(argv[0]) )
			return EXIT_FAILURE;

		return EXIT_SUCCESS;
	}

	/* run as a device OSC server */
	if( !(device = monome_open(argv[1])) )
		return EXIT_FAILURE;

#ifndef WIN32
	setenv("AVAHI_COMPAT_NOWARN", "shut up", 1);
#endif

	server_run(device);
	monome_close(device);

	return EXIT_SUCCESS;
}
