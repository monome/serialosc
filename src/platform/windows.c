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

#ifdef __STRICT_ANSI__
#undef __STRICT_ANSI__
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <direct.h>

#include "platform.h"

static int mk_monome_dir(char *cdir) {
	int ret = 0;
	char *last_slash = strrchr(cdir, '\\');
	*last_slash = '\0';

	if( _mkdir(cdir) && errno != EEXIST )
		ret = 1;

	*last_slash = '\\';
	return ret;
}

char *sosc_get_config_directory() {
	char *appdata;

	if( !(appdata = getenv("LOCALAPPDATA")) )
		appdata = getenv("APPDATA");

	return s_asprintf("%s\\Monome\\serialosc", appdata);
}

int sosc_config_create_directory() {
	char *cdir;
	struct _stat buf[1];

	cdir = sosc_get_config_directory();
	if( !_stat(cdir, buf) )
		return 0; /* all is well */

	if( mk_monome_dir(cdir) )
		goto err_mkdir;

	if( _mkdir(cdir) )
		goto err_mkdir;

	s_free(cdir);
	return 0;

err_mkdir:
	s_free(cdir);
	return 1;
}

char *s_asprintf(const char *fmt, ...) {
	va_list args;
	char *buf;
	int len;

	va_start(args, fmt);

	len = _vscprintf(fmt, args) + 1;
	if( !(buf = s_calloc(sizeof(char), len)) )
		return NULL;

	vsprintf(buf, fmt, args);
	va_end(args);

	return buf;
}

void *s_malloc(size_t size) {
	return malloc(size);
}

void *s_calloc(size_t nmemb, size_t size) {
	return calloc(nmemb, size);
}

void *s_strdup(const char *s) {
	return _strdup(s);
}

void s_free(void *ptr) {
	free(ptr);
}
