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

monome_t *scan_connected_devices() {
	monome_t *device = NULL;
	glob_t gb;
	int i;

	gb.gl_offs = 0;
	if( glob("/dev/tty.usbserial*", GLOB_NOSORT, NULL, &gb) )
		return NULL;
	
	for( i = 0; i < gb.gl_pathc; i++ ) {
		if( fork() )
			continue;

		device = monome_open(gb.gl_pathv[i]);
		break;
	}
	
	globfree(&gb);
	return device;
}

monome_t *next_device() {
	monome_t *device = NULL;
	int stat_loc;

	disable_subproc_waiting();

	if( (device = scan_connected_devices()) )
		return device;

	wait(&stat_loc);
	return NULL;
}
