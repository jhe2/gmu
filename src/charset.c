/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: charset.c  Created: 061115
 *
 * Description: Charset conversion functions (UTF-8/UTF-16/ISO-8859-1)
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
#include "debug.h"

int charset_utf8_to_iso8859_1(char *target, const char *source, int target_size)
{
	int            i, j, len = strlen(source), valid = 1;
	unsigned char *src = (unsigned char *)source;
	len = (len < target_size ? len : target_size);
	for (i = 0, j = 0; i < len; i++) {
		if (src[i] < 128) { /* ASCII char */
			target[j] = src[i];
		} else if (src[i] >= 192 && src[i] < 224) { /* 2 byte char */
			if (i+1 >= len || src[i+1] < 128) valid = 0;
			target[j] = src[i+1] + 64;
			i += 1;
		} else if (src[i] >= 224 && src[i] < 240) { /* 3 byte char */
			if (i+2 >= len || src[i+1] < 128 || src[i+2] < 128) valid = 0;
			target[j] = '?';
			i += 2;
		} else if (src[i] >= 240 && src[i] < 248) { /* 4 byte char */
			if (i+3 >= len || src[i+1] < 128 || src[i+2] < 128 || src[i+3] < 128) valid = 0;
			target[j] = '?';
			i += 3;
		} else {
			valid = 0;
		}
		if (!valid) {
			/*wdprintf(V_DEBUG, "charset", "utf-8: Invalid UTF-8 string!\n");*/
			break;
		}
		j++;
	}
	target[j] = '\0';
	return valid;
}

static UCodePoint get_utf16_code_point(const char *source, ByteOrder byte_order)
{
	unsigned char b1, b2;
	UCodePoint    code_point;

	if (byte_order == BE) {
		b1 = (unsigned char)source[0];
		b2 = (unsigned char)source[1];
	} else {
		b1 = (unsigned char)source[1];
		b2 = (unsigned char)source[0];
	}
	code_point = 256 * b1 + b2;
	return code_point;
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
	wdprintf(V_DEBUG, "charset", "utf-16: byte order: %s endian\n", byte_order == BE ? "big" : "little");
	wdprintf(V_DEBUG, "charset", "utf-16: valid utf-16 till here: %s\n", valid ? "yes" : "no");
	while (valid && i < source_size - 1 && !(source[i] == 0 && source[i+1] == 0)) {
		int code_point = get_utf16_code_point(source+i, byte_order);
		if (code_point >= UNICODE_SUR_HIGH_START && code_point <= UNICODE_SUR_HIGH_END) { /* surrogates */
			i += 2; /* skip next 16 bit. we ignore everything outside the BMP */
			target[j] = '?';
			if (j < target_size - 1) j++;
		} else if (code_point < 256) {
			target[j] = code_point;
			if (j < target_size - 1) j++;
		} else {
			/*wdprintf(V_DEBUG, "charset", "utf-16: code_point = %d\n", code_point);*/
			target[j] = '?';
			if (j < target_size - 1) j++;
		}
		i += 2;
	}
	target[j] = '\0';
	return valid;
}

int charset_iso8859_1_to_utf8(char *target, const char *source, int target_size)
{
	int            i, j, len = strlen(source), valid = 1;
	unsigned char *src = (unsigned char *)source;
	len = (len < target_size ? len : target_size);
	for (i = 0, j = 0; i < len && j < target_size-1; i++, j++) {
		if (src[i] < 128) { /* ASCII char */
			target[j] = src[i];
		} else { /* Latin-1 character => 2 byte UTF-8 char */
			target[j]   = 192 + (src[i] >> 6);
			target[j+1] = 128 + (src[i] & 63);
			j++;
		}
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
	char *buf = NULL;
	int   len = filename ? strlen(filename) : 0;
	if (len > 0) buf = malloc(len+1);
	if (buf) {
		buf[len] = '\0';
		switch (filename_charset) {
			case UTF_8:
				charset_utf8_to_iso8859_1(buf, filename, len);
				break;
			default:
				strncpy(buf, filename, len);
				break;
		}
	}
	return buf;
}

int charset_utf16_to_utf8(char       *target, int target_size,
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
	wdprintf(V_DEBUG, "charset", "utf-16: byte order: %s endian\n", byte_order == BE ? "big" : "little");
	wdprintf(V_DEBUG, "charset", "utf-16: valid utf-16 till here: %s\n", valid ? "yes" : "no");
	while (valid && i < source_size - 1 && !(source[i] == 0 && source[i+1] == 0)) {
		UCodePoint code_point = get_utf16_code_point(source+i, byte_order);
		int        bm, bytes_to_write = 0;

		/* If high surrogate, we need to get the next surrogate as well and convert to UTF32 */
		if (code_point >= UNICODE_SUR_HIGH_START && code_point <= UNICODE_SUR_HIGH_END) {
			/* If the 16 bits following the high surrogate are in the source buffer... */
			if (i + 2 < source_size) {
				UCodePoint cp2 = get_utf16_code_point(source + i + 2, byte_order);
				/* If it's a low surrogate, convert to UTF32. */
				if (cp2 >= UNICODE_SUR_LOW_START && cp2 <= UNICODE_SUR_LOW_END) {
					code_point = ((code_point - UNICODE_SUR_HIGH_START) << 10)
					           + (cp2 - UNICODE_SUR_LOW_START) + 0x0010000UL;
					i += 2;
				}
			} else { /* We don't have the 16 bits following the high surrogate. */
				valid = 0;
				break;
			}
		}

		if (code_point < 0x80) {
			bytes_to_write = 1;
			bm = 0x00;
		} else if (code_point < 0x800) {
			bytes_to_write = 2;
			bm = 0xC0;
		} else if (code_point < 0x10000) {
			bytes_to_write = 3;
			bm = 0xE0;
		} else if (code_point < 0x110000) {
			bytes_to_write = 4;
			bm = 0xF0;
		} else {
			bytes_to_write = 3;
			bm = 0xE0;
			code_point = UNICODE_REPLACEMENT_CHAR;
		}

		if (j + bytes_to_write < target_size) {
			switch (bytes_to_write) { /* everything falls through */
				case 4: target[j+3] = (((code_point >> 6) | 0x80) & 0xBF);
				case 3: target[j+2] = (((code_point >> (6 * (bytes_to_write-3))) | 0x80) & 0xBF);
				case 2: target[j+1] = (((code_point >> (6 * (bytes_to_write-2))) | 0x80) & 0xBF);
				case 1: target[j]   =  ((code_point >> (6 * (bytes_to_write-1))) | bm);
			}
			j += bytes_to_write;
		} else {
			valid = 0;
			break;
		}

		i += 2;
	}
	target[j] = '\0';
	return valid;
}

/*int charset_convert_string_autodetect(const char *source, char *target,
                                      Charset     target_charset,
                                      int         target_size)
{*/
	/* Try to convert from UTF-8 first: */
	
	/* When everything else failed, assume ISO-8859-1: */
/*	return 0;
}*/

/*int charset_convert_string(const char *source, Charset source_charset,
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
				wdprintf(V_WARNING, "charset", "Source charset not supported.\n");
				break;
		}
	} else {
		wdprintf(V_WARNING, "charset", "Target charset not supported.\n");
	}
	return result;
}*/

int charset_is_valid_utf8_string(const char *str)
{
	int            i, len = strlen(str), valid = 1;
	unsigned char *src = (unsigned char *)str;
	for (i = 0; i < len; i++) {
		if (src[i] < 128) { /* ASCII char */
			/* nothing to do */
		} else if (src[i] >= 192 && src[i] < 224) { /* 2 byte char */
			if (i+1 >= len || src[i+1] < 128) valid = 0;
			i += 1;
		} else if (src[i] >= 224 && src[i] < 240) { /* 3 byte char */
			if (i+2 >= len || src[i+1] < 128 || src[i+2] < 128) valid = 0;
			i += 2;
		} else if (src[i] >= 240 && src[i] < 248) { /* 4 byte char */
			if (i+3 >= len || src[i+1] < 128 || src[i+2] < 128 || src[i+3] < 128) valid = 0;
			i += 3;
		} else {
			valid = 0;
		}
		if (!valid)	break;
	}
	return valid;
}

int charset_utf8_to_codepoints(UCodePoint *target, const char *source, int target_size)
{
	int            i, j, len = strlen(source), valid = 1;
	unsigned char *src = (unsigned char *)source;

	for (i = 0, j = 0; j < target_size && source[i]; i++) {
		if (src[i] < 128) { /* ASCII char */
			target[j] = src[i];
		} else if (src[i] >= 192 && src[i] < 224) { /* 2 byte char */
			if (i+1 >= len || src[i+1] < 128) {
				valid = 0;
			} else {
				target[j] = ((src[i] & 0x1F) << 6) + (src[i+1] & 0x3F);
				i += 1;
			}
		} else if (src[i] >= 224 && src[i] < 240) { /* 3 byte char */
			if (i+2 >= len || src[i+1] < 128 || src[i+2] < 128) {
				valid = 0;
			} else {
				target[j] = ((src[i] & 0x0F) << 12) + ((src[i+1] & 0x3F) << 6) + (src[i+2] & 0x3F);
				i += 2;
			}
		} else if (src[i] >= 240 && src[i] < 248) { /* 4 byte char */
			if (i+3 >= len || src[i+1] < 128 || src[i+2] < 128 || src[i+3] < 128) {
				valid = 0;
			} else {
				target[j] = ((src[i] & 0x07) << 18) + ((src[i+1] & 0x3F) << 12) + 
							((src[i+2] & 0x3F) << 6) + (src[i+3] & 0x3F);
				i += 3;
			}
		} else {
			valid = 0;
		}
		if (!valid) {
			/*wdprintf(V_DEBUG, "charset", "utf-8: Invalid UTF-8 string!\n");*/
			break;
		}
		j++;
	}
	for (i = j; i < target_size; i++) target[i] = 0;
	return valid;
}

int charset_utf8_len(const char *str)
{
	int u8len = 0;
	int            i, len = strlen(str), valid = 1;
	unsigned char *src = (unsigned char *)str;
	for (i = 0; i < len; i++) {
		if (src[i] < 128) { /* ASCII char */
			u8len++;
		} else if (src[i] >= 192 && src[i] < 224) { /* 2 byte char */
			if (i+1 >= len || src[i+1] < 128) valid = 0;
			i += 1;
			u8len++;
		} else if (src[i] >= 224 && src[i] < 240) { /* 3 byte char */
			if (i+2 >= len || src[i+1] < 128 || src[i+2] < 128) valid = 0;
			i += 2;
			u8len++;
		} else if (src[i] >= 240 && src[i] < 248) { /* 4 byte char */
			if (i+3 >= len || src[i+1] < 128 || src[i+2] < 128 || src[i+3] < 128) valid = 0;
			i += 3;
			u8len++;
		} else {
			valid = 0;
		}
		if (!valid) {
			u8len = 0;
			/*wdprintf(V_DEBUG, "charset", "utf-8: Invalid UTF-8 string!\n");*/
			break;
		}
	}
	return u8len;
}

int charset_fix_broken_utf8_string(char *str)
{
	int i, len = strlen(str);
	int fixed = 0;
	unsigned char *src = (unsigned char *)str;
	for (i = 0; i < len; i++) {
		if (src[i] < 128) { /* ASCII char */
			/* nothing to do */
		} else if (src[i] >= 192 && src[i] < 224) { /* 2 byte char */
			if (i+1 < len && src[i+1] >= 128) {
				i += 1;
			} else {
				fixed = 1;
				str[i] = '\0';
			}
		} else if (src[i] >= 224 && src[i] < 240) { /* 3 byte char */
			if (i+2 < len && (src[i+1] >= 128 || src[i+2] >= 128)) {
				i += 2;
			} else {
				fixed = 1;
				str[i] = '\0';
			}
		} else if (src[i] >= 240 && src[i] < 248) { /* 4 byte char */
			if (i+3 < len && (src[i+1] < 128 || src[i+2] < 128 || src[i+3] < 128)) {
				i += 3;
			} else {
				fixed = 1;
				str[i] = '\0';
			}
		} else {
			fixed = 1;
			str[i] = '\0';
		}
	}
	return fixed;
}
