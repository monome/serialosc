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
	char *dir;

	if( getenv("XDG_CONFIG_HOME") )
		dir = s_asprintf("%s/serialosc", getenv("XDG_CONFIG_HOME"));
	else
		dir = s_asprintf("%s/.config/serialosc", getenv("HOME"));

	return dir;
}

int sosc_config_create_directory() {
	char *cdir, *xdgdir;
	struct stat buf[1];

	cdir = sosc_get_config_directory();
	if( !stat(cdir, buf) )
		return 0; /* all is well */

	if( !getenv("XDG_CONFIG_HOME") ) {
		xdgdir = s_asprintf("%s/.config", getenv("HOME"));

		/* well, I guess somebody's got to do it */
		if( stat(xdgdir, buf) && mkdir(xdgdir, S_IRWXU) )
			goto err_xdg;
	}

	if( mkdir(cdir, S_IRWXU) )
		goto err_mkdir;

	s_free(cdir);
	return 0;

err_xdg:
	s_free(xdgdir);
err_mkdir:
	s_free(cdir);
	return 1;
}
