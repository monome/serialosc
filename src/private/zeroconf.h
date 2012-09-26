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

#include <dns_sd.h>

#include "serialosc.h"

typedef DNSServiceErrorType (DNSSD_API *dnssd_registration_func_t)
(
	DNSServiceRef                       *sdRef,
	DNSServiceFlags                     flags,
	uint32_t                            interfaceIndex,
	const char                          *name,         /* may be NULL */
	const char                          *regtype,
	const char                          *domain,       /* may be NULL */
	const char                          *host,         /* may be NULL */
	uint16_t                            port,
	uint16_t                            txtLen,
	const void                          *txtRecord,    /* may be NULL */
	DNSServiceRegisterReply             callBack,      /* may be NULL */
	void                                *context       /* may be NULL */
	);

typedef void (DNSSD_API *dnssd_deallocation_func_t)(DNSServiceRef sdRef);

/* declared in src/zeroconf/common.c */
extern dnssd_registration_func_t sosc_dnssd_registration_func;
extern dnssd_deallocation_func_t sosc_dnssd_deallocation_func;
