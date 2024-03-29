/**
 * Copyright (c) 2011 William Light <wrl@illest.net>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <serialosc/serialosc.h>

char *
s_asprintf(const char *fmt, ...)
{
	va_list args;
	char *buf;

	va_start(args, fmt);

	if (vasprintf(&buf, fmt, args) < 0)
		buf = NULL;

	va_end(args);
	return buf;
}

void *
s_malloc(size_t size)
{
	return malloc(size);
}

void *
s_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void *
s_strdup(const char *s)
{
	return strdup(s);
}

void
s_free(void *ptr)
{
	free(ptr);
}
