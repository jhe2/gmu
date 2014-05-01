/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
 *
 * File: metadatareader.h  Created: 130709
 *
 * Description: Meta data reader; Reads meta data from audio files into
 *              TrackInfo struct
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#include "trackinfo.h"

int metadatareader_read(const char *file, const char *file_type, TrackInfo *ti);
