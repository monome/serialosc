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

/* for vasprintf */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <confuse.h>
#include <monome.h>

#include "serialosc.h"


#define DEFAULT_SERVER_PORT  0
#define DEFAULT_OSC_PREFIX   "/monome"
#define DEFAULT_APP_PORT     8000
#define DEFAULT_APP_HOST     "127.0.0.1"
#define DEFAULT_ROTATION     MONOME_ROTATE_0


static cfg_opt_t server_opts[] = {
	CFG_INT("port",       DEFAULT_SERVER_PORT, CFGF_NONE),
	CFG_END()
};

static cfg_opt_t app_opts[] = {
	CFG_STR("osc_prefix", DEFAULT_OSC_PREFIX,  CFGF_NONE),
	CFG_STR("host",       DEFAULT_APP_HOST,    CFGF_NONE),
	CFG_INT("port",       DEFAULT_APP_PORT,    CFGF_NONE),
	CFG_END()
};

static cfg_opt_t dev_opts[] = {
	CFG_INT("rotation",   DEFAULT_ROTATION,    CFGF_NONE),
	CFG_END()
};

static cfg_opt_t opts[] = {
	CFG_SEC("server", server_opts, CFGF_NONE),
	CFG_SEC("application", app_opts, CFGF_NONE),
	CFG_SEC("device", dev_opts, CFGF_NONE),
	CFG_END()
};


static void set_port_if_not_zero(char *dest, long int port) {
	if( port )
		snprintf(dest, sizeof(char) * 6, "%ld", port);
	else
		*dest = '\0';
}

static void prepend_slash_if_necessary(char **dest, char *prefix) {
	if( *prefix != '/' )
		asprintf(dest, "/%s", prefix);
	else
		*dest = strdup(prefix);
}

int sosc_read_device_config(const char *serial, sosc_config_t *config) {
	cfg_t *cfg, *sec;
	char *path;

	if( !serial )
		return 1;

	cfg = cfg_init(opts, CFGF_NOCASE);
	asprintf(&path, "%s/serialosc/%s.conf", getenv("XDG_CONFIG_HOME"), serial);

	switch( cfg_parse(cfg, path) ) {
	case CFG_FILE_ERROR:
		perror(serial);
		break;

	case CFG_PARSE_ERROR:
		fprintf(stderr, "serialosc [%s]: parse error in saved configuration\n",
				serial);
		break;
	}

	free(path);

	sec = cfg_getsec(cfg, "server");
	set_port_if_not_zero(config->server.port, cfg_getint(sec, "port"));

	sec = cfg_getsec(cfg, "application");
	prepend_slash_if_necessary(&config->app.osc_prefix, cfg_getstr(sec, "osc_prefix"));
	config->app.host = strdup(cfg_getstr(sec, "host"));
	set_port_if_not_zero(config->app.port, cfg_getint(sec, "port"));

	sec = cfg_getsec(cfg, "device");
	config->dev.rotation = (cfg_getint(sec, "rotation") / 90) % 4;

	cfg_free(cfg);

	return 0;
}
