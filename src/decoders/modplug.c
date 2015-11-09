/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wej.k.vu)
 *
 * File: modplug.c  Created: 130303
 *
 * Description: ModPlug module decoder
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
#include <libmodplug/modplug.h>
#include "../trackinfo.h"
#include "../reader.h"
#include "../gmudecoder.h"
#include "../debug.h"

static Reader      *r;
static ModPlugFile *mpf = NULL;
static char        *module;
static TrackInfo    ti;

static const char *get_name(void)
{
	return "ModPlug module decoder v0.8";
}

static int decode_data(char *stream, size_t len)
{
	return ModPlug_Read(mpf, stream, len);
}

static int modplug_play_file(const char *mod_file)
{
	int    result = 0;
	size_t size = 0;

	mpf = NULL;
	module = NULL;
	if (r) {
		char *buf = reader_get_buffer(r);
		int   offset = 0;

		size = reader_get_file_size(r);
		module = malloc(size);
		if (module) {
			int s = reader_get_number_of_bytes_in_buffer(r);
			memcpy(module, buf, s);
			offset = s;
		}
		if (module && size > 0 && reader_read_bytes(r, size-offset)) {
			wdprintf(V_DEBUG, "modplug", "modplug: size = %ld.\n", size);
			buf = reader_get_buffer(r);
			memcpy(module+offset, buf, size-offset);
			mpf = ModPlug_Load(module, size);
			if (mpf) ModPlug_SetMasterVolume(mpf, 320);
		} else {
			wdprintf(V_ERROR, "modplug", "Not enough memory or unable to read from file.\n");
			mpf = NULL;
		}
		if (mpf == NULL) {
			wdprintf(V_DEBUG, "modplug", "Could not load %s.\n", mod_file);
		} else {
			ModPlug_Settings settings;

			strncpy(ti.title, ModPlug_GetName(mpf), 63);
			strcpy(ti.artist, "Module");
			ti.album[0] = '\0';
			strcpy(ti.file_type, "Module");
			ti.samplerate = 44100;
			ti.channels   = 2;
			ti.bitrate    = 0; /* ((size / 1000) * 8) / (ModPlug_GetLength(mpf) / 1000) * 1000; */
			if (strlen(ti.title) == 0) strcpy(ti.title, "Unknown");

			ModPlug_GetSettings(&settings);
			settings.mFlags = 0;
			settings.mResamplingMode = MODPLUG_RESAMPLE_LINEAR;
			settings.mChannels = 2;
			settings.mBits = 16;
			settings.mFrequency = ti.samplerate;
			/* insert more setting changes here */
			/*settings.mLoopCount = 2;*/
			ModPlug_SetSettings(&settings);
			result = 1;
		}
	}
	return result;
}

static int close_file(void)
{
	if (module) free(module);
	ModPlug_Unload(mpf);
	return 0;
}

/*static int seek_to(int offset_seconds)
{
	seek_to_sample_offset = offset_seconds * sample_rate;
	return seek_to_sample_offset;
}*/

static const char* get_file_extensions(void)
{
	return ".mod;.xm;.it;.669;.s3m;.amf;.ams;.dbm;.dmf;.dsm;.far;.mdl;.med;.mtm;.okt;.ptm;.stm;.ult;.umx;.mt2;.psm;.mid;.midi";
}

static const char *get_file_type(void)
{
	return "Module";
}

static const char *get_mime_types(void)
{
	return "audio/mod;audio/xm;audio/it;audio/s3m";
}

static void set_reader_handle(Reader *reader)
{
	r = reader;
}

static int get_current_bitrate(void)
{
	return 0;
}

static int get_length(void)
{
	return ModPlug_GetLength(mpf) / 1000;
}

static int get_samplerate(void)
{
	return 44100;
}

static int get_channels(void)
{
	return 2;
}

static int get_bitrate(void)
{
	return 0;
}

static int get_decoder_buffer_size(void)
{
	return 256;
}

static GmuCharset meta_data_get_charset(void)
{
	return M_CHARSET_UTF_8;
}

static int get_meta_data_int(GmuMetaDataType gmdt, int for_current_file)
{
	int        result = 0;
	TrackInfo *t = &ti;

	switch (gmdt) {
		case GMU_META_IMAGE_DATA_SIZE:
			result = trackinfo_get_image_data_size(t);
			break;
		case GMU_META_IS_UPDATED:
			result = trackinfo_is_updated(t);
			break;
		default:
			break;
	}
	return result;
}

static const char *get_meta_data(GmuMetaDataType gmdt, int for_current_file)
{
	char      *result = NULL;
	TrackInfo *t = &ti;

	switch (gmdt) {
		case GMU_META_ARTIST:
			result = trackinfo_get_artist(t);
			break;
		case GMU_META_TITLE:
			result = trackinfo_get_title(t);
			break;
		case GMU_META_ALBUM:
			result = trackinfo_get_album(t);
			break;
		case GMU_META_TRACKNR:
			result = trackinfo_get_tracknr(t);
			break;
		case GMU_META_DATE:
			result = trackinfo_get_date(t);
			break;
		case GMU_META_IMAGE_DATA:
			result = trackinfo_get_image_data(t);
			break;
		case GMU_META_IMAGE_MIME_TYPE:
			result = trackinfo_get_image_mime_type(t);
			break;
		default:
			break;
	}
	return result;
}

static GmuDecoder gd = {
	"modplug_module_decoder",
	NULL,
	NULL,
	get_name,
	NULL,
	get_file_extensions,
	get_mime_types,
	modplug_play_file,
	close_file,
	decode_data,
	NULL, /*seek_to,*/
	get_current_bitrate,
	get_meta_data,
	get_meta_data_int,
	get_samplerate,
	get_channels,
	get_length,
	get_bitrate,
	get_file_type,
	get_decoder_buffer_size,
	NULL,
	NULL,
	meta_data_get_charset,
	NULL,
	set_reader_handle,
	NULL
};

GmuDecoder *GMU_REGISTER_DECODER(void)
{
	return &gd;
}
