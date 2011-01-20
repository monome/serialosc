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
#include <time.h>
#include <arpa/inet.h>

#include <lo/lo.h>
#include <dns_sd.h>
#include <monome.h>

#include "serialosc.h"
#include "osc.h"


#define DEFAULT_OSC_PREFIX      "/monome"
#define DEFAULT_OSC_SERVER_PORT NULL
#define DEFAULT_OSC_APP_PORT    "8000"
#define DEFAULT_OSC_APP_HOST    "127.0.0.1"
#define DEFAULT_ROTATION        MONOME_ROTATE_0


static void lo_error(int num, const char *error_msg, const char *path) {
	fprintf(stderr, "serialosc: lo server error %d in %s: %s\n",
	        num, path, error_msg);
	fflush(stderr);
}

static const char *null_if_zero(const char *s) {
	if( !*s )
		return NULL;
	return s;
}

static void handle_press(const monome_event_t *e, void *data) {
	sosc_state_t *state = data;
	char *cmd;

	cmd = osc_path("press", state->config.app.osc_prefix);
	lo_send_from(state->outgoing, state->server, LO_TT_IMMEDIATE, cmd, "iii",
	             e->x, e->y, e->event_type);
	free(cmd);
}

static void mdns_callback(DNSServiceRef sdRef, DNSServiceFlags flags,
                   DNSServiceErrorType errorCode, const char *name,
                   const char *regtype, const char *domain, void *context) {

	/* on OSX, the bonjour library insists on having a callback passed to
	   DNSServiceRegister. */

	return;

}

void server_run(monome_t *monome) {
	sosc_state_t state = { .monome = monome };

	if( sosc_config_read(monome_get_serial(state.monome), &state.config) ) {
		fprintf(
			stderr, "serialosc [%s]: couldn't read config, using defaults\n",
			monome_get_serial(state.monome));
	}

	if( !(state.server = lo_server_new(null_if_zero(state.config.server.port),
									   lo_error)) )
		goto err_server_new;

	if( !(state.outgoing = lo_address_new(
				state.config.app.host, null_if_zero(state.config.app.port))) ) {
		fprintf(
			stderr, "serialosc [%s]: couldn't allocate lo_address, aieee!\n",
			monome_get_serial(state.monome));
		goto err_lo_addr;
	}

	DNSServiceRegister(
		/* sdref          */  &state.ref,
		/* interfaceIndex */  0,
		/* flags          */  0,
		/* name           */  monome_get_serial(state.monome),
		/* regtype        */  "_monome-osc._udp",
		/* domain         */  NULL,
		/* host           */  NULL,
		/* port           */  htons(lo_server_get_port(state.server)),
		/* txtLen         */  0,
		/* txtRecord      */  NULL,
		/* callBack       */  mdns_callback,
		/* context        */  NULL);

	monome_register_handler(state.monome, MONOME_BUTTON_DOWN,
	                        handle_press, &state);
	monome_register_handler(state.monome, MONOME_BUTTON_UP,
	                        handle_press, &state);

	monome_set_rotation(state.monome, state.config.dev.rotation);
	monome_clear(state.monome, MONOME_CLEAR_OFF);
	monome_mode(state.monome, MONOME_MODE_NORMAL);

	osc_register_sys_methods(&state);
	osc_register_methods(&state);

	printf("serialosc [%s]: connected, server running on port %d\n",
	       monome_get_serial(state.monome), lo_server_get_port(state.server));

	event_loop(&state);

	printf("serialosc [%s]: disconnected, exiting\n",
	       monome_get_serial(state.monome));

	DNSServiceRefDeallocate(state.ref);

	lo_address_free(state.outgoing);

	/* set configuration parameters from server params */
	sosc_port_itos(state.config.server.port, lo_server_get_port(state.server));
	strncpy(state.config.app.port, lo_address_get_port(state.outgoing), 6);
	state.config.app.port[5] = '\0';

	if( sosc_config_write(monome_get_serial(state.monome), &state.config) ) {
		fprintf(
			stderr, "serialosc [%s]: couldn't write config :(\n",
			monome_get_serial(state.monome));
	}

err_lo_addr:
	lo_server_free(state.server);
err_server_new:
	free(state.config.app.osc_prefix);
	free(state.config.app.host);
}
