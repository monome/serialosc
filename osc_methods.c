/**
 * Copyright (c) 2010 William Light <will@illest.net>
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
#include <unistd.h>

#include <lo/lo.h>
#include <monome.h>

#include "serialosc.h"
#include "osc.h"


static int osc_clear_handler(const char *path, const char *types,
							 lo_arg **argv, int argc,
							 lo_message data, void *user_data) {
	monome_t *monome = user_data;
	int mode = (argc) ? argv[0]->i : 0;

	return monome_clear(monome, mode);
}

static int osc_intensity_handler(const char *path, const char *types,
								 lo_arg **argv, int argc,
								 lo_message data, void *user_data) {
	monome_t *monome = user_data;
	int intensity = (argc) ? argv[0]->i : 0xF;

	return monome_intensity(monome, intensity);
}

static int osc_led_handler(const char *path, const char *types,
						   lo_arg **argv, int argc,
						   lo_message data, void *user_data) {
	monome_t *monome = user_data;

	if( argv[2]->i )
		return monome_led_on(monome, argv[0]->i, argv[1]->i);
	else
		return monome_led_off(monome, argv[0]->i, argv[1]->i);
}

static int osc_led_col_handler(const char *path, const char *types,
								   lo_arg **argv, int argc,
								   lo_message data, void *user_data) {
	monome_t *monome = user_data;
	uint8_t buf[2] = {argv[1]->i};

	if( argc == 3 )
		buf[1] = argv[2]->i;

	return monome_led_col(monome, argv[0]->i, argc - 1, buf);
}

static int osc_led_row_handler(const char *path, const char *types,
								   lo_arg **argv, int argc,
								   lo_message data, void *user_data) {
	monome_t *monome = user_data;
	uint8_t buf[2] = {argv[1]->i};

	if( argc == 3 )
		buf[1] = argv[2]->i;

	return monome_led_row(monome, argv[0]->i, argc - 1, buf);
}

static int osc_frame_handler(const char *path, const char *types,
							 lo_arg **argv, int argc,
							 lo_message data, void *user_data) {
	monome_t *monome = user_data;
	uint8_t buf[8];
	uint i;

	for( i = 0; i < 8; i++ )
		buf[i] = argv[i]->i;

	switch( argc ) {
	case 8:
		return monome_led_frame(monome, 0, buf);
		break;

	case 9:
		return monome_led_frame(monome, argv[8]->i, buf);
		break;

	case 10:
		/* offsets are unimplemented */
	default:
		break;
	}

	return -1;
}


char *osc_path(const char *path, const char *prefix) {
	char *buf;

	if( asprintf(&buf, "%s/%s", prefix, path) < 0 ) {
		fprintf(stderr, "aieee, could not allocate memory in "
				"osc_path(), bailing out!\n");
		_exit(EXIT_FAILURE);
	}

	return buf;
}

void osc_register_methods(sosc_state_t *state) {
	char *prefix, *cmd_buf;
	monome_t *monome;
	lo_server srv;

	prefix = state->osc_prefix;
	monome = state->monome;
	srv = state->server;

#define METHOD(path) for( cmd_buf = osc_path(path, prefix); cmd_buf; \
						  free(cmd_buf), cmd_buf = NULL )

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
		REGISTER("iiiiiiiii", osc_frame_handler);
	}

#undef REGISTER
#undef METHOD
}
