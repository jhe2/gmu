/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: id3.h  Created: 061030
 *
 * Description: Wejp's ID3 parser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _ID3_H
#define _ID3_H
#include <stdio.h>
#include "trackinfo.h"

int id3_read_id3v1(FILE *file, TrackInfo *ti, const char *file_type);
int id3_read_id3v2(FILE *file, TrackInfo *ti, const char *file_type);
int id3_read_tag(const char *filename, TrackInfo *ti, const char *file_type);
#endif
