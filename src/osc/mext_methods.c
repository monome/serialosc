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

#include <lo/lo.h>
#include <monome.h>

#include "serialosc.h"
#include "osc.h"


OSC_HANDLER_FUNC(led_set_handler) {
	monome_t *monome = user_data;
	return monome_led_set(monome, argv[0]->i, argv[1]->i, !!argv[2]->i);
}

OSC_HANDLER_FUNC(led_all_handler) {
	monome_t *monome = user_data;
	return monome_led_all(monome, !!argv[0]->i);
}

OSC_HANDLER_FUNC(led_map_handler) {
	monome_t *monome = user_data;
	uint8_t buf[8];
	int i;

	for( i = 0; i < 8; i++ )
		buf[i] = argv[i + (argc - 8)]->i;

	return monome_led_map(monome, argv[0]->i, argv[1]->i, buf);
}

OSC_HANDLER_FUNC(led_col_handler) {
	monome_t *monome = user_data;
	uint8_t buf[32];
	int i;

	if( argc < 3 || argc > 34 )
		return 1;

	for( i = 0; i < argc; i++ )
		if( types[i] != 'i' ) /* only integers are invited to this party */
			return 1;

	for( i = 0; i < (argc - 2); i++ )
		buf[i] = argv[i + 2]->i;

	return monome_led_col(monome, argv[0]->i, argv[1]->i, argc - 2, buf);
}

OSC_HANDLER_FUNC(led_row_handler) {
	monome_t *monome = user_data;
	uint8_t buf[32];
	int i;

	if( argc < 3 || argc > 34 )
		return 1;

	for( i = 0; i < argc; i++ )
		if( types[i] != 'i' )
			return 1;

	for( i = 0; i < (argc - 2); i++ )
		buf[i] = argv[i + 2]->i;

	return monome_led_row(monome, argv[0]->i, argv[1]->i, argc - 2, buf);
}

OSC_HANDLER_FUNC(led_intensity_handler) {
	monome_t *monome = user_data;
	return monome_led_intensity(monome, argv[0]->i);
}

OSC_HANDLER_FUNC(led_level_set_handler) {
	monome_t *monome = user_data;
	return monome_led_level_set(monome, argv[0]->i, argv[1]->i, argv[2]->i);
}

OSC_HANDLER_FUNC(led_level_all_handler) {
	monome_t *monome = user_data;
	return monome_led_level_all(monome, argv[0]->i);
}

OSC_HANDLER_FUNC(led_level_map_handler) {
	monome_t *monome = user_data;
	uint8_t buf[64];
	int i;

	for( i = 0; i < 64; i++ )
		buf[i] = argv[i + (argc - 64)]->i;

	return monome_led_level_map(monome, argv[0]->i, argv[1]->i, buf);
}

OSC_HANDLER_FUNC(led_level_col_handler) {
	monome_t *monome = user_data;
	uint8_t buf[32];
	int i;

	if( argc < 3 || argc > 34 )
		return 1;

	for( i = 0; i < argc; i++ )
		if( types[i] != 'i' ) /* only integers are invited to this party */
			return 1;

	for( i = 0; i < (argc - 2); i++ )
		buf[i] = argv[i + 2]->i;

	return monome_led_level_col(monome, argv[0]->i, argv[1]->i, argc - 2, buf);
}

OSC_HANDLER_FUNC(led_level_row_handler) {
	monome_t *monome = user_data;
	uint8_t buf[32];
	int i;

	if( argc < 3 || argc > 34 )
		return 1;

	for( i = 0; i < argc; i++ )
		if( types[i] != 'i' )
			return 1;

	for( i = 0; i < (argc - 2); i++ )
		buf[i] = argv[i + 2]->i;

	return monome_led_level_row(monome, argv[0]->i, argv[1]->i, argc - 2, buf);
}

OSC_HANDLER_FUNC(led_ring_set_handler) {
	monome_t *monome = user_data;

	return monome_led_ring_set(monome, argv[0]->i, argv[1]->i, argv[2]->i);
}

OSC_HANDLER_FUNC(led_ring_all_handler) {
	monome_t *monome = user_data;

	return monome_led_ring_all(monome, argv[0]->i, argv[1]->i);
}

OSC_HANDLER_FUNC(led_ring_map_handler) {
	monome_t *monome = user_data;
	uint8_t buf[64];
	int i;

	for( i = 0; i < 64; i++ )
		buf[i] = argv[i + (argc - 64)]->i;

	return monome_led_ring_map(monome, argv[0]->i, buf);
}

OSC_HANDLER_FUNC(led_ring_range_handler) {
	monome_t *monome = user_data;

	return monome_led_ring_range(monome, argv[0]->i, argv[1]->i, argv[2]->i, argv[3]->i);
}

OSC_HANDLER_FUNC(tilt_set_handler) {
	monome_t *monome = user_data;

	if( argv[1]->i )
		return monome_tilt_enable(monome, argv[0]->i);
	else
		return monome_tilt_disable(monome, argv[0]->i);
}

#define METHOD(path) for( cmd_buf = osc_path(path, prefix); cmd_buf; \
                          s_free(cmd_buf), cmd_buf = NULL )

void osc_register_methods(sosc_state_t *state) {
	char *prefix, *cmd_buf;
	monome_t *monome;
	lo_server srv;

	prefix = state->config.app.osc_prefix;
	monome = state->monome;
	srv = state->server;

#define REGISTER(typetags, cb) \
	lo_server_add_method(srv, cmd_buf, typetags, cb, monome)

	METHOD("grid/led/set")
		REGISTER("iii", led_set_handler);

	METHOD("grid/led/all")
		REGISTER("i", led_all_handler);

	METHOD("grid/led/map")
		REGISTER("iiiiiiiiii", led_map_handler);

	METHOD("grid/led/col")
		REGISTER(NULL, led_col_handler);

	METHOD("grid/led/row")
		REGISTER(NULL, led_row_handler);

	METHOD("grid/led/intensity")
		REGISTER("i", led_intensity_handler);

	METHOD("grid/led/level/set")
		REGISTER("iii", led_level_set_handler);

	METHOD("grid/led/level/all")
		REGISTER("i", led_level_all_handler);

	METHOD("grid/led/level/map")
		REGISTER("ii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii",
		         led_level_map_handler);

	METHOD("grid/led/level/col")
		REGISTER(NULL, led_level_col_handler);

	METHOD("grid/led/level/row")
		REGISTER(NULL, led_level_row_handler);

	METHOD("ring/set")
		REGISTER("iii", led_ring_set_handler);

	METHOD("ring/all")
		REGISTER("ii", led_ring_all_handler);

	METHOD("ring/map")
		REGISTER("i"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii"
		         "iiiiiiii",
		         led_ring_map_handler);

	METHOD("ring/range")
		REGISTER("iiii", led_ring_range_handler);

	METHOD("tilt/set")
		REGISTER("ii", tilt_set_handler);

#undef REGISTER
}

void osc_unregister_methods(sosc_state_t *state) {
	char *prefix, *cmd_buf;
	lo_server srv;

	prefix = state->config.app.osc_prefix;
	srv = state->server;

#define UNREGISTER(typetags) \
	lo_server_del_method(srv, cmd_buf, typetags)

	METHOD("grid/led/set")
		UNREGISTER("iii");

	METHOD("grid/led/all")
		UNREGISTER("i");

	METHOD("grid/led/map")
		UNREGISTER("iiiiiiiiii");

	METHOD("grid/led/col")
		UNREGISTER(NULL);

	METHOD("grid/led/row")
		UNREGISTER(NULL);

	METHOD("grid/led/intensity")
		UNREGISTER("i");

	METHOD("grid/led/level/set")
		UNREGISTER("iii");

	METHOD("grid/led/level/all")
		UNREGISTER("i");

	METHOD("grid/led/level/map")
		UNREGISTER("ii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii");

	METHOD("grid/led/level/col")
		UNREGISTER(NULL);

	METHOD("grid/led/level/row")
		UNREGISTER(NULL);

	METHOD("ring/set")
		UNREGISTER("iii");

	METHOD("ring/all")
		UNREGISTER("ii");

	METHOD("ring/map")
		UNREGISTER("i"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii"
		           "iiiiiiii");

	METHOD("ring/range")
		UNREGISTER("iiii");

	METHOD("tilt/set")
		UNREGISTER("ii");

#undef UNREGISTER
}

#undef METHOD
