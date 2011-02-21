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

#include <dns_sd.h>
#include <lo/lo.h>
#include <monome.h>

#ifndef SERIALOSC_H
#define SERIALOSC_H

#include "platform.h"

typedef struct {
	struct {
		char port[6];
	} server;

	struct {
		char *osc_prefix;
		char *host;
		char port[6];
	} app;

	struct {
		monome_rotate_t rotation;
	} dev;
} sosc_config_t;

typedef struct {
	monome_t *monome;
	lo_address *outgoing;
	lo_server *server;

	DNSServiceRef ref;

	sosc_config_t config;
} sosc_state_t;

int event_loop(const sosc_state_t *state);

int detector_run(const char *exec);
void server_run(monome_t *monome);

int sosc_config_create_directory();
int sosc_config_read(const char *serial, sosc_config_t *config);
int sosc_config_write(const char *serial, sosc_state_t *state);

void sosc_port_itos(char *dest, long int port);

#endif /* defined __SERIALOSC_H_ */
