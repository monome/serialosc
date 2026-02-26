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

#include <windows.h>
#include <windns.h>
#include <strsafe.h>

#include <serialosc/serialosc.h>

static void WINAPI
register_complete(DWORD status, PVOID context, PDNS_SERVICE_INSTANCE instance)
{
	if (status != ERROR_SUCCESS) {
		fprintf(stderr, "sosc_zeroconf_register(): DnsServiceRegister failed (%lu)\n", status);
	}
}

static void WINAPI
deregister_complete(DWORD status, PVOID context, PDNS_SERVICE_INSTANCE instance)
{
	if (status != ERROR_SUCCESS) {
		fprintf(stderr, "sosc_zeroconf_unregister(): DnsServiceDeRegister failed (%lu)\n", status);
	}
}

void
sosc_zeroconf_init(void)
{
	/* dnsapi.dll is linked at build time, no library to load */
	return;
}

void
sosc_zeroconf_register(sosc_state_t *state, const char *svc_name)
{
	PDNS_SERVICE_INSTANCE instance;
	WCHAR w_service_name[256];
	WCHAR w_hostname[256];
	DWORD w_hostname_len = ARRAYSIZE(w_hostname);
	int port;

	port = lo_server_get_port(state->server);

	MultiByteToWideChar(CP_UTF8, 0, svc_name, -1, w_service_name, ARRAYSIZE(w_service_name));

	StringCchCatW(w_service_name, ARRAYSIZE(w_service_name), L"._monome-osc._udp.local");

	if (!GetComputerNameExW(ComputerNameDnsHostname, w_hostname, &w_hostname_len)) {
		fprintf(stderr, "sosc_zeroconf_register(): GetComputerNameExW failed\n");
		return;
	}

	StringCchCatW(w_hostname, ARRAYSIZE(w_hostname), L".local");

	instance = DnsServiceConstructInstance(
		w_service_name,
		w_hostname,
		NULL, /* ipv4 */
		NULL, /* ipv6 */
		(WORD) port,
		0,    /* priority */
		0,    /* weight */
		0,    /* properties count */
		NULL, /* keys */
		NULL  /* values */
	);

	if (!instance) {
		fprintf(stderr, "sosc_zeroconf_register(): DnsServiceConstructInstance failed\n");
		return;
	}

	DNS_SERVICE_REGISTER_REQUEST request = {
		.Version = DNS_QUERY_REQUEST_VERSION1,
		.pServiceInstance = instance,
		.pRegisterCompletionCallback = register_complete,
	};

	if (DnsServiceRegister(&request, NULL) != DNS_REQUEST_PENDING) {
		fprintf(stderr, "sosc_zeroconf_register(): DnsServiceRegister failed\n");
		DnsServiceFreeInstance(instance);
		return;
	}

	state->dnssd_service_ref = instance;
}

void
sosc_zeroconf_unregister(sosc_state_t *state)
{
	PDNS_SERVICE_INSTANCE instance = state->dnssd_service_ref;

	if (!instance) {
		return;
	}

	DNS_SERVICE_REGISTER_REQUEST request = {
		.Version = DNS_QUERY_REQUEST_VERSION1,
		.pServiceInstance = instance,
		.pRegisterCompletionCallback = deregister_complete,
	};

	DnsServiceDeRegister(&request, NULL);

	DnsServiceFreeInstance(instance);
	state->dnssd_service_ref = NULL;
}
