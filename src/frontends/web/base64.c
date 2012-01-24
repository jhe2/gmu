/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: base64.c  Created: 120118
 *
 * Description: base64
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <string.h>
#include "base64.h"

static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode_block(unsigned char in[3], char out[4], int len)
{
    out[0] = cb64[ in[0] >> 2 ];
    out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
    out[2] = (char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
    out[3] = (char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}

void base64_encode_data(unsigned char *in, int in_len, char *out, int out_max_len)
{
	int i, j, k, m;
	unsigned char in_buf[3];
	char out_buf[4];

	memset(out, 0, out_max_len);
	for (i = 0, j = 0, k = 0; i < in_len; i++, j++) {
		if (j >= 3) {
			j = 0;
			base64_encode_block(in_buf, out_buf, 3);
			memset(in_buf, 0, 3);
			for (m = 0; m < 4; m++) out[k+m < out_max_len ? k+m : 0] = out_buf[m];
			k+=4;
		}
		in_buf[j] = in[i];
	}
	base64_encode_block(in_buf, out_buf, in_len % 3);
	for (m = 0; m < 4; m++) out[k+m] = out_buf[m];
}
