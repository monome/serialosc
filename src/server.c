/**
 * Copyright (c) 2010-2011 William Light <wrl@illest.net>
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
#include <unistd.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <arpa/inet.h>
#else
#include <Winsock2.h>
#endif

#include <lo/lo.h>
#include <dns_sd.h>
#include <monome.h>

#include "serialosc.h"
#include "osc.h"
#include "ipc.h"


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

	cmd = osc_path("grid/key", state->config.app.osc_prefix);
	lo_send_from(state->outgoing, state->server, LO_TT_IMMEDIATE, cmd, "iii",
	             e->grid.x, e->grid.y, e->event_type == MONOME_BUTTON_DOWN);
	s_free(cmd);
}

static void handle_enc_delta(const monome_event_t *e, void *data) {
	sosc_state_t *state = data;
	char *cmd;

	cmd = osc_path("enc/delta", state->config.app.osc_prefix);
	lo_send_from(state->outgoing, state->server, LO_TT_IMMEDIATE, cmd, "ii",
	             e->encoder.number, e->encoder.delta);
	s_free(cmd);
}

static void handle_enc_key(const monome_event_t *e, void *data) {
	sosc_state_t *state = data;
	char *cmd;

	cmd = osc_path("enc/key", state->config.app.osc_prefix);
	lo_send_from(state->outgoing, state->server, LO_TT_IMMEDIATE, cmd, "ii",
	             e->encoder.number, e->event_type == MONOME_ENCODER_KEY_DOWN);
	s_free(cmd);
}

static void handle_tilt(const monome_event_t *e, void *data) {
	sosc_state_t *state = data;
	char *cmd;

	cmd = osc_path("tilt", state->config.app.osc_prefix);
	lo_send_from(state->outgoing, state->server, LO_TT_IMMEDIATE, cmd, "iiii",
	             e->tilt.sensor, e->tilt.x, e->tilt.y, e->tilt.z);
	s_free(cmd);
}

static void send_connection_status(sosc_state_t *state, int status) {
	char *cmd, *cmds[] = {
		"/sys/disconnect",
		"/sys/connect"
	};

	cmd = cmds[status & 1];
	lo_send_from(state->outgoing, state->server, LO_TT_IMMEDIATE, cmd, "");
}

static void send_device_info(int fd, monome_t *monome)
{
	sosc_ipc_msg_t msg = {
		.type = SOSC_DEVICE_INFO,
		.device_info = {
			.serial = (char *) monome_get_serial(monome),
			.friendly = (char *) monome_get_friendly_name(monome)
		}
	};

	sosc_ipc_msg_write(fd, &msg);
}

static void send_osc_port_change(int fd, uint16_t port)
{
	sosc_ipc_msg_t msg = {
		.type = SOSC_OSC_PORT_CHANGE,
		.port_change = {
			.port = port
		}
	};

	sosc_ipc_msg_write(fd, &msg);
}

static void send_simple_ipc(int fd, sosc_ipc_type_t type)
{
	sosc_ipc_msg_t msg = {
		.type = type
	};

	sosc_ipc_msg_write(fd, &msg);
}

void sosc_server_run(monome_t *monome)
{
	sosc_state_t state = {
		.monome = monome,
		.ipc_fd = (!isatty(STDOUT_FILENO)) ? STDOUT_FILENO : -1
	};

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

#define HANDLE(ev, cb) monome_register_handler(state.monome, ev, cb, &state)
	HANDLE(MONOME_BUTTON_DOWN, handle_press);
	HANDLE(MONOME_BUTTON_UP, handle_press);
	HANDLE(MONOME_ENCODER_DELTA, handle_enc_delta);
	HANDLE(MONOME_ENCODER_KEY_DOWN, handle_enc_key);
	HANDLE(MONOME_ENCODER_KEY_UP, handle_enc_key);
	HANDLE(MONOME_TILT, handle_tilt);
#undef HANDLE

	monome_set_rotation(state.monome, state.config.dev.rotation);
	monome_led_all(state.monome, 0);
	monome_mode(state.monome, MONOME_MODE_NORMAL);

	osc_register_sys_methods(&state);
	osc_register_methods(&state);

	if (state.ipc_fd < 0) {
		fprintf(
			stderr, "serialosc [%s]: connected, server running on port %d\n",
			monome_get_serial(state.monome), lo_server_get_port(state.server));
	} else {
		send_device_info(state.ipc_fd, monome);
		send_osc_port_change(
			state.ipc_fd, lo_server_get_port(state.server));
		send_simple_ipc(state.ipc_fd, SOSC_DEVICE_READY);
	}

	send_connection_status(&state, 1);
	sosc_event_loop(&state);
	send_connection_status(&state, 0);

	if (state.ipc_fd < 0) {
		fprintf(stderr, "serialosc [%s]: disconnected, exiting\n",
				monome_get_serial(state.monome));
	} else
		send_simple_ipc(state.ipc_fd, SOSC_DEVICE_DISCONNECTION);

	if( sosc_config_write(monome_get_serial(state.monome), &state) ) {
		fprintf(
			stderr, "serialosc [%s]: couldn't write config :(\n",
			monome_get_serial(state.monome));
	}

	lo_address_free(state.outgoing);
err_lo_addr:
	lo_server_free(state.server);
err_server_new:
	s_free(state.config.app.osc_prefix);
	s_free(state.config.app.host);
}
