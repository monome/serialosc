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

#include "serialosc.h"
#include "ipc.h"

#define IPC_MAGIC 0x505C

int sosc_ipc_msg_write(int fd, sosc_ipc_msg_t *msg)
{
	ssize_t written;

	msg->magic = IPC_MAGIC;

	if ((written = write(fd, msg, sizeof(*msg))) < sizeof(*msg))
		return -1;

	switch (msg->type) {
	case SOSC_DEVICE_CONNECTION:
		if( write(fd, msg->connection.devnode, msg->connection.devnode_len)
		    < msg->connection.devnode_len)
			return -1; /* sorry player. not much we can do. */

	default:
		break;
	}

	return written;
}

int sosc_ipc_msg_read(int fd, sosc_ipc_msg_t *buf)
{
	ssize_t nbytes;
	char *nodebuf;

	if ((nbytes = read(fd, buf, sizeof(*buf))) < sizeof(*buf)
		|| buf->magic != IPC_MAGIC)
		return -1;

	switch (buf->type) {
	case SOSC_DEVICE_CONNECTION:
		nodebuf = s_calloc(buf->connection.devnode_len + 1, sizeof(char));

		if (read(fd, nodebuf, buf->connection.devnode_len)
			< buf->connection.devnode_len) {
			s_free(nodebuf);

			buf->connection.devnode = NULL;
			buf->connection.devnode_len = 0;

			return -1;
		}

		nodebuf[buf->connection.devnode_len] = 0;
		buf->connection.devnode = nodebuf;
		nbytes += buf->connection.devnode_len;

	default:
		break;
	}

	return nbytes;
}
