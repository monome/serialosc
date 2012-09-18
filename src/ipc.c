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

#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "serialosc.h"
#include "ipc.h"

#define IPC_MAGIC 0x505C /* SOSC, get it? */

static int read_strdata(int fd, size_t n, ...)
{
	const char **cur;
	uint16_t magic;
	size_t slen;
	va_list ap;
	char *buf;

	va_start(ap, n);

	while (n--) {
		cur = va_arg(ap, const char **);
		*cur = NULL;

		read(fd, &slen, sizeof(slen));
		if (!(buf = s_calloc(slen + 1, sizeof(char)))) {
			/* XXX: proper error handling? i.e. what do we do with the 
			        strings that are already allocated? we can't NULL
			        all of them at the start, I guess that's up to the
					caller? */
			continue;
		}

		if (read(fd, buf, slen) < slen) {
			s_free(buf);
			return 1;
		}

		*cur = buf;

		if (read(fd, &magic, sizeof(magic)) < sizeof(magic)
			|| magic != IPC_MAGIC)
			return 1;
	}

	va_end(ap);

	return 0;
}

static int write_strdata(int fd, size_t n, ...)
{
	uint16_t magic = IPC_MAGIC;
	const char *cur;
	size_t slen;
	va_list ap;

	va_start(ap, n);
	while (n--) {
		cur = va_arg(ap, const char *);
		slen = strlen(cur);

		write(fd, &slen, sizeof(slen));
		write(fd, cur, slen);
		write(fd, &magic, sizeof(magic));
	}
	va_end(ap);

	return 0;
}

int sosc_ipc_msg_write(int fd, sosc_ipc_msg_t *msg)
{
	ssize_t written;

	msg->magic = IPC_MAGIC;

	if ((written = write(fd, msg, sizeof(*msg))) < sizeof(*msg))
		return -1;

	switch (msg->type) {
	case SOSC_DEVICE_CONNECTION:
		if (write_strdata(fd, 1, msg->connection.devnode))
			return -1;

		break;

	case SOSC_DEVICE_INFO:
		if (write_strdata(fd, 2, msg->device_info.serial,
		                  msg->device_info.friendly))
			return -1;

	default:
		break;
	}

	return written;
}

int sosc_ipc_msg_read(int fd, sosc_ipc_msg_t *buf)
{
	ssize_t nbytes;

	if ((nbytes = read(fd, buf, sizeof(*buf))) < sizeof(*buf)
		|| buf->magic != IPC_MAGIC)
		return -1;

	switch (buf->type) {
	case SOSC_DEVICE_CONNECTION:
		buf->connection.devnode = NULL;
		if (read_strdata(fd, 1, &buf->connection.devnode))
			return -1;

		break;

	case SOSC_DEVICE_INFO:
		buf->device_info.serial = buf->device_info.friendly = NULL;

		if (read_strdata(fd, 2, &buf->device_info.serial,
		                 &buf->device_info.friendly))
			return -1;

	default:
		break;
	}

	return nbytes;
}

static int strdata_from_buf(uint8_t *buf, size_t nbytes, size_t n, ...)
{
	const char **cur;
	uint16_t magic;
	size_t slen;
	va_list ap;

	va_start(ap, n);

#define BUF_WALK(bytes)  \
	do {                 \
		buf += bytes;    \
		nbytes -= bytes; \
		                 \
		if (nbytes < 0)  \
			return 1;    \
	} while (0);

	while (n--) {
		cur = va_arg(ap, const char **);
		*cur = NULL;

		slen = *((size_t *) buf);

		BUF_WALK(sizeof(slen));
		*cur = (const char *) buf;

		BUF_WALK(slen);
		magic = *((uint16_t *) buf);
		if (magic != IPC_MAGIC) {
			*cur = NULL;
			return 1;
		}

		/* null-terminate the string (this is a little hack) */
		*buf = '\0';
		BUF_WALK(magic);
	}

	va_end(ap);

	return 0;

}

int sosc_ipc_msg_from_buf(uint8_t *buf, size_t nbytes, sosc_ipc_msg_t **msg)
{
	*msg = (sosc_ipc_msg_t *) buf;

	if (nbytes < sizeof(**msg) || (*msg)->magic != IPC_MAGIC)
		goto invalid_msg;

	buf += sizeof(**msg);
	nbytes -= sizeof(**msg);

	switch ((*msg)->type) {
	case SOSC_DEVICE_CONNECTION:
		(*msg)->connection.devnode = NULL;
		if (strdata_from_buf(buf, nbytes, 1, &(*msg)->connection.devnode))
			goto invalid_msg;

		break;

	case SOSC_DEVICE_INFO:
		(*msg)->device_info.serial = (*msg)->device_info.friendly = NULL;

		if (strdata_from_buf(buf, nbytes, 2, &(*msg)->device_info.serial,
		                 &(*msg)->device_info.friendly))
			goto invalid_msg;

	default:
		break;
	}

	return 0;

invalid_msg:
	*msg = NULL;
	return -1;
}
