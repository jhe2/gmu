/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
 *
 * File: nethelper.h  Created: 130202
 *
 * Description: Helper functions for network communication
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef GMU_NETHELPER_H
#define GMU_NETHELPER_H

/* Returns a file descriptor on success, or 0 or a negative number otherwise */
int nethelper_tcp_connect_to_host(char *hostname, int port, int flags);
#endif
