/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
 *
 * File: metadatareader.c  Created: 130709
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
#include "gmudecoder.h"
#include "util.h"
#include "decloader.h"
#include "metadatareader.h"

int metadatareader_read(const char *file, const char *file_type, TrackInfo *ti)
{
	int         result = 0;
	GmuDecoder *gd = decloader_get_decoder_for_extension(file_type);
	GmuCharset  charset = M_CHARSET_AUTODETECT;

	if (gd && *gd->meta_data_get_charset)
		charset = (*gd->meta_data_get_charset)();

	trackinfo_clear(ti);
	if (gd && *gd->meta_data_load && (*gd->meta_data_load)(file)) {
		if (*gd->get_meta_data) {
			if ((*gd->get_meta_data)(GMU_META_ARTIST, 0))
				strncpy_charset_conv(ti->artist,  (*gd->get_meta_data)(GMU_META_ARTIST, 0), SIZE_ARTIST-1, 0, charset);
			if ((*gd->get_meta_data)(GMU_META_TITLE, 0))
				strncpy_charset_conv(ti->title,   (*gd->get_meta_data)(GMU_META_TITLE, 0), SIZE_TITLE-1, 0, charset);
			if ((*gd->get_meta_data)(GMU_META_ALBUM, 0))
				strncpy_charset_conv(ti->album,   (*gd->get_meta_data)(GMU_META_ALBUM, 0), SIZE_ALBUM-1, 0, charset);
			if ((*gd->get_meta_data)(GMU_META_TRACKNR, 0))
				strncpy_charset_conv(ti->tracknr, (*gd->get_meta_data)(GMU_META_TRACKNR, 0), SIZE_TRACKNR-1, 0, charset);
			if ((*gd->get_meta_data)(GMU_META_DATE, 0))
				strncpy_charset_conv(ti->date,    (*gd->get_meta_data)(GMU_META_DATE, 0), SIZE_DATE-1, 0, charset);
			trackinfo_set_updated(ti);
			result = 1;
		}
		if (*gd->meta_data_close) (*gd->meta_data_close)();
	}
	return result;
}
