/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: charset.c  Created: 061115
 *
 * Description: Charset conversion functions (UTF-8/UTF-16 -> ISO-8859-1)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "charset.h"

int charset_utf8_to_iso8859_1(char *target, const char *source, int target_size)
{
	int            i, j, len = strlen(source), valid = 1;
	unsigned char *src = (unsigned char *)source;
	len = (len < target_size ? len : target_size);
	for (i = 0, j = 0; i < len; i++) {
		if (src[i] < 128) { /* ASCII char */
			target[j] = src[i];
		} else if (src[i] >= 192 && src[i] < 224) { /* 2 byte char */
			if (src[i+1] < 128) valid = 0;
			target[j] = src[i+1] + 64;
			i += 1;
		} else if (src[i] >= 224 && src[i] < 240) { /* 3 byte char */
			if (src[i+1] < 128 || src[i+2] < 128) valid = 0;
			target[j] = '?';
			i += 2;
		} else if (src[i] >= 240 && src[i] < 248) { /* 4 byte char */
			if (src[i+1] < 128 || src[i+2] < 128 || src[i+3] < 128) valid = 0;
			target[j] = '?';
			i += 3;
		} else {
			valid = 0;
		}
		if (valid == 0) {
			/*printf("utf-8: Invalid UTF-8 string!\n");*/
			break;
		}
		j++;
	}
	target[j] = '\0';
	return valid;
}

int charset_utf16_to_iso8859_1(char       *target, int target_size,
                               const char *source, int source_size,
                               ByteOrder   byte_order)
{
	int valid = 1, i = (byte_order == BOM ? 2 : 0), j = 0;

	if (source_size < 2) valid = 0;
	if (valid && byte_order == BOM) { /* check byte order */
		if ((unsigned char)source[0] == 0xff && (unsigned char)source[1] == 0xfe)
			byte_order = LE;
		else if ((unsigned char)source[1] == 0xff && (unsigned char)source[0] == 0xfe)
			byte_order = BE;
		else
			valid = 0; 
	}
	printf("utf-16: byte order: %s endian\n", byte_order == BE ? "big" : "little");
	printf("utf-16: valid utf-16 till here: %s\n", valid ? "yes" : "no");
	while (valid && i < source_size - 1 && !(source[i] == 0 && source[i+1] == 0)) {
		unsigned char b1, b2;
		int      code_point;
		if (byte_order == BE) {
			b1 = (unsigned char)source[i];
			b2 = (unsigned char)source[i+1];
		} else {
			b1 = (unsigned char)source[i+1];
			b2 = (unsigned char)source[i];
		}
		code_point = 256 * b1 + b2;
		if (code_point >= 0xd800 && code_point <= 0xf800) { /* surrogates */
			i += 2; /* skip next 16 bit. we ignore everything outside the BMP */
			target[j] = '?';
			if (j < target_size - 1) j++;
		} else if (code_point < 256) {
			target[j] = b2;
			if (j < target_size - 1) j++;
		} else {
			/*printf("utf-16: code_point = %d\n", code_point);*/
			target[j] = '?';
			if (j < target_size - 1) j++;
		}
		i += 2;
	}
	target[j] = '\0';
	return valid;
}

static Charset filename_charset;

void charset_filename_set(Charset charset)
{
	filename_charset = charset;
}

char *charset_filename_convert_alloc(const char *filename)
{
	char *buf;
	buf = malloc(256);
	buf[255] = '\0';
	switch (filename_charset) {
		case UTF_8:
			charset_utf8_to_iso8859_1(buf, filename, 255);
			break;
		default:
			strncpy(buf, filename, 255);
			break;
	}
	return buf;
}

/*int charset_convert_string_autodetect(const char *source, char *target,
                                      Charset     target_charset,
                                      int         target_size)
{*/
	/* Try to convert from UTF-8 first: */
	
	/* When everything else failed, assume ISO-8859-1: */
/*	return 0;
}*/

int charset_convert_string(const char *source, Charset source_charset,
                           char       *target, Charset target_charset,
                           int         target_size)
{
	int result = 0;
	if (target_charset == ISO_8859_1 || target_charset == ISO_8859_15) {
		switch (source_charset) {
			case ASCII:
			case ISO_8859_1:
			case ISO_8859_15:
				strncpy(target, source, target_size);
				result = 1;
				break;
			case UTF_8:
				result = charset_utf8_to_iso8859_1(target, source, target_size);
				break;
			case UNKNOWN_CHARSET:
			default:
				printf("charset: Source charset not supported.\n");
				break;
		}
	} else {
		printf("charset: Target charset not supported.\n");
	}
	return result;
}
