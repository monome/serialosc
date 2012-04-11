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

#define _BSD_SOURCE

#include <stdlib.h>
#include <string.h>

#include <monome.h>
#include "serialosc.h"

int main(int argc, char **argv)
{
	monome_t *device;

	/* this file is the main entry-point for serialosc. here, we devide
	   whether we're running as serialoscd or as one of the per-device
	   OSC servers.

	   to run as a per-device OSC server, we expect argv[1] to be a path
	   to a monome tty/COM port. */

	if (argc < 2) {
		/* if we're missing that argument, run as the "supervisor" process,
		   which goes on to spawn the monitor, and in turn the individual
		   device processes.

		   this process will run as "serialoscd", and the monitor runs
		   as "serialoscm".

		   XXX: add some sort of lock file to prevent two manager
		        instances from running at the same time. */

		if (sosc_supervisor_run(argv[0]))
			return EXIT_FAILURE;
		else
			return EXIT_SUCCESS;
	}

	/* otherwise, we'll run as a per-device server. this next odd line
	   changes the process name from "serialoscd" to "serialosc" to aid
	   in differentiating between process types. it works on linux, at
	   least, because tools like ps peer inside the running executable
	   image and use argv[0] as the command name. */

	argv[0][strlen(argv[0]) - 1] = ' ';

	if (!(device = monome_open(argv[1])))
		return EXIT_FAILURE;

#ifndef WIN32
	setenv("AVAHI_COMPAT_NOWARN", "shut up", 1);
#endif

	sosc_server_run(device);
	monome_close(device);

	return EXIT_SUCCESS;
}
