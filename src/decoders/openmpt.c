/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wej.k.vu)
 *
 * File: openmpt.c  Created: 151106
 *
 * Description: OpenMPT module decoder
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
#include <libopenmpt/libopenmpt.h>
#include <libopenmpt/libopenmpt_stream_callbacks_file.h>
#include "../trackinfo.h"
#include "../reader.h"
#include "../gmudecoder.h"
#include "../debug.h"

static Reader         *r;
static openmpt_module *mod = 0;
static TrackInfo       ti;

static const char *get_name(void)
{
	return "OpenMPT module decoder v0.1";
}

static int decode_data(char *stream, size_t len)
{
	size_t count = openmpt_module_read_interleaved_stereo(
		mod,
		44100,
		len / 4,
		(int16_t *)stream
	);
	return count * 4;
}

static int openmpt_play_file(const char *mod_file)
{
	int    result = 0;
	size_t size = 0;
	char  *module = NULL;

	mod = NULL;
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
			buf = reader_get_buffer(r);
			memcpy(module+offset, buf, size-offset);
			mod = openmpt_module_create_from_memory(
				module,
				size,
				NULL,
				NULL,
				NULL
			);
		} else {
			wdprintf(V_ERROR, "openmpt", "Not enough memory or unable to read from file.\n");
			mod = NULL;
		}
		if (module) free(module);
		if (!mod) {
			wdprintf(V_WARNING, "openmpt", "Could not load %s.\n", mod_file);
		} else {
			const char *meta = openmpt_module_get_metadata(mod, "title");
			if (strlen(meta) == 0) meta = "Unknown";
			trackinfo_set_title(&ti, meta);
			meta = openmpt_module_get_metadata(mod, "artist");
			if (strlen(meta) == 0) meta = "Module";
			trackinfo_set_artist(&ti, meta);
			ti.album[0] = '\0';
			trackinfo_set_file_type(&ti, openmpt_module_get_metadata(mod, "type_long"));
			ti.samplerate = 44100;
			ti.channels   = 2;
			ti.bitrate    = 0;
			result = 1;
		}
	}
	return result;
}

static int close_file(void)
{
	openmpt_module_destroy(mod);
	return 0;
}

/*static int seek_to(int offset_seconds)
{
	seek_to_sample_offset = offset_seconds * sample_rate;
	return seek_to_sample_offset;
}*/

static const char* get_file_extensions(void)
{
	return ".mod;.s3m;.xm;.it;.mptm;.stm;.nst;.m15;.stk;.wow;.ult;.669;.mtm;.med;.far;.mdl;.ams;.dsm;.amf;.okt;.dmf;.ptm;.psm;.mt2;.dbm;.digi;.imf;.j2b;.gdm;.umx;.plm;.mo3;.xpk;.ppm;.mmcmp";
}

static const char *get_file_type(void)
{
	return ti.file_type;
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
	return openmpt_module_get_duration_seconds(mod);
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
	"openmpt_module_decoder",
	NULL,
	NULL,
	get_name,
	NULL,
	get_file_extensions,
	get_mime_types,
	openmpt_play_file,
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
