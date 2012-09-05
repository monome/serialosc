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

#include <dlfcn.h>
#include <stdio.h>

#include "serialosc.h"
#include "zeroconf.h"

union reg_func {
	dnssd_registration_func_t fptr;
	void *vptr;
};

union dealloc_func {
	dnssd_deallocation_func_t fptr;
	void *vptr;
};

void sosc_zeroconf_init()
{
	union dealloc_func dfunc;
	union reg_func rfunc;
	void *ldnssd;

	if (!(ldnssd = dlopen("libdns_sd.so", RTLD_LAZY))) {
		fprintf(stderr, "sosc_zeroconf_init(): couldn't load libdns_sd.so\n");
		return;
	}

	rfunc.vptr = dlsym(ldnssd, "DNSServiceRegister");
	dfunc.vptr = dlsym(ldnssd, "DNSServiceRefDeallocate");

	if (!rfunc.vptr || !dfunc.vptr) {
		fprintf(stderr, "sosc_zeroconf_init(): couldn't resolve symbols in libdns_sd.so\n");
		dlclose(ldnssd);
		return;
	}

	sosc_dnssd_registration_func = rfunc.fptr;
	sosc_dnssd_deallocation_func = dfunc.fptr;
}
