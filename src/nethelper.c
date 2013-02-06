/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
 *
 * File: nethelper.c  Created: 130202
 *
 * Description: Helper functions for network communication
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "debug.h"
#include "nethelper.h"

/* Returns a file descriptor on success, or 0 or a negative number otherwise */
int nethelper_tcp_connect_to_host(char *hostname, int port, int flags)
{
	int fd = -1;
	if (hostname && port > 0) {
		struct addrinfo hints, *servinfo, *p;
		int    rv;
		char   port_str[6];

		snprintf(port_str, 6, "%d", port);
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		if ((rv = getaddrinfo(hostname, port_str, &hints, &servinfo)) != 0) {
			wdprintf(V_ERROR, "nethelper",  "getaddrinfo: %s\n", gai_strerror(rv));
		} else {
			/* loop through all the results and connect to the first we can */
			for (p = servinfo; p != NULL; p = p->ai_next) {
				int cur_flags;
				if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
					wdprintf(V_INFO, "nethelper", "socket: %s\n", strerror(errno));
					continue;
				} else { /* Set socket timeout to 2 seconds */
					struct timeval tv;
					tv.tv_sec = 2;
					tv.tv_usec = 0;
					if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof tv)) {
						wdprintf(V_INFO, "nethelper", "setsockopt: %s\n", strerror(errno));
					}
				}

				cur_flags = fcntl(fd, F_GETFL, 0);
				if (flags != 0) fcntl(fd, F_SETFL, cur_flags | flags);
				if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
					wdprintf(V_INFO, "nethelper", "connect: %s\n", strerror(errno));
					if (errno == EINPROGRESS) {
						break;
					} else {
						close(fd);
						fd = -1;
					}
					continue;
				}
				break;
			}
			freeaddrinfo(servinfo);
		}
	}
	return fd;
}
