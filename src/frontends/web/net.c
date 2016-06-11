/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: net.c  Created: 121005
 *
 * Description: Network helper functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include <errno.h>
#include "net.h"
#include "debug.h"

/*
 * Send data of specified length 'size' to the client socket
 * Returns 1 on success, 0 on error
 */
int net_send_block(int sock, const unsigned char *buf, size_t size)
{
	const unsigned char *r = buf;
	ssize_t              len = 0;
	int                  res = 1, tries = 3;

	while (size > 0 && tries > 0) {
		errno = 0;
		len = send(sock, r, size, 0);
		if (len == -1) {
			tries--;
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				struct pollfd ufds[1];
				int           rv;

				wdprintf(V_DEBUG, "net", "Socket %d does not accept more data. Retrying %d more time(s)...\n", sock, tries);
				ufds[0].fd = sock;
				ufds[0].events = POLLOUT;
				rv = poll(ufds, 1, 500);
				if (rv == -1) {
					res = 0;
					break;
				}
			} else {
				wdprintf(V_DEBUG, "net", "Error on socket %d: %s\n", sock, strerror(errno));
				res = 0;
				break;
			} 
		} else if (len > 0) {
			size -= len;
			r += len;
		} else {
			tries--;
		}
	}
	if (tries <= 0) res = 0;
	return res;
}

/*
 * Send a null-terminated string of arbitrary length to the client socket
 * Returns 1 on success, 0 on error
 */
int net_send_buf(int sock, const char *buf)
{
	const char *r = buf;
	ssize_t     len = 0;
	size_t      rlen = strlen(buf);

	if (sock) {
		while (rlen > 0) {
			if ((len = send(sock, r, strlen(r), 0)) == -1)
				return 0;
			rlen -= len;
			r += len;
		}
	}
	return 1;
}
