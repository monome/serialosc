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

#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <uv.h>
#include <monome.h>
#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API static
#include <optparse/optparse.h>

#include <serialosc/serialosc.h>

int
main(int argc, char **argv)
{
	monome_t *device;
	const char *config_dir = NULL;
	const char *device_arg;

	int opt, longindex;
	struct optparse options;
	struct optparse_long longopts[] = {
		{"config-dir", 'c', OPTPARSE_REQUIRED},
		{0, 0, 0}
	};

	uv_setup_args(argc, argv);
	optparse_init(&options, argv);

	while ((opt = optparse_long(&options, longopts, &longindex)) != -1) {
		switch (opt) {
		case 'c':
			config_dir = options.optarg;
			break;
		default:
			fprintf(stderr, "%s: %s\n", argv[0], options.errmsg);
			return EXIT_FAILURE;
		}
	}

	if (options.optind < argc) {
		device_arg = optparse_arg(&options);
	} else {
		fprintf(stderr, "%s: device not specified, exiting\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (!(device = monome_open(device_arg))) {
		fprintf(stderr, "%s: failed to open device %s\n", argv[0], device_arg);
		return EXIT_FAILURE;
	}

	argv[0][strlen(argv[0]) - 1] = ' ';

#ifndef WIN32
	setenv("AVAHI_COMPAT_NOWARN", "shut up", 1);
#endif

	sosc_zeroconf_init();
	sosc_server_run(config_dir, device);
	monome_close(device);

	return EXIT_SUCCESS;
}
