/**
 * Copyright (c) 2010-2015 William Light <wrl@illest.net>
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

#pragma once

#ifdef SOSC_ZEROCONF
#ifdef _WIN32
#include <windows.h>
#include <windns.h>
#else
#include <dns_sd.h>
#endif
#endif

#include <lo/lo.h>
#include <monome.h>

#include <serialosc/platform.h>

#define SOSC_SUPERVISOR_OSC_PORT "12002"
#define SOSC_WIN_SERVICE_NAME "serialosc"

#define container_of(ptr, type, member) \
	((type *) ((char *) (ptr) - offsetof(type, member)))

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

typedef struct sosc_state {
	int running;

	monome_t *monome;
	lo_address *outgoing;
	lo_server *server;

	int ipc_in_fd;
	int ipc_out_fd;

#ifdef SOSC_ZEROCONF
#ifdef _WIN32
	PDNS_SERVICE_INSTANCE dnssd_service_ref;
#else
	DNSServiceRef dnssd_service_ref;
#endif
#endif

	sosc_config_t config;
} sosc_state_t;

int  sosc_event_loop(struct sosc_state *state);
void sosc_server_run(const char *config_dir, monome_t *monome);

int sosc_config_create_directory();
int sosc_config_read(const char *config_dir, const char *serial, sosc_config_t *config);
int sosc_config_write(const char *config_dir, const char *serial, sosc_state_t *state);

void sosc_port_itos(char *dest, long int port);
size_t sosc_strlcpy(char *dst, const char *src, size_t size);

void sosc_zeroconf_init(void);
void sosc_zeroconf_register(sosc_state_t *state, const char *svc_name);
void sosc_zeroconf_unregister(sosc_state_t *state);
