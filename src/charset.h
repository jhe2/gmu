/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: charset.h  Created: 061115
 *
 * Description: Charset conversion functions (UTF-8/UTF-16/ISO-8859-1)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _CHARSET_H
#define _CHARSET_H
#define UNICODE_REPLACEMENT_CHAR 0x0000FFFD
#define UNICODE_SUR_HIGH_START   0xD800
#define UNICODE_SUR_HIGH_END     0xDBFF
#define UNICODE_SUR_LOW_START    0xDC00
#define UNICODE_SUR_LOW_END      0xDFFF
typedef enum Charset { ASCII, ISO_8859_1, ISO_8859_15, UTF_8, UTF_16, UTF_16_BOM, UNKNOWN_CHARSET } Charset;
typedef enum { BE, LE, BOM } ByteOrder;
typedef unsigned long UCodePoint;

int   charset_utf8_to_iso8859_1(char *target, const char *source, int target_size);
int   charset_utf16_to_iso8859_1(char       *target, int target_size,
                                 const char *source, int source_size,
                                 ByteOrder   byte_order);
int   charset_iso8859_1_to_utf8(char *target, const char *source, int target_size);
int   charset_utf16_to_utf8(char       *target, int target_size,
                            const char *source, int source_size,
                            ByteOrder   byte_order);
void  charset_filename_set(Charset charset);
char *charset_filename_convert_alloc(const char *filename);
int   charset_convert_string(const char *source, Charset source_charset,
                             char       *target, Charset target_charset,
                             int         target_size);
int   charset_is_valid_utf8_string(const char *str);
int   charset_utf8_to_codepoints(UCodePoint *target, const char *source, int target_size);
int   charset_utf8_len(const char *str);
/* Fiexes a broken UTF-8 string. If there is nothing to be fixed,
 * the string is left as is and 0 is returned, 1 otherwise */
int   charset_fix_broken_utf8_string(char *str);
#endif
