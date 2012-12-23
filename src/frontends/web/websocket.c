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
#include <time.h>
#include "websocket.h"
#include "base64.h"
#include "net.h"
#include "debug.h"

static char *mask_message_alloc(char *message, int len, char *mask)
{
	int i;
	char *res = malloc(len+1);
	if (res) {
		for (i = 0; i < len; i++) {
			res[i] = message[i] ^ mask[i % 4];
		}
		res[len] = '\0';
	}
	return res;
}

char *websocket_unmask_message_alloc(char *msgbuf, int msgbuf_size)
{
	char *flags, *mask = NULL, *message, *unmasked_message = NULL;
	int   masked, len, offset = 0;

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
			unmasked_message = mask_message_alloc(message, len, mask);
			wdprintf(V_DEBUG, "websocket", "websocket_unmask_message_alloc(): unmasked text='%s'\n", unmasked_message);
		} else {
			wdprintf(V_DEBUG, "websocket", "websocket_unmask_message_alloc(): ERROR: Got unmasked message: [%s]\n", message);
		}
	} else {
		wdprintf(V_DEBUG, "websocket", "websocket_unmask_message_alloc(): invalid length: %d (max. length: %d)\n", len, msgbuf_size);
	}
	return unmasked_message;
}

char *websocket_client_generate_sec_websocket_key_alloc(void)
{
	unsigned char randdata[16];
	char *res = NULL;
	int  i;
	srand(time(NULL));
	for (i = 0; i < 16; i++) {
		randdata[i] = rand() % 256;
	}
	/* base64 encode random data... */
	res = malloc(30);
	if (res) {
		memset(res, 0, 30);
		base64_encode_data(randdata, 16, res, 22);
	}
	return res;
}

/* Returns 1 on success, 0 otherwise */
int websocket_send_str(int sock, char *str, int mask)
{
	int res = 0;
	if (str) {
		char *buf, mask_key[4];
		int   i, len = strlen(str);

		srand(time(NULL));
		for (i = 0; i < 4; i++) {
			mask_key[i] = rand() % 256;
		}

		buf = malloc(len+10); /* data length + 1 byte for flags + 9 bytes for length (1+8) */
		if (buf) {
			char *msg = str;
			if (mask) msg = mask_message_alloc(str, len, mask_key);
			memset(buf, 0, len);
			wdprintf(V_DEBUG, "websocket", "websocket_send_string(): len=%d str='%s' %d|%d\n", len, str, len >> 8, len & 0xFF);
			wdprintf(V_DEBUG, "websocket", "mask=%d masked_str='%s'\n", mask, msg);
			if (len <= 125) {
				if (mask) {
					snprintf(buf, len+10, "%c%c%c%c%c%c", 0x80+0x01, len+0x80,
					         mask_key[0], mask_key[1], mask_key[2], mask_key[3]);
					memcpy(buf+6, msg, len);
				} else {
					snprintf(buf, len+10, "%c%c%s", 0x80+0x01, len, msg);
				}
				res = net_send_block(sock, (unsigned char *)buf, len+2+(mask ? 4 : 0));
			} else if (len > 125 && len < 65535) {
				if (mask) {
					snprintf(buf, len+10, "%c%c%c%c%c%c%c%c", 0x80+0x01, 126+0x80, 
					         (unsigned char)(len >> 8), (unsigned char)(len & 0xFF),
					         mask_key[0], mask_key[1], mask_key[2], mask_key[3]);
					memcpy(buf+8, msg, len);
				} else {
					snprintf(buf, len+10, "%c%c%c%c%s", 0x80+0x01, 126, 
					         (unsigned char)(len >> 8), (unsigned char)(len & 0xFF), msg);
				}
				res = net_send_block(sock, (unsigned char *)buf, len+4+(mask ? 4 : 0));
			} else { /* More than 64k bytes */
				/* Such long strings are not supported right now */
			}
			if (mask) free(msg);
			free(buf);
		}
	}
	return res;
}


int websocket_calculate_payload_size(char *websocket_packet_header)
{
	int len = websocket_packet_header[1] & 127;
	if (len == 126) {
		len = (((unsigned int)websocket_packet_header[2]) << 8) + (unsigned char)websocket_packet_header[3];
	} else if (len == 127) {
		len = -1; /* unsupported */
	}
	return len;
}

int websocket_calculate_packet_size(char *websocket_packet)
{
	int len = websocket_packet[1] & 127;
	int size = 1;
	if (len > 0) {
		if (len < 126)
			size += 1;
		else if (len == 126)
			size += 3;
		else if (len == 127)
			size += 8;
	}
	if (websocket_packet[1] & 128) /* masked -> 4 bytes extra */
		size += 4;
	len = websocket_calculate_payload_size(websocket_packet);
	if (len > 0) size += len; else size = -1;
	return size;
}

char *websocket_get_payload(char *websocket_packet)
{
	char *payload = NULL;
	int   len = websocket_packet[1] & 127;
	if (len > 0) {
		payload = websocket_packet;
		if (len < 126)
			payload += 2;
		else if (len == 126)
			payload += 4;
		else if (len == 127)
			payload += 9;
	}
	if (websocket_packet[1] & 128) /* masked -> 4 bytes extra */
		payload += 4;
	return payload;
}
