/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
#include "net.h"

/*
 * Send data of specified length 'size' to the client socket
 * Returns 1 on success, 0 on error
 */
int net_send_block(int sock, unsigned char *buf, int size)
{
	unsigned char *r = buf;
	int            len = 0;

	while (size > 0) {
		if ((len = send(sock, r, size, 0)) == -1)
			return 0;
		size -= len;
		r += len;
	}
	return 1;
}

/*
 * Send a null-terminated string of arbitrary length to the client socket
 * Returns 1 on success, 0 on error
 */
int net_send_buf(int sock, char *buf)
{
	char *r = buf;
	int   len = 0;
	int   rlen = strlen(buf);

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
