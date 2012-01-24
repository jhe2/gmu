/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: base64.h  Created: 120118
 *
 * Description: base64
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef GMU_BASE64_H
#define GMU_BASE64_H
void base64_encode_data(unsigned char *in, int in_len, char *out, int out_max_len);
#endif
