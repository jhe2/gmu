/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: net.h  Created: 121005
 *
 * Description: Network helper functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#ifndef WEJ_NET_H
#define WEJ_NET_H
/* Send data of specified length 'size' to the client socket */
int net_send_block(int sock, unsigned char *buf, int size);
/* Send a null-terminated string of arbitrary length to the client socket */
int net_send_buf(int sock, char *buf);
#endif
