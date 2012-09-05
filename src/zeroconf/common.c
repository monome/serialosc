/**
 * Copyright (c) 2010-2012 William Light <wrl@illest.net>
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
#include <dns_sd.h>

#include "serialosc.h"
#include "zeroconf.h"

dnssd_registration_func_t sosc_dnssd_registration_func = NULL;
dnssd_deallocation_func_t sosc_dnssd_deallocation_func = NULL;

static void DNSSD_API mdns_callback(DNSServiceRef sdRef, DNSServiceFlags flags,
                   DNSServiceErrorType errorCode, const char *name,
                   const char *regtype, const char *domain, void *context) {

	/* on OSX, the bonjour library insists on having a callback passed to
	   DNSServiceRegister. */

	return;
}

void sosc_zeroconf_register(sosc_state_t *state, const char *svc_name)
{
	if (!sosc_dnssd_registration_func)
		return;

	sosc_dnssd_registration_func(
		/* sdref          */  &state->ref,
		/* interfaceIndex */  0,
		/* flags          */  0,
		/* name           */  svc_name,
		/* regtype        */  "_monome-osc._udp",
		/* domain         */  NULL,
		/* host           */  NULL,
		/* port           */  htons(lo_server_get_port(state->server)),
		/* txtLen         */  0,
		/* txtRecord      */  NULL,
		/* callBack       */  mdns_callback,
		/* context        */  NULL);
}

void sosc_zeroconf_unregister(sosc_state_t *state)
{
	if (!sosc_dnssd_deallocation_func)
		return;

	sosc_dnssd_deallocation_func(state->ref);
}
