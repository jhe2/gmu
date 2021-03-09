/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2021 Johannes Heimansberg (wej.k.vu)
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
static Reader         *r;

static const char *get_name(void)
{
	return "Tremor Vorbis decoder v1.1";
}

static size_t read_callback(void *buffer, size_t size, size_t nmemb, void *datasource)
{
	/*
	 * When initializing the stream, some bytes may have been read already,
	 * which we don't want to skip, so we first check if there is something
	 * in the buffer already, otherwise we initiate a read operation with
	 * the desired size.
	 */
	size_t bs = reader_get_number_of_bytes_in_buffer(r);

	if (bs <= 0) {
		if (reader_read_bytes(r, size * nmemb)) {
			bs = reader_get_number_of_bytes_in_buffer(r);
		} else {
			bs = 0;
		}
	}

	if (bs != 0 && !reader_is_eof(r)) {
		memcpy(buffer, reader_get_buffer(r), bs);
		/* Clear the buffer, so we know that we have consumed the data: */
		reader_clear_buffer(r);
	} else {
		bs = 0;
		wdprintf(V_DEBUG, "vorbis", "read_callback(): End of stream!\n");
	}
	return bs;
}

static long tell_callback(void *datasource)
{
	return reader_get_stream_position(r);
}

/* Returns 0 on success or -1 if seeking is unsupported or an error occured */
static int seek_callback(void *datasource, ogg_int64_t offset, int whence)
{
	int     res = -1;
	Reader *r = (Reader *)datasource;

	if (reader_is_seekable(r)) {
		res = reader_seek_whence(r, offset, whence) ? 0 : -1;
	}
	return res;
}

static ov_callbacks callbacks;

static int open_file(const char *filename)
{
	int res = 0;

	callbacks.read_func = read_callback;
	callbacks.tell_func = tell_callback;
	callbacks.seek_func = seek_callback;

	if (!r) {
		wdprintf(V_WARNING, "flac", "Unable to open stream: %s\n", filename);
		return 0;
	}
	/* We need to seek back to position 0, because Gmu might have already
	 * read some bytes from the file/stream to determine mime type, which
	 * upsets the Vorbis decoder, when it tries to do a relative seek at
	 * the beginning and expects to be at absolute position 0: */
	reader_seek(r, 0);

	if (ov_open_callbacks(r, &vf, NULL, 0, callbacks) < 0) {
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

static int decode_data(char *target, size_t max_size)
{
	int        size = -1;
	static int current_section;

	if (4096 <= max_size) {
		int i;
		/* In case of a (temporary) error (e.g. OV_HOLE), we retry a few times before giving up */
		for (i = 0; i < 10 && size < 0; i++) {
			size = ov_read(&vf, target, 4096, &current_section);
			if (size > 0) break;
		}
	} else {
		wdprintf(V_ERROR, "vorbis", "Target buffer too small: %d < 4096\n", max_size);
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
	return 4096;
}

static const char* get_file_extensions(void)
{
	return ".ogg;.oga";
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

static void set_reader_handle(Reader *reader)
{
	r = reader;
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
	set_reader_handle,
	NULL
};

GmuDecoder *GMU_REGISTER_DECODER(void)
{
	return &gd;
}
