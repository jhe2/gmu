/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2021 Johannes Heimansberg (wej.k.vu)
 *
 * File: flac.c  Created: 081104
 *
 * Description: FLAC decoder
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
#include "../reader.h"
#include "../charset.h"
#include "FLAC/stream_decoder.h"
#include "../debug.h"
#define BUF_SIZE 65536

static FLAC__StreamDecoder *fsd;
static long                 total_samples, seek_to_sample;
static int                  sample_rate, channels, track_length, bitrate, file_size;
static unsigned int         size = 1; /* size of decoded data */
static char                 buf[BUF_SIZE];
static TrackInfo            ti, ti_metaonly;
static Reader              *r;

static const char *get_name(void)
{
	return "FLAC decoder v0.6";
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, 
                                                     const FLAC__Frame         *frame,
                                                     const FLAC__int32 *const   buffer[],
                                                     void                      *client_data)
{
	unsigned int length = frame->header.blocksize * frame->header.channels
	                                              * frame->header.bits_per_sample / 8;
	unsigned int sample, channel, pos = 0, byte_count = 0;
	FLAC__int16 *packed16 = (FLAC__int16 *)alloca(length);
	FLAC__int8  *packed   = (FLAC__int8 *)packed16;

	for (sample = 0; sample < frame->header.blocksize; sample++) {
		for (channel = 0; channel < frame->header.channels; channel++) {
			switch (frame->header.bits_per_sample) {
				case 8:
					packed[pos] = (FLAC__int8)buffer[channel][sample];
					byte_count++;
					break;
				case 16:
					packed16[pos] = (FLAC__int16)buffer[channel][sample];
					byte_count+=2;
					break;
				case 24:
					packed16[pos] = (FLAC__int16)(buffer[channel][sample] / 256);
					byte_count+=2;
					break;
			}
			pos++;
		}
	}

	if (byte_count <= BUF_SIZE) {
		memcpy(buf, (char *)packed, byte_count);
		size = byte_count;
	} else {
		wdprintf(V_DEBUG, "flac", "Sample size > buffer size: %d bytes\n", byte_count);
	}
	return 0;
}

static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	FLAC__StreamDecoderReadStatus res;
	/*
	 * When initializing the stream, some bytes may have been read already,
	 * which we don't want to skip, so we first check if there is something
	 * in the buffer already, otherwise we initiate a read operation with
	 * the desired size.
	 */
	size_t bs = reader_get_number_of_bytes_in_buffer(r);

	if (bs <= 0) {
		if (reader_read_bytes(r, *bytes)) {
			bs = reader_get_number_of_bytes_in_buffer(r);
		} else {
			bs = 0;
		}
	}
	if (bs != *bytes) {
		*bytes = bs;
	}

	if (bs != 0 && !reader_is_eof(r)) {
		memcpy(buffer, reader_get_buffer(r), *bytes);
		/* Clear the buffer, so we know that we have consumed the data: */
		reader_clear_buffer(r);
		res = FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	} else {
		res = FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		wdprintf(V_DEBUG, "flac", "read_callback(): End of stream!\n");
	}
	return res;
}

static void metadata_callback(const FLAC__StreamDecoder  *decoder,
                              const FLAC__StreamMetadata *metadata,
                              void                       *client_data)
{
	TrackInfo   *ti = (TrackInfo *)client_data;
	unsigned int i;

	switch (metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			sample_rate        = metadata->data.stream_info.sample_rate;
			channels           = metadata->data.stream_info.channels;
			track_length       = metadata->data.stream_info.total_samples / sample_rate;
			bitrate            = (int)((FLAC__int64)file_size * 8 * sample_rate / metadata->data.stream_info.total_samples);

			ti->samplerate     = metadata->data.stream_info.sample_rate;
			ti->channels       = metadata->data.stream_info.channels;

			ti->recent_bitrate = ti->bitrate;
			ti->length         = metadata->data.stream_info.total_samples / ti->samplerate;
			ti->vbr            = 1;
			wdprintf(
				V_DEBUG,
				"flac",
				"Bitstream is %d channel(s), %d bits per sample, %ld kbps, %d Hz\n",
				ti->channels,
				metadata->data.stream_info.bits_per_sample,
				bitrate / 1000,
				ti->samplerate
			);
			break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			wdprintf(V_DEBUG, "flac", "Vorbis comment detected.\n");
			for (i = 0; i < metadata->data.vorbis_comment.num_comments; i++) {
				/*wdprintf(V_DEBUG, "flac", "%d. %s\n", i, metadata->data.vorbis_comment.comments[i].entry);*/
				char buf[80], *ptr = (char *)metadata->data.vorbis_comment.comments[i].entry;
				strtoupper(buf, ptr, 79);
				/*wdprintf(V_DEBUG, "flac", "%s\n", ptr);*/
				if (strstr(buf, "ARTIST=") == buf)
					strncpy(ti->artist, ptr+7, SIZE_ARTIST-1);
				if (strstr(buf, "TITLE=") == buf)
					strncpy(ti->title, ptr+6, SIZE_TITLE-1);
				if (strstr(buf, "ALBUM=") == buf)
					strncpy(ti->album, ptr+6, SIZE_ALBUM-1);
				if (strstr(buf, "DATE=") == buf)
					strncpy(ti->date, ptr+5, SIZE_DATE-1);
				if (strstr(buf, "TRACKNUMBER=") == buf)
					strncpy(ti->tracknr, ptr+12, SIZE_TRACKNR-1);
				/* metadata->data.vorbis_comment.comments[i].entry (.length) */
			}
			break;
		default:
			break;
	}
}

static void error_callback(const FLAC__StreamDecoder      *decoder,
                           FLAC__StreamDecoderErrorStatus  status,
                           void                           *client_data)
{
}

static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	*absolute_byte_offset = reader_get_stream_position(r);
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	*stream_length = reader_get_file_size(r);
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder, void *client_data)
{
	return reader_is_eof(r);
}

static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	FLAC__StreamDecoderSeekStatus res;
	if (reader_is_seekable(r)) {
		res = reader_seek(r, absolute_byte_offset) ? FLAC__STREAM_DECODER_SEEK_STATUS_OK : FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	} else {
		res = FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
	}
	return res;
}

static int open_file(const char *filename)
{
	int result = 1;

	total_samples = 0;
	seek_to_sample = 0;
	sample_rate = 0;
	fsd = FLAC__stream_decoder_new();
	FLAC__stream_decoder_set_metadata_respond(fsd, FLAC__METADATA_TYPE_VORBIS_COMMENT);

	trackinfo_clear(&ti);

	if (!r) {
		wdprintf(V_WARNING, "flac", "Unable to open stream: %s\n", filename);
		return 0;
	}

	file_size = reader_get_file_size(r);

	if (FLAC__stream_decoder_init_stream(fsd,
		&read_callback,
		&seek_callback,
		&tell_callback,
		&length_callback,
		&eof_callback,
		&write_callback,
		&metadata_callback,
		&error_callback,
		&ti) != FLAC__STREAM_DECODER_INIT_STATUS_OK)  {
		wdprintf(V_ERROR, "flac", "Could not initialize decoder.\n");
		result = 0;
	} else {
		if (FLAC__stream_decoder_process_until_end_of_metadata(fsd) == false) {
			wdprintf(V_ERROR, "flac", "Stream error.\n");
			result = 0;
		} else {
			wdprintf(V_INFO, "flac", "Stream ready.\n");
		}
	}
	return result;
}

static int close_file(void)
{
	FLAC__stream_decoder_finish(fsd);
	FLAC__stream_decoder_delete(fsd);
	return 0;
}

static int decode_data(char *target, size_t max_size)
{
	if (seek_to_sample) {
		if (seek_to_sample < 0) seek_to_sample = 0;
		FLAC__stream_decoder_seek_absolute(fsd, seek_to_sample);
		seek_to_sample = 0;
	}
	if (FLAC__stream_decoder_process_single(fsd) == false)
		size = 0;
	if (FLAC__stream_decoder_get_state(fsd) >= FLAC__STREAM_DECODER_END_OF_STREAM)
		size = 0;
	if (size <= max_size) {
		memcpy(target, buf, size);
		/*total_samples++;
		size += ret;*/
	} else {
		wdprintf(V_ERROR, "flac", "FATAL: Target buffer too small: %d < %d\n", max_size, size);
		size = max_size;
	}
	return size;
}

static int seek(int seconds)
{
	seek_to_sample = seconds * sample_rate;
	return 1;
}

static int get_decoder_buffer_size(void)
{
	return 65536;
}

static const char* get_file_extensions(void)
{
	return ".flac";
}

static int get_current_bitrate(void)
{
	return bitrate;
}

static int get_length(void)
{
	return track_length;
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

static const char *get_meta_data(GmuMetaDataType gmdt, int for_current_file)
{
	char *result = NULL;
	TrackInfo *ti_res = (for_current_file ? &ti : &ti_metaonly);

	switch (gmdt) {
		case GMU_META_ARTIST:
			result = ti_res->artist;
			break;
		case GMU_META_TITLE:
			result = ti_res->title;
			break;
		case GMU_META_ALBUM:
			result = ti_res->album;
			break;
		case GMU_META_TRACKNR:
			result = ti_res->tracknr;
			break;
		case GMU_META_DATE:
			result = ti_res->date;
			break;
		default:
			break;
	}
	return result;
}

static const char *get_file_type(void)
{
	return "FLAC";
}

static FLAC__StreamDecoderWriteStatus dummy_write_callback(const FLAC__StreamDecoder *decoder, 
                                                           const FLAC__Frame *frame,
                                                           const FLAC__int32 *const buffer[],
                                                           void *client_data)
{
	return 0;
}

static int meta_data_load(const char *filename)
{
	int                  result = 0;
	FILE                *file;
	FLAC__StreamDecoder *decoder;
	char                *filename_without_path = NULL;

	decoder = FLAC__stream_decoder_new(); 
	FLAC__stream_decoder_set_metadata_respond(decoder, FLAC__METADATA_TYPE_VORBIS_COMMENT);

	trackinfo_clear(&ti_metaonly);

	file = fopen(filename, "rb");
	if (file) {
		fseek(file, SEEK_END, 0);
		ti_metaonly.file_size = ftell(file);
		fseek(file, SEEK_SET, 0);
	}
	if (!file) {
		wdprintf(V_WARNING, "flac", "Could not open file.\n");
	} else if (FLAC__stream_decoder_init_FILE(decoder, file, &dummy_write_callback,
	                                          &metadata_callback, &error_callback, &ti_metaonly)
	                                         != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		wdprintf(V_ERROR, "flac", "Could not initialize decoder.\n");
	} else {
		strncpy(ti_metaonly.file_name, filename, SIZE_FILE_NAME-1);
		filename_without_path = strrchr(filename, '/');
		if (filename_without_path != NULL)
			filename_without_path++;

		filename_without_path = charset_filename_convert_alloc(
			filename_without_path ? filename_without_path : filename
		);
		strncpy(ti_metaonly.title, filename_without_path, SIZE_TITLE-1);
		free(filename_without_path);

		strncpy(ti_metaonly.file_type, "FLAC", SIZE_FILE_TYPE-1);

		if (FLAC__stream_decoder_process_until_end_of_metadata(decoder) == false) {
			wdprintf(V_ERROR, "flac", "Stream error.\n");
		} else {
			result = 1;
		}
		FLAC__stream_decoder_finish(decoder);
	}
	FLAC__stream_decoder_delete(decoder);
	return result;
}


static int meta_data_close(void)
{
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
	"FLAC_decoder",
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
