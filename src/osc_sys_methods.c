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

#define _GNU_SOURCE /* for asprintf */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lo/lo.h>
#include <monome.h>

#include "serialosc.h"
#include "osc.h"


static int portstr(char *dest, int src) {
	return snprintf(dest, 6, "%d", src);
}

static int sys_mode_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message data, void *user_data) {
	monome_t *monome = user_data;

	return monome_mode(monome, argv[0]->i);
}

static int sys_cable_legacy_handler(const char *path, const char *types,
                             lo_arg **argv, int argc,
                             lo_message data, void *user_data) {
	monome_t *monome = user_data;

	switch( argv[0]->s ) {
	case 'L':
	case 'l':
	case '0':
		monome_set_rotation(monome, MONOME_ROTATE_0);
		return 0;

	case 'T':
	case 't':
	case '9':
		monome_set_rotation(monome, MONOME_ROTATE_90);
		return 0;

	case 'R':
	case 'r':
	case '1':
		monome_set_rotation(monome, MONOME_ROTATE_180);
		return 0;

	case 'B':
	case 'b':
	case '2':
		monome_set_rotation(monome, MONOME_ROTATE_270);
		return 0;

	default:
		return 1;
	}
}

static int sys_rotation_handler(const char *path, const char *types,
                             lo_arg **argv, int argc,
                             lo_message data, void *user_data) {
	monome_t *monome = user_data;

	switch( argv[0]->i ) {
	case 0:   monome_set_rotation(monome, MONOME_ROTATE_0);   return 0;
	case 90:  monome_set_rotation(monome, MONOME_ROTATE_90);  return 0;
	case 180: monome_set_rotation(monome, MONOME_ROTATE_180); return 0;
	case 270: monome_set_rotation(monome, MONOME_ROTATE_270); return 0;
	default:  return 1;
	}
}

static void send_info(lo_address *to, sosc_state_t *state) {
	lo_send_from(to, state->server, LO_TT_IMMEDIATE, "/sys/info", "siis",
	             monome_get_serial(state->monome),
	             monome_get_rows(state->monome),
	             monome_get_cols(state->monome),
	             state->osc_prefix);
}

static int sys_info_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message data, void *user_data) {
	sosc_state_t *state = user_data;
	char port[6], *host = NULL;
	lo_address *dst;

	host = ( argc == 2 ) ? &argv[0]->s : NULL;
	portstr(port, argv[argc - 1]->i);

	if( !(dst = lo_address_new(host, port)) ) {
		fprintf(stderr, "sys_info_handler(): error in lo_address_new()");
		return 1;
	}

	send_info(dst, state);
	lo_address_free(dst);

	return 0;
}

static int sys_info_handler_default(const char *path, const char *types,
                                    lo_arg **argv, int argc,
                                    lo_message data, void *user_data) {
	sosc_state_t *state = user_data;
	send_info(state->outgoing, state);
	return 0;
}

static int sys_port_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message data, void *user_data) {
	sosc_state_t *state = user_data;
	lo_address *new, *old = state->outgoing;
	char port[6];

	portstr(port, argv[0]->i);

	if( !(new = lo_address_new(lo_address_get_hostname(old), port)) ) {
		fprintf(stderr, "sys_port_handler(): error in lo_address_new()\n");
		return 1;
	}

	lo_send_from(old, state->server, LO_TT_IMMEDIATE,
	             "/sys/port", "i", argv[0]->i);
	lo_send_from(new, state->server, LO_TT_IMMEDIATE,
	             "/sys/port", "i", argv[0]->i);

	state->outgoing = new;
	lo_address_free(old);

	return 0;
}

static int sys_host_handler(const char *path, const char *types,
                            lo_arg **argv, int argc,
                            lo_message data, void *user_data) {
	sosc_state_t *state = user_data;
	lo_address *new, *old = state->outgoing;

	if( !(new = lo_address_new(&argv[0]->s, lo_address_get_port(old))) ) {
		fprintf(stderr, "sys_host_handler(): error in lo_address_new()\n");
		return 1;
	}

	lo_send_from(old, state->server, LO_TT_IMMEDIATE,
	             "/sys/host", "s", &argv[0]->s);
	lo_send_from(new, state->server, LO_TT_IMMEDIATE,
	             "/sys/host", "s", &argv[0]->s);

	state->outgoing = new;
	lo_address_free(old);

	return 0;
}

static int sys_prefix_handler(const char *path, const char *types,
                              lo_arg **argv, int argc,
                              lo_message data, void *user_data) {
	sosc_state_t *state = user_data;
	char *new, *old = state->osc_prefix;

	new = &argv[0]->s;

	if( argv[0]->s != '/' )
		/* prepend a slash */
		asprintf(&new, "/%s", &argv[0]->s);
	else
		new = strdup(&argv[0]->s);

	osc_unregister_methods(state);
	state->osc_prefix = new;
	osc_register_methods(state);

	/*
	 * question: how do we notify applications of the new prefix?
	 * there's no {prefix}/prefix command, only /sys/prefix
	 */

	free(old);

	return 0;
}

void osc_register_sys_methods(sosc_state_t *state) {
	char *cmd;

#define METHOD(path) for( cmd = "/sys/" path; cmd; cmd = NULL )
#define REGISTER(types, handler, context) \
	lo_server_add_method(state->server, cmd, types, handler, context)

	METHOD("mode")
		REGISTER("i", sys_mode_handler, state->monome);

	METHOD("cable")
		REGISTER("s", sys_cable_legacy_handler, state->monome);

	METHOD("rotation")
		REGISTER("i", sys_rotation_handler, state->monome);

	METHOD("info") {
		REGISTER("si", sys_info_handler, state);
		REGISTER("i", sys_info_handler, state);
		REGISTER("", sys_info_handler_default, state);
	}

	METHOD("port")
		REGISTER("i", sys_port_handler, state);

	METHOD("host")
		REGISTER("s", sys_host_handler, state);

	METHOD("prefix")
		REGISTER("s", sys_prefix_handler, state);

#undef REGISTER
#undef METHOD
}
