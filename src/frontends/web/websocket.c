/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: websocket.c  Created: 120930
 *
 * Description: Websocket functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "websocket.h"
#include "debug.h"

char *websocket_mask_unmask_message_alloc(char *msgbuf, int msgbuf_size)
{
	char *flags, *mask = NULL, *message, *unmasked_message = NULL;
	int   masked, len, i, offset = 0;

	flags  = msgbuf;
	len    = msgbuf[1] & 127;
	if (len == 126) {
		len = (((unsigned int)msgbuf[2]) << 8) + (unsigned char)msgbuf[3];
		offset += 2;
	} else if (len == 127) {
		len = -1; /* unsupported */
	}

	if (len > 0 && len < msgbuf_size) {
		masked = (msgbuf[1] & 128) ? 1 : 0;
		if (masked) mask = msgbuf + 2 + offset;
		message = msgbuf + 2 + (masked ? 4 : 0) + offset;
		wdprintf(V_DEBUG, "websocket", "websocket_unmask_message_alloc(): len=%d flags=%x masked=%d\n", len, flags[0], masked);
		if (masked) {
			unmasked_message = malloc(len+1);
			if (unmasked_message)  {
				for (i = 0; i < len; i++) {
					char c = message[i] ^ mask[i % 4];
					unmasked_message[i] = c;
				}
				unmasked_message[len] = '\0';
				wdprintf(V_DEBUG, "websocket", "websocket_unmask_message_alloc(): unmasked text='%s'\n", unmasked_message);
			}
		} else {
			wdprintf(V_DEBUG, "websocket", "websocket_unmask_message_alloc(): ERROR: Unmasked message: [%s]\n", message);
		}
	} else {
		wdprintf(V_DEBUG, "websocket", "websocket_unmask_message_alloc(): invalid length: %d (max. length: %d)\n", len, msgbuf_size);
	}
	return unmasked_message;
}
