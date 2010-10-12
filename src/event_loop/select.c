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
#include <errno.h>
#include <sys/select.h>

#include "serialosc.h"


int event_loop(const sosc_state_t *state) {
	fd_set rfds, efds;
	int maxfd, mfd, lofd;

	mfd  = monome_get_fd(state->monome);
	lofd = lo_server_get_socket_fd(state->server);
	maxfd = ((lofd > mfd) ? lofd : mfd) + 1;

	do {
		FD_ZERO(&rfds);
		FD_SET(mfd, &rfds);
		FD_SET(lofd, &rfds);

		FD_ZERO(&efds);
		FD_SET(mfd, &efds);

		/* block until either the monome or liblo have data */
		if( select(maxfd, &rfds, NULL, &efds, NULL) < 0 )
			switch( errno ) {
			case EBADF:
			case EINVAL:
				perror("error in select()");
				return 1;

			case EINTR:
				continue;
			}

		/* is the monome still connected? */
		if( FD_ISSET(mfd, &efds) )
			return 1;

		/* is there data available for reading from the monome? */
		if( FD_ISSET(mfd, &rfds) )
			monome_event_handle_next(state->monome);

		/* how about from OSC? */
		if( FD_ISSET(lofd, &rfds) )
			lo_server_recv_noblock(state->server, 0);
	} while( 1 );
}
