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

#include <stdint.h>

#define PACKED __attribute__((__packed__))

typedef enum {
	SOSC_DEVICE_CONNECTION,
	SOSC_DEVICE_INFO,
	SOSC_DEVICE_DISCONNECTION,
	SOSC_OSC_PORT_CHANGE
} sosc_ipc_type_t;

typedef struct {
	sosc_ipc_type_t type;

	__extension__ union {
		struct {
			char *devnode;
		} PACKED connection;

		struct {
			char *serial;
			char *friendly;
		} PACKED device_info;

		struct {
			uint16_t port;
		} PACKED port_change;
	};

	uint16_t magic;
} PACKED sosc_ipc_msg_t;

int sosc_ipc_msg_write(int fd, sosc_ipc_msg_t *msg);
int sosc_ipc_msg_read(int fd, sosc_ipc_msg_t *buf);
