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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include <confuse.h>
#include <monome.h>
#include <lo/lo.h>

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


static void prepend_slash_if_necessary(char **dest, char *prefix) {
	if( *prefix != '/' )
		*dest = s_asprintf("/%s", prefix);
	else
		*dest = s_strdup(prefix);
}

static char *path_for_serial(const char *serial) {
	char *path, *cdir;

	cdir = sosc_get_config_directory();
	path = s_asprintf("%s/%s.conf", cdir, serial);

	s_free(cdir);
	return path;
}

int sosc_config_read(const char *serial, sosc_config_t *config) {
	cfg_t *cfg, *sec;
	char *path;

	if( !serial )
		return 1;

	cfg = cfg_init(opts, CFGF_NOCASE);
	path = path_for_serial(serial);

	switch( cfg_parse(cfg, path) ) {
	case CFG_PARSE_ERROR:
		fprintf(stderr, "serialosc [%s]: parse error in saved configuration\n",
				serial);
		break;
	}

	s_free(path);

	sec = cfg_getsec(cfg, "server");
	sosc_port_itos(config->server.port, cfg_getint(sec, "port"));

	sec = cfg_getsec(cfg, "application");
	prepend_slash_if_necessary(&config->app.osc_prefix, cfg_getstr(sec, "osc_prefix"));
	config->app.host = s_strdup(cfg_getstr(sec, "host"));
	sosc_port_itos(config->app.port, cfg_getint(sec, "port"));

	sec = cfg_getsec(cfg, "device");
	config->dev.rotation = (cfg_getint(sec, "rotation") / 90) % 4;

	cfg_free(cfg);

	return 0;
}

int sosc_config_write(const char *serial, sosc_state_t *state) {
	cfg_t *cfg, *sec;
	char *path;
	const char *p;
	FILE *f;

	if( !serial )
		return 1;

	cfg = cfg_init(opts, CFGF_NOCASE);

	path = path_for_serial(serial);
	if( !(f = fopen(path, "w")) ) {
		s_free(path);
		return 1;
	}

	s_free(path);

	sec = cfg_getsec(cfg, "server");
	cfg_setint(sec, "port", lo_server_get_port(state->server));

	sec = cfg_getsec(cfg, "application");
	cfg_setstr(sec, "osc_prefix", state->config.app.osc_prefix);
	cfg_setstr(sec, "host", lo_address_get_hostname(state->outgoing));
	p = lo_address_get_port(state->outgoing);
	cfg_setint(sec, "port", strtol(p , NULL, 10));

	sec = cfg_getsec(cfg, "device");
	cfg_setint(sec, "rotation", monome_get_rotation(state->monome) * 90);

	cfg_print(cfg, f);
	fclose(f);

	return 0;
}
