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
#include <stdlib.h>
#include <string.h>

#include <lo/lo.h>
#include <monome.h>

#include "serialosc.h"
#include "osc.h"


/**
 * utils
 */

static int portstr(char *dest, int src) {
	return snprintf(dest, 6, "%d", src);
}

/**
 * /sys/info business
 */

typedef void (info_reply_func_t)(lo_address *, sosc_state_t *);

static int info_prop_handler(lo_arg **argv, int argc, void *user_data,
                              info_reply_func_t cb) {
	sosc_state_t *state = user_data;
	const char *host = NULL;
	char port[6];
	lo_address *dst;

	if( argc == 2 )
		host = &argv[0]->s;
	else
		host = lo_address_get_hostname(state->outgoing);

	portstr(port, argv[argc - 1]->i);

	if( !(dst = lo_address_new(host, port)) ) {
		fprintf(stderr, "sys_info_handler(): error in lo_address_new()");
		return 1;
	}

	cb(dst, state);
	lo_address_free(dst);

	return 0;
}

static int info_prop_handler_default(void *user_data, info_reply_func_t cb) {
	sosc_state_t *state = user_data;
	cb(state->outgoing, state);
	return 0;
}

#define DECLARE_INFO_REPLY_FUNC(prop, typetag, ...)\
	static void info_reply_##prop(lo_address *to, sosc_state_t *state) {\
		lo_send_from(to, state->server, LO_TT_IMMEDIATE, "/sys/" #prop,\
					 typetag, __VA_ARGS__);\
	}

#define DECLARE_INFO_HANDLERS(prop)\
	OSC_HANDLER_FUNC(sys_info_##prop##_handler) {\
		return info_prop_handler(argv, argc, user_data, info_reply_##prop);\
	}\
	OSC_HANDLER_FUNC(sys_info_##prop##_handler_default) {\
		return info_prop_handler_default(user_data, info_reply_##prop);\
	}

#define DECLARE_INFO_PROP(prop, typetag, ...)\
	DECLARE_INFO_REPLY_FUNC(prop, typetag, __VA_ARGS__)\
	DECLARE_INFO_HANDLERS(prop)

DECLARE_INFO_PROP(id, "s", monome_get_serial(state->monome))
DECLARE_INFO_PROP(size, "ii", monome_get_cols(state->monome),
                  monome_get_rows(state->monome))
DECLARE_INFO_PROP(host, "s", lo_address_get_hostname(state->outgoing))
DECLARE_INFO_PROP(port, "i", atoi(lo_address_get_port(state->outgoing)))
DECLARE_INFO_PROP(prefix, "s", state->config.app.osc_prefix)

static void info_reply_all(lo_address *to, sosc_state_t *state) {
	info_reply_id(to, state);
	info_reply_size(to, state);
	info_reply_host(to, state);
	info_reply_port(to, state);
	info_reply_prefix(to, state);
}

OSC_HANDLER_FUNC(sys_info_handler) {
	return info_prop_handler(argv, argc, user_data, info_reply_all);
}

OSC_HANDLER_FUNC(sys_info_handler_default) {
	return info_prop_handler_default(user_data, info_reply_all);
}

/**/
 

OSC_HANDLER_FUNC(sys_mode_handler) {
	monome_t *monome = user_data;

	return monome_mode(monome, argv[0]->i);
}

OSC_HANDLER_FUNC(sys_cable_legacy_handler) {
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

OSC_HANDLER_FUNC(sys_rotation_handler) {
	monome_t *monome = user_data;

	switch( argv[0]->i ) {
	case 0:   monome_set_rotation(monome, MONOME_ROTATE_0);   return 0;
	case 90:  monome_set_rotation(monome, MONOME_ROTATE_90);  return 0;
	case 180: monome_set_rotation(monome, MONOME_ROTATE_180); return 0;
	case 270: monome_set_rotation(monome, MONOME_ROTATE_270); return 0;
	default:  return 1;
	}
}

OSC_HANDLER_FUNC(sys_port_handler) {
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

OSC_HANDLER_FUNC(sys_host_handler) {
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

OSC_HANDLER_FUNC(sys_prefix_handler) {
	sosc_state_t *state = user_data;
	char *new, *old = state->config.app.osc_prefix;

	if( argv[0]->s != '/' )
		/* prepend a slash */
		new = s_asprintf("/%s", &argv[0]->s);
	else
		new = s_strdup(&argv[0]->s);

	osc_unregister_methods(state);
	state->config.app.osc_prefix = new;
	osc_register_methods(state);

	lo_send_from(state->outgoing, state->server, LO_TT_IMMEDIATE,
	             "/sys/prefix", "s", new);

	s_free(old);

	return 0;
}

void osc_register_sys_methods(sosc_state_t *state) {
	char *cmd;

#define METHOD(path) for( cmd = "/sys/" path; cmd; cmd = NULL )
#define REGISTER(types, handler, context) \
	lo_server_add_method(state->server, cmd, types, handler, context)
#define REGISTER_INFO_PROP(prop) do {\
	METHOD("info/" #prop) {\
		REGISTER("si", sys_info_##prop##_handler, state);\
		REGISTER("i", sys_info_##prop##_handler, state);\
		REGISTER("", sys_info_##prop##_handler_default, state);\
	} } while ( 0 )

	REGISTER_INFO_PROP(id);
	REGISTER_INFO_PROP(size);
	REGISTER_INFO_PROP(host);
	REGISTER_INFO_PROP(port);
	REGISTER_INFO_PROP(prefix);

	METHOD("info") {
		REGISTER("si", sys_info_handler, state);
		REGISTER("i", sys_info_handler, state);
		REGISTER("", sys_info_handler_default, state);
	}

	METHOD("mode")
		REGISTER("i", sys_mode_handler, state->monome);

	METHOD("cable")
		REGISTER("s", sys_cable_legacy_handler, state->monome);

	METHOD("rotation")
		REGISTER("i", sys_rotation_handler, state->monome);

	METHOD("port")
		REGISTER("i", sys_port_handler, state);

	METHOD("host")
		REGISTER("s", sys_host_handler, state);

	METHOD("prefix")
		REGISTER("s", sys_prefix_handler, state);

#undef REGISTER
#undef METHOD
}
