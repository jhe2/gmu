/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
 *
 * File: opus.c  Created: 130303
 *
 * Description: Opus decoder
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
#include <opusfile.h>
#include "../gmudecoder.h"
#include "../trackinfo.h"
#include "../util.h"
#include "../reader.h"
#include "../wejpconfig.h"
#include "../debug.h"

static int          init = 0;
static long         seek_to_sample_offset;
static int          sample_rate, channels = 0, bitrate = 0;
static TrackInfo    ti, ti_metaonly;
static Reader      *r;
static OggOpusFile *oof;
static int          byte_counter;

static const char *get_name(void)
{
	return "Ogg Opus decoder v0.1";
}

/* Returns the number of bytes read, or a negative value on error */
static int read_func(void *_stream, unsigned char *_ptr, int _nbytes)
{
	int     res = -1;
	Reader *r = (Reader *)_stream;
	if (reader_read_bytes(r, _nbytes)) {
		memcpy(_ptr, reader_get_buffer(r), _nbytes);
		byte_counter += _nbytes;
		res = _nbytes;
	}
	return res;
}

/* Returns 0 on success or -1 if seeking is unsupported or an error occured */
static int seek_func(void *_stream, opus_int64 _offset, int _whence)
{
	int res = -1, start;
	Reader *r = (Reader *)_stream;
	wdprintf(V_DEBUG, "opus", "seek_func() called: %d / %d\n", _offset, _whence);
	if (reader_is_seekable(r)) {
		switch (_whence) {
			default:
			case SEEK_SET:
				start = 0;
				break;
			case SEEK_CUR:
				start = byte_counter;
				break;
			case SEEK_END:
				start = reader_get_file_size(r);
				break;
		}
		res = reader_seek(r, start + _offset) ? 0 : -1;
		if (res == 0) byte_counter = start + _offset;
	}
	return res;
}

/* Returns the current position in the stream */
static opus_int64 tell_func(void *_stream)
{
	wdprintf(V_DEBUG, "opus", "tell_func(): %d bytes\n", byte_counter);
	return byte_counter;
}

/* Returns 0 on success and EOF otherwise */
static int close_func(void *_stream)
{
	wdprintf(V_DEBUG, "opus", "close_func() called\n");
	return 0;
}

static int decode_data(char *target, int max_size)
{
	int res = 0;
	int samples = op_read_stereo(oof, (opus_int16 *)target, max_size / 2);
	if (samples > 0) {
		res = samples * 4;
	}
	return res;
}

static int opus_play_file(char *opus_file)
{
	int result = 1, error;
	OpusFileCallbacks ofc;

	wdprintf(V_DEBUG, "opus", "Initializing.\n");

	ofc.read  = read_func;
	ofc.seek  = seek_func;
	ofc.tell  = tell_func;
	ofc.close = close_func;

	int available_bytes = reader_get_number_of_bytes_in_buffer(r);

	byte_counter = available_bytes;

	wdprintf(V_DEBUG, "opus", "Available bytes in buffer: %d\n", available_bytes);
	oof = op_open_callbacks(r, &ofc, (unsigned char *)reader_get_buffer(r),
	                        available_bytes, &error);

	wdprintf(V_INFO, "opus", "Stream open result: %d\n", error);
	if (error) {
		result = 0;
	} else {
		channels = op_channel_count(oof, -1);
		bitrate  = op_bitrate(oof, -1);
		sample_rate = 48000;
	}
	return result;
}

static int opus_seek_to(int offset_seconds)
{
	seek_to_sample_offset = offset_seconds * sample_rate;
	return seek_to_sample_offset;
}

static int close_file(void)
{
	wdprintf(V_DEBUG, "opus", "Closing file.\n");
	op_free(oof);
	init = 0;
	return 0;
}

static int get_decoder_buffer_size(void)
{
	return 256;
}

static const char* get_file_extensions(void)
{
	return ".opus";
}

static int get_current_bitrate(void)
{
	return op_bitrate_instant(oof);
}

static int get_length(void)
{
	return 0;
}

static int get_samplerate(void)
{
	return sample_rate;
}

static int get_channels(void)
{
	return channels;
}

static int get_bitrate(void)
{
	return bitrate;
}

static int get_meta_data_int(GmuMetaDataType gmdt, int for_current_file)
{
	int result = 0;
	TrackInfo *t = &ti_metaonly;;
	if (for_current_file) t = &ti;

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
	char *result = NULL;
	TrackInfo *t = &ti_metaonly;;
	if (for_current_file) t = &ti;

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

static const char *get_file_type(void)
{
	return "Ogg Opus Audio";
}

static const char *get_mime_types(void)
{
	return "audio/ogg";
}

static int meta_data_load(const char *filename)
{
	return 0;
}

static int meta_data_close(void)
{
	trackinfo_clear(&ti_metaonly);
	return 1;
}

static GmuCharset meta_data_get_charset(void)
{
	return M_CHARSET_UTF_8;
}

static int data_check_magic_bytes(const char *data, int size)
{
	int res = 0;
	if (strncmp(data, "OggS", 4) == 0) { /* Ok, we've got an Ogg container */
		if (strncmp(data+28, "OpusHead", 8) == 0) {
			res = 1;
			wdprintf(V_DEBUG, "opus", "Magic check: Ogg Opus data detected!\n");
		}
	}
	return res;
}

static void set_reader_handle(Reader *reader)
{
	r = reader;
}

static GmuDecoder gd = {
	"opus_decoder",
	NULL,
	NULL,
	get_name,
	NULL,
	get_file_extensions,
	get_mime_types,
	opus_play_file,
	close_file,
	decode_data,
	opus_seek_to,
	get_current_bitrate,
	get_meta_data,
	get_meta_data_int,
	get_samplerate,
	get_channels,
	get_length,
	get_bitrate,
	get_file_type,
	get_decoder_buffer_size,
	meta_data_load,
	meta_data_close,
	meta_data_get_charset,
	data_check_magic_bytes,
	set_reader_handle,
	NULL
};

GmuDecoder *GMU_REGISTER_DECODER(void)
{
	return &gd;
}
