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
#include <arpa/inet.h>

#include <lo/lo.h>
#include <dns_sd.h>
#include <monome.h>

#include "serialosc.h"
#include "monitor.h"
#include "osc.h"


#define DEFAULT_OSC_PREFIX      "/monome"
#define DEFAULT_OSC_SERVER_PORT NULL
#define DEFAULT_OSC_APP_PORT    "8000"
#define DEFAULT_OSC_APP_HOST    "127.0.0.1"
#define DEFAULT_ROTATION        MONOME_CABLE_LEFT


static void lo_error(int num, const char *error_msg, const char *path) {
	fprintf(stderr, "serialosc: lo server error %d in %s: %s\n",
	        num, path, error_msg);
	fflush(stderr);
}

static void handle_press(const monome_event_t *e, void *data) {
	sosc_state_t *state = data;
	char *cmd;

	cmd = osc_path("press", state->osc_prefix);
	lo_send_from(state->outgoing, state->server, LO_TT_IMMEDIATE, cmd, "iii",
	             e->x, e->y, e->event_type);
	free(cmd);
}

void router_process(monome_t *monome) {
	sosc_state_t state = { .monome = monome };

	if( !(state.server = lo_server_new(DEFAULT_OSC_SERVER_PORT, lo_error)) )
		return;

	if( !(state.outgoing = lo_address_new(DEFAULT_OSC_APP_HOST,
	                                      DEFAULT_OSC_APP_PORT)) ) {
		fprintf(
			stderr, "serialosc [%s]: couldn't allocate lo_address, aieee!\n",
			monome_get_serial(state.monome));
		goto err_lo_addr;
	}

	if( !(state.osc_prefix = strdup(DEFAULT_OSC_PREFIX)) ) {
		fprintf(
			stderr, "serialosc [%s]: can't strdup(), aieee!\n",
			monome_get_serial(state.monome));
		goto err_nomem;
	}

	DNSServiceRegister(
		&state.ref, 0, 0, monome_get_serial(state.monome), "_monome-osc._udp",
		NULL, NULL, htons(lo_server_get_port(state.server)), 0, NULL, NULL,
		NULL);

	monome_register_handler(state.monome, MONOME_BUTTON_DOWN,
	                        handle_press, &state);
	monome_register_handler(state.monome, MONOME_BUTTON_UP,
	                        handle_press, &state);

	monome_set_orientation(state.monome, DEFAULT_ROTATION);
	monome_clear(state.monome, MONOME_CLEAR_OFF);
	monome_mode(state.monome, MONOME_MODE_NORMAL);

	osc_register_sys_methods(&state);
	osc_register_methods(&state);

	printf("serialosc [%s]: connected, server running on port %d\n",
	       monome_get_serial(state.monome), lo_server_get_port(state.server));

	osc_event_loop(&state);

	printf("serialosc [%s]: disconnected, exiting\n",
	       monome_get_serial(state.monome));

	DNSServiceRefDeallocate(state.ref);

	free(state.osc_prefix);
err_nomem:
	lo_address_free(state.outgoing);
err_lo_addr:
	lo_server_free(state.server);
}

int main(int argc, char **argv) {
	monome_t *device;

	/* next_device() either returns on failure or in a subprocess */
	if( !(device = next_device()) )
		exit(EXIT_FAILURE);

	setenv("AVAHI_COMPAT_NOWARN", "shut up", 1);
	router_process(device);
	monome_close(device);

	return EXIT_SUCCESS;
}
