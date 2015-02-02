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

#include <stdio.h>
#include "serialosc.h"

/* convert a port int to either a zero-length string if 0 or
   a maximum 5 length string. */

void
sosc_port_itos(char *dest, long int port)
{
	if (port)
		snprintf(dest, sizeof(char) * 6, "%ld", port);
	else
		*dest = '\0';
}

size_t
sosc_strlcpy(char *dst, const char *src, size_t size)
{
	size_t ret;

	if (!dst)
		return 0;

	if (size > 0)
		size--;

	for (ret = 0; *src && ret < size; ret++, src++, dst++)
		*dst = *src;

	*dst = '\0';

	/* man page says that strlcpy() is supposed to walk the rest of src if
	 * if doesn't fit to determine strlen() on it, but walking an entire
	 * string that we've already verified doesn't fit into dst feels wasteful
	 * at best and dangerous at worst, so we'll just return the original
	 * size argument.
	 *
	 * in the event size was 0 and the first character of src is non-null,
	 * we return 1.
	 */
	if (*src)
		return size + 1;
	return ret;
}
