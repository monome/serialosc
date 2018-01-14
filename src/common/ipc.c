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

#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <serialosc/serialosc.h>
#include <serialosc/ipc.h>

#define IPC_MAGIC 0x505C /* SOSC, get it? */

/*************************************************************************
 * i/o from file descriptors
 *************************************************************************/

static int
read_strdata(int fd, size_t n, ...)
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

		if (read(fd, &slen, sizeof(slen)) < sizeof(slen))
			return -1;

		if (!(buf = s_calloc(slen + 1, sizeof(char)))) {
			/* XXX: proper error handling? i.e. what do we do with the
			        strings that are already allocated? we can't NULL
			        all of them at the start, I guess that's up to the
					caller? */
			continue;
		}

		if (read(fd, buf, slen) < slen) {
			s_free(buf);
			return -1;
		}

		*cur = buf;

		if (read(fd, &magic, sizeof(magic)) < sizeof(magic)
			|| magic != IPC_MAGIC)
			return -1;
	}

	va_end(ap);

	return 0;
}

int
sosc_ipc_msg_write(int fd, sosc_ipc_msg_t *msg)
{
	uint8_t buf[SOSC_IPC_MSG_BUFFER_SIZE];
	ssize_t written;
	ssize_t bufsiz;

	msg->magic = IPC_MAGIC;

	bufsiz = sosc_ipc_msg_to_buf(buf, sizeof(buf), msg);

	if (bufsiz < 0) {
		fprintf(stderr, "[-] couldn't serialize msg\n");
		return -1;
	}

	if ((written = write(fd, buf, bufsiz)) < bufsiz)
		return -1;

	return written;
}

int
sosc_ipc_msg_read(int fd, sosc_ipc_msg_t *buf)
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

/*************************************************************************
 * serializing to and from buffers
 *************************************************************************/

static ssize_t
strdata_to_buf(uint8_t *buf, size_t nbytes, size_t n, ...)
{
	uint16_t magic = IPC_MAGIC;
	const char *cur;
	ssize_t avail;
	size_t slen;
	va_list ap;

#define BUF_WALK(bytes)  \
	do {                 \
		buf += bytes;    \
		avail -= bytes; \
		                 \
		if (avail < 0)  \
			return -1;   \
	} while (0)

#define EMIT_BYTES(b, nb)    \
	({                       \
		uint8_t *obuf = buf; \
		BUF_WALK(nb);        \
		memcpy(obuf, b, nb); \
	})

#define EMIT_VAR(v) EMIT_BYTES(&v, sizeof(v))

	avail = nbytes;
	va_start(ap, n);

	while (n--) {
		cur = va_arg(ap, const char *);
		slen = strlen(cur);

		EMIT_VAR(slen);
		EMIT_BYTES(cur, slen);
		EMIT_VAR(magic);
	}

	va_end(ap);

#undef EMIT_VAR
#undef EMIT_BYTES
#undef BUF_WALK

	return nbytes - avail;
}

ssize_t
sosc_ipc_msg_to_buf(uint8_t *buf, size_t nbytes, sosc_ipc_msg_t *msg)
{
	ssize_t avail, strbytes;

	if (nbytes < sizeof(*msg))
		return -1;

	avail = nbytes;

	msg->magic = IPC_MAGIC;
	memcpy(buf, msg, sizeof(*msg));

	buf += sizeof(*msg);
	avail -= sizeof(*msg);

	switch (msg->type) {
	case SOSC_DEVICE_CONNECTION:
		strbytes = strdata_to_buf(buf, nbytes, 1, msg->connection.devnode);

		if (strbytes < 0)
			return -1;
		break;

	case SOSC_DEVICE_INFO:
		strbytes = strdata_to_buf(buf, nbytes, 2, msg->device_info.serial,
								  msg->device_info.friendly);

		if (strbytes < 0)
			return -1;
		break;

	case SOSC_DEVICE_READY:
	case SOSC_DEVICE_DISCONNECTION:
	case SOSC_OSC_PORT_CHANGE:
	case SOSC_PROCESS_SHOULD_EXIT:
		strbytes = 0;
		break;

	default:
		return -1;
	}

	avail -= strbytes;
	assert(avail >= 0);

	return nbytes - avail;
}

static ssize_t
strdata_from_buf(uint8_t *buf, size_t nbytes, size_t n, ...)
{
	const char **cur;
	uint16_t magic;
	ssize_t avail;
	size_t slen;
	va_list ap;

#define BUF_WALK(bytes)  \
	do {                 \
		buf += bytes;    \
		avail -= bytes; \
		                 \
		if (avail < 0)  \
			return -1;   \
	} while (0)

#define CONSUME_BYTES(nb)     \
	({                        \
		uint8_t *obuf = buf;  \
		BUF_WALK(nb);         \
		obuf;                 \
	})

#define CONSUME_TYPE(type)           \
	*((type *) ({                    \
		CONSUME_BYTES(sizeof(type)); \
	}))

	avail = nbytes;
	va_start(ap, n);

	while (n--) {
		cur = va_arg(ap, const char **);
		*cur = NULL;

		slen  = CONSUME_TYPE(size_t);
		*cur  = (char *) CONSUME_BYTES(slen);
		magic = CONSUME_TYPE(uint16_t);

		if (magic != IPC_MAGIC) {
			*cur = NULL;
			return -1;
		}

		/* null-terminate the string (this is a little hack) */
		*(buf - 2) = '\0';
		*cur = s_strdup(*cur);
	}

	va_end(ap);

#undef CONSUME_TYPE
#undef CONSUME_BYTES
#undef BUF_WALK

	return nbytes - avail;
}

ssize_t
sosc_ipc_msg_from_buf(uint8_t *buf, size_t nbytes, sosc_ipc_msg_t **msg)
{
	ssize_t avail, strbytes;
	*msg = (sosc_ipc_msg_t *) buf;

	if (nbytes < sizeof(**msg) || (*msg)->magic != IPC_MAGIC)
		goto invalid_msg;

	avail = nbytes;

	buf += sizeof(**msg);
	avail -= sizeof(**msg);

	switch ((*msg)->type) {
	case SOSC_DEVICE_CONNECTION:
		(*msg)->connection.devnode = NULL;

		strbytes = strdata_from_buf(
			buf, nbytes, 1,
			&(*msg)->connection.devnode);

		if (strbytes < 0)
			goto invalid_msg;
		break;

	case SOSC_DEVICE_INFO:
		(*msg)->device_info.serial = (*msg)->device_info.friendly = NULL;

		strbytes = strdata_from_buf(
			buf, nbytes, 2,
			&(*msg)->device_info.serial,
			&(*msg)->device_info.friendly);

		if (strbytes < 0)
			goto invalid_msg;
		break;

	case SOSC_DEVICE_READY:
	case SOSC_DEVICE_DISCONNECTION:
	case SOSC_OSC_PORT_CHANGE:
	case SOSC_PROCESS_SHOULD_EXIT:
		strbytes = 0;
		break;

	default:
		return -1;
	}

	avail -= strbytes;
	return nbytes - avail;

invalid_msg:
	*msg = NULL;
	return -1;
}
