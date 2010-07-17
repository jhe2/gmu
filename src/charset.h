/* 
 * Gmu GP2X Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: charset.h  Created: 061115
 *
 * Description: Charset conversion functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _CHARSET_H
#define _CHARSET_H
typedef enum Charset { ASCII, ISO_8859_1, ISO_8859_15, UTF_8, UTF_16, UTF_16_BOM, UNKNOWN_CHARSET } Charset;
typedef enum { BE, LE, BOM } ByteOrder;

int   charset_utf8_to_iso8859_1(char *target, const char *source, int target_size);
int   charset_utf16_to_iso8859_1(char       *target, int target_size,
                                 const char *source, int source_size,
                                 ByteOrder   byte_order);
void  charset_filename_set(Charset charset);
char *charset_filename_convert_alloc(const char *filename);
int   charset_convert_string(const char *source, Charset source_charset,
                             char       *target, Charset target_charset,
                             int         target_size);
#endif
