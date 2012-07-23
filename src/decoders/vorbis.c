/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: vorbis.c  Created: 081022
 *
 * Description: Tremor vorbis decoder
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../gmudecoder.h"
#include "../trackinfo.h"
#include "../util.h"
#include "tremor/ivorbiscodec.h"
#include "tremor/ivorbisfile.h"
#include "../debug.h"

static OggVorbis_File  vf, vf_metaonly;
static vorbis_info    *vi;

static const char *get_name(void)
{
	return "Tremor Vorbis decoder v1.0";
}

static int open_file(char *filename)
{
	FILE *file;
	int   res = 0;

	if (!(file = fopen(filename, "r"))) {
		wdprintf(V_WARNING, "vorbis", "Could not open file.\n");
	} else if (ov_open(file, &vf, NULL, 0) < 0) {
		wdprintf(V_WARNING, "vorbis", "Input does not appear to be an Ogg bitstream.\n");
	} else {
		vi  = ov_info(&vf, -1);
		res = 1;
	}
	return res;
}

static int close_file(void)
{
	ov_clear(&vf);
	return 0;
}

static int decode_data(char *target, int max_size)
{
	int        size = 0, ret = 1;
	static int current_section;

	/*while (ret > 0 && size < max_size) {*/
	if (256 <= max_size) {
		ret = ov_read(&vf, target+size, 256, &current_section);
		size += ret;
	} else {
		wdprintf(V_ERROR, "vorbis", "Target buffer too small: %d < 256\n", max_size);
	}
	return size;
}

static int seek(int seconds)
{
	int  unsuccessful = 1;
	long pos = seconds * 1000;

	if (pos <= 0) pos = 0;
	unsuccessful = ov_time_seek_page(&vf, pos);
	return !unsuccessful;
}

static int get_decoder_buffer_size(void)
{
	return 256;
}

static const char* get_file_extensions(void)
{
	return ".ogg";
}

static int get_current_bitrate(void)
{
	return ov_bitrate_instant(&vf);
}

static int get_length(void)
{
	return ov_time_total(&vf, -1) / 1000;
}

static int get_samplerate(void)
{
	return vi->rate;
}

static int get_channels(void)
{
	return vi->channels;
}

static int get_bitrate(void)
{
	return ov_bitrate(&vf, -1);
}

static const char *get_meta_data(GmuMetaDataType gmdt, int for_current_file)
{
	char *result = NULL;
	char **ptr   = NULL;

	if (for_current_file) {
		ptr = ov_comment(&vf, -1)->user_comments;
	} else {
		ptr = ov_comment(&vf_metaonly, -1)->user_comments;
	}

	while (*ptr) {
		char buf[80];
		strtoupper(buf, *ptr, 79);				
		switch (gmdt) {
			case GMU_META_ARTIST:
				if (strstr(buf, "ARTIST=") == buf)
					result = *ptr+7;
				break;
			case GMU_META_TITLE:
				if (strstr(buf, "TITLE=") == buf)
					result = *ptr+6;
				break;
			case GMU_META_ALBUM:
				if (strstr(buf, "ALBUM=") == buf)
					result = *ptr+6;
				break;
			case GMU_META_TRACKNR:
				if (strstr(buf, "TRACKNUMBER=") == buf)
					result = *ptr+12;
				break;
			case GMU_META_DATE:
				if (strstr(buf, "DATE=") == buf)
					result = *ptr+5;
				break;
			default:
				break;
		}
		ptr++;
	}
	return result;
}

static const char *get_file_type(void)
{
	return "Ogg Vorbis";
}

static int meta_data_load(const char *filename)
{
	FILE *file;
	int   result = 1;

	if ((file = fopen(filename, "r"))) {
		if (ov_open(file, &vf_metaonly, NULL, 0) < 0) {
			wdprintf(V_WARNING, "vorbis", "Input does not appear to be an Ogg bitstream.\n");
			result = 0;
		}
	} else {
		result = 0;
	}
	return result;
}

static int meta_data_close(void)
{
	ov_clear(&vf_metaonly);
	return 1;
}

static GmuCharset meta_data_get_charset(void)
{
	return M_CHARSET_UTF_8;
}

static GmuDecoder gd = {
	"vorbis_decoder",
	NULL,
	NULL,
	get_name,
	NULL,
	get_file_extensions,
	NULL,
	open_file,
	close_file,
	decode_data,
	seek,
	get_current_bitrate,
	get_meta_data,
	NULL,
	get_samplerate,
	get_channels,
	get_length,
	get_bitrate,
	get_file_type,
	get_decoder_buffer_size,
	meta_data_load,
	meta_data_close,
	meta_data_get_charset,
	NULL,
	NULL,
	NULL
};

GmuDecoder *GMU_REGISTER_DECODER(void)
{
	return &gd;
}
