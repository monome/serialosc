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

#ifdef __STRICT_ANSI__
#undef __STRICT_ANSI__
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <serialosc/osc.h>

char *
osc_path(const char *path, const char *prefix)
{
	char *buf;

	if (!(buf = s_asprintf("%s/%s", prefix, path))) {
		fprintf(stderr, "aieee, could not allocate memory in "
				"osc_path(), bailing out!\n");

		/* in a child process, use _exit() instead of exit() */
		_exit(EXIT_FAILURE);
	}

	return buf;
}
