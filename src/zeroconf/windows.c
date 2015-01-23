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

#include <Windows.h>
#include <dns_sd.h>

#include "serialosc.h"
#include "zeroconf.h"

void
sosc_zeroconf_init(void)
{
	FARPROC rfunc, dfunc;
	HMODULE ldnssd;

	if (!(ldnssd = LoadLibrary("dnssd.dll"))) {
		fprintf(stderr, "sosc_zeroconf_init(): couldn't load dnssd.dll\n");
		return;
	}

	rfunc = GetProcAddress(ldnssd, "DNSServiceRegister");
	dfunc = GetProcAddress(ldnssd, "DNSServiceRefDeallocate");

	if (!rfunc || !dfunc) {
		fprintf(stderr, "sosc_zeroconf_init(): couldn't resolve symbols in dnssd.dll\n");
		FreeLibrary(ldnssd);
		return;
	}

	sosc_dnssd_registration_func = (dnssd_registration_func_t) rfunc;
	sosc_dnssd_deallocation_func = (dnssd_deallocation_func_t) dfunc;
}
