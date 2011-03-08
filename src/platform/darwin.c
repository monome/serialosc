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
#include <sys/stat.h>

#include "platform.h"

char *sosc_get_config_directory() {
	return s_asprintf("%s/Library/Preferences/org.monome.serialosc",
					  getenv("HOME"));
}

int sosc_config_create_directory() {
	struct stat buf[1];
	char *cdir;

	cdir = sosc_get_config_directory();
	if( !stat(cdir, buf) )
		return 0; /* all is well */

	if( mkdir(cdir, S_IRWXU) )
		goto err_mkdir;

	s_free(cdir);
	return 0;

err_mkdir:
	s_free(cdir);
	return 1;
}
