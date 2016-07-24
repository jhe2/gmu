/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2016 Johannes Heimansberg (wej.k.vu)
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
#include "../debug.h"

static int          init = 0;
static long         seek_to_sample_offset;
static int          sample_rate, channels = 0, bitrate = 0;
static TrackInfo    ti, ti_metaonly;
static Reader      *r;
static OggOpusFile *oof;
static int          seek_request = 0;

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
				start = reader_get_stream_position(r);
				break;
			case SEEK_END:
				start = reader_get_file_size(r);
				break;
		}
		res = reader_seek(r, start + _offset) ? 0 : -1;
	}
	return res;
}

/* Returns the current position in the stream */
static opus_int64 tell_func(void *_stream)
{
	Reader *r = (Reader *)_stream;
	wdprintf(V_DEBUG, "opus", "tell_func(): %d bytes\n", reader_get_stream_position(r));
	return reader_get_stream_position(r);
}

/* Returns 0 on success and EOF otherwise */
static int close_func(void *_stream)
{
	wdprintf(V_DEBUG, "opus", "close_func() called\n");
	return 0;
}

struct _trackinfo_mapping {
	char *key;
	char *target;
	int   maxlen;
};

static struct _trackinfo_mapping tim[] = {
	{ "artist=",      ti.artist,  SIZE_ARTIST },
	{ "title=",       ti.title,   SIZE_TITLE },
	{ "album=",       ti.album,   SIZE_ALBUM },
	{ "tracknumber=", ti.tracknr, SIZE_TRACKNR },
	{ "date=",        ti.date,    SIZE_DATE },
	{ "comment=",     ti.comment, SIZE_COMMENT },
	{ NULL,           NULL,       0 }
};

static struct _trackinfo_mapping tim_metaonly[] = {
	{ "artist=",      ti_metaonly.artist,  SIZE_ARTIST },
	{ "title=",       ti_metaonly.title,   SIZE_TITLE },
	{ "album=",       ti_metaonly.album,   SIZE_ALBUM },
	{ "tracknumber=", ti_metaonly.tracknr, SIZE_TRACKNR },
	{ "date=",        ti_metaonly.date,    SIZE_DATE },
	{ "comment=",     ti_metaonly.comment, SIZE_COMMENT },
	{ NULL,           NULL,       0 }
};

static void read_tags(OggOpusFile *oof, int li, struct _trackinfo_mapping *tim)
{
	const OpusTags *tags = op_tags(oof, li);
	int             ci, i;
	if (tags) {
		for (ci = 0; ci < tags->comments; ci++) {
			wdprintf(V_DEBUG, "opus", "metadata(%d): %s\n", ci, tags->user_comments[ci]);
			for (i = 0; tim[i].key; i++) {
				int len = strlen(tim[i].key);
				if (strncasecmp(tags->user_comments[ci], tim[i].key, len) == 0) {
					wdprintf(V_INFO, "opus", "%s> %s\n", tim[i].key, tags->user_comments[ci]+len);
					strncpy(tim[i].target, tags->user_comments[ci]+len, tim[i].maxlen);
					tim[i].target[tim[i].maxlen-1] = '\0';
				}
			}
		}
	}
}

static int decode_data(char *target, size_t max_size)
{
	int        res = 0;
	static int prev_li = -1;
	int        samples = 0;
	int        li = op_current_link(oof);

	if (li != prev_li) {
		prev_li = li;
		read_tags(oof, li, tim);
	}

	if (seek_request && reader_is_seekable(r) && seek_to_sample_offset >= 0) {
		wdprintf(V_DEBUG, "opus", "Seeking requested to sample %d.\n", seek_to_sample_offset);
		if (op_pcm_seek(oof, seek_to_sample_offset) != 0)
			wdprintf(V_WARNING, "opus", "Seeking failed.\n");
		seek_to_sample_offset = 0;
		seek_request = 0;
	}

	if (channels > 1)
		samples = op_read_stereo(oof, (opus_int16 *)target, max_size / 2);
	else if (channels == 1)
		samples = op_read(oof, (opus_int16 *)target, max_size / 2, NULL);
	if (samples > 0)
		res = samples * 2 * channels;
	return res;
}

static int opus_play_file(const char *opus_file)
{
	int result = 1, error;
	OpusFileCallbacks ofc;
	int available_bytes;

	wdprintf(V_DEBUG, "opus", "Initializing.\n");
	trackinfo_init(&ti, 0);

	ofc.read  = read_func;
	ofc.seek  = seek_func;
	ofc.tell  = tell_func;
	ofc.close = close_func;

	if (r) {
		available_bytes = reader_get_number_of_bytes_in_buffer(r);

		wdprintf(V_DEBUG, "opus", "Available bytes in buffer: %d\n", available_bytes);
		oof = op_open_callbacks(r, &ofc, (unsigned char *)reader_get_buffer(r),
								available_bytes, &error);

		wdprintf(V_INFO, "opus", "Stream open result: %d\n", error);
		if (error) {
			result = 0;
		} else {
			int li = op_current_link(oof);
			read_tags(oof, li, tim);
			channels = op_channel_count(oof, -1);
			bitrate  = op_bitrate(oof, -1);
			sample_rate = 48000;
		}
	} else {
		wdprintf(V_WARNING, "opus", "Reader was unable to open stream/file.\n");
		result = 0;
	}
	return result;
}

static int opus_seek_to(int offset_seconds)
{
	int res = 0;
	if (offset_seconds >= 0) {
		seek_to_sample_offset = offset_seconds * sample_rate;
		seek_request = 1;
		res = 1;
	}
	return res;
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
	ogg_int64_t samples = op_pcm_total(oof, -1);
	return samples == OP_EINVAL ? 0 : samples / 48000;
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
	int result = 0, error;
	OpusFileCallbacks ofc;
	Reader *re;
	OggOpusFile *oof_tmp;

	wdprintf(V_DEBUG, "opus", "Initializing.\n");
	trackinfo_init(&ti, 0);

	ofc.read  = read_func;
	ofc.seek  = seek_func;
	ofc.tell  = tell_func;
	ofc.close = close_func;

	re = reader_open(filename);
	if (re) {
		oof_tmp = op_open_callbacks(re, &ofc, (unsigned char *)reader_get_buffer(re), 0, &error);

		wdprintf(V_INFO, "opus", "Stream open result: %d\n", error);
		if (error) {
			result = 0;
		} else {
			int li = op_current_link(oof_tmp);
			read_tags(oof_tmp, li, tim_metaonly);
			result = 1;
		}
		op_free(oof_tmp);
		reader_close(re);
	}
	return result;
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

static int data_check_magic_bytes(const char *data, size_t size)
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
