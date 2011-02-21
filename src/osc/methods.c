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

#include <stdlib.h>

#include <lo/lo.h>
#include <monome.h>

#include "serialosc.h"
#include "osc.h"


OSC_HANDLER_FUNC(osc_clear_handler) {
	monome_t *monome = user_data;
	int mode = (argc) ? argv[0]->i : 0;

	return monome_clear(monome, mode);
}

OSC_HANDLER_FUNC(osc_intensity_handler) {
	monome_t *monome = user_data;
	int intensity = (argc) ? argv[0]->i : 0xF;

	return monome_intensity(monome, intensity);
}

OSC_HANDLER_FUNC(osc_led_handler) {
	monome_t *monome = user_data;
	return monome_led(monome, argv[0]->i, argv[1]->i, argv[2]->i);
}

OSC_HANDLER_FUNC(osc_led_col_handler) {
	monome_t *monome = user_data;
	uint8_t buf[2] = {argv[1]->i};

	if( argc == 3 )
		buf[1] = argv[2]->i;

	return monome_led_col(monome, argv[0]->i, 0, argc - 1, buf);
}

OSC_HANDLER_FUNC(osc_led_row_handler) {
	monome_t *monome = user_data;
	uint8_t buf[2] = {argv[1]->i};

	if( argc == 3 )
		buf[1] = argv[2]->i;

	return monome_led_row(monome, argv[0]->i, 0, argc - 1, buf);
}

OSC_HANDLER_FUNC(osc_frame_handler) {
	monome_t *monome = user_data;
	uint8_t buf[8];
	int i;

	for( i = 0; i < 8; i++ )
		buf[i] = argv[i + (argc - 8)]->i;

	switch( argc ) {
	case 8:
		return monome_led_frame(monome, 0, 0, buf);

	case 10:
		return monome_led_frame(monome, argv[0]->i, argv[1]->i, buf);
	}

	return -1;
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

	METHOD("clear") {
		REGISTER("", osc_clear_handler);
		REGISTER("i", osc_clear_handler);
	}

	METHOD("intensity") {
		REGISTER("", osc_intensity_handler);
		REGISTER("i", osc_intensity_handler);
	}

	METHOD("led")
		REGISTER("iii", osc_led_handler);

	METHOD("led_row") {
		REGISTER("ii", osc_led_row_handler);
		REGISTER("iii", osc_led_row_handler);
	}

	METHOD("led_col") {
		REGISTER("ii", osc_led_col_handler);
		REGISTER("iii", osc_led_col_handler);
	}

	METHOD("frame") {
		REGISTER("iiiiiiii", osc_frame_handler);
		REGISTER("iiiiiiiiii", osc_frame_handler);
	}

#undef REGISTER
}

void osc_unregister_methods(sosc_state_t *state) {
	char *prefix, *cmd_buf;
	monome_t *monome;
	lo_server srv;

	prefix = state->config.app.osc_prefix;
	monome = state->monome;
	srv = state->server;

#define UNREGISTER(typetags) \
	lo_server_del_method(srv, cmd_buf, typetags)

	METHOD("clear") {
		UNREGISTER("");
		UNREGISTER("i");
	}

	METHOD("intensity") {
		UNREGISTER("");
		UNREGISTER("i");
	}

	METHOD("led")
		UNREGISTER("iii");

	METHOD("led_row") {
		UNREGISTER("ii");
		UNREGISTER("iii");
	}

	METHOD("led_col") {
		UNREGISTER("ii");
		UNREGISTER("iii");
	}

	METHOD("frame") {
		UNREGISTER("iiiiiiii");
		UNREGISTER("iiiiiiiiii");
	}

#undef UNREGISTER
}

#undef METHOD
