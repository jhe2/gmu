/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: musepack.c  Created: 081022
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
#include "../id3.h"
#include "../util.h"
#include "mpcdec/mpcdec.h"
#include "../debug.h"

typedef struct reader_data_t {
    FILE      *file;
    long       size;
    mpc_bool_t seekable;
} reader_data;

static mpc_decoder  decoder;
static mpc_reader   reader;
static reader_data  data;

static long         seek_to_sample_offset;
static int          sample_rate, channels, length, bitrate;
static FILE        *file;
static TrackInfo    ti, ti_metaonly;

/*
  Our implementations of the mpc_reader callback functions.
*/
static mpc_int32_t read_impl(void *data, void *ptr, mpc_int32_t size)
{
    reader_data *d = (reader_data *) data;
    return fread(ptr, 1, size, d->file);
}

static mpc_bool_t seek_impl(void *data, mpc_int32_t offset)
{
    reader_data *d = (reader_data *) data;
    return d->seekable ? !fseek(d->file, offset, SEEK_SET) : FALSE;
}

static mpc_int32_t tell_impl(void *data)
{
    reader_data *d = (reader_data *) data;
    return ftell(d->file);
}

static mpc_int32_t get_size_impl(void *data)
{
    reader_data *d = (reader_data *) data;
    return d->size;
}

static mpc_bool_t canseek_impl(void *data)
{
    reader_data *d = (reader_data *) data;
    return d->seekable;
}

#define WFX_SIZE (2+2+4+4+2+2)

#ifdef MPC_FIXED_POINT
static int shift_signed(MPC_SAMPLE_FORMAT val, int shift)
{
    if (shift > 0)
        val <<= shift;
    else if (shift < 0)
        val >>= -shift;
    return (int)val;
}
#endif

static const char *get_name(void)
{
	return "Musepack decoder v0.9";
}

static int open_file(char *filename)
{
	int   result = 1;
	/*char *filename_without_path;*/

	seek_to_sample_offset = 0;

	wdprintf(V_INFO, "musepack", "Opening %s...\n", filename);
	if (!(file = fopen(filename, "r"))) {
		wdprintf(V_WARNING, "musepack", "Could not open file.\n");
		result = 0;
	} else {
		mpc_streaminfo info;
		trackinfo_clear(&ti);
		/*strncpy(ti->file_name, mpc_file, SIZE_FILE_NAME-1);
		filename_without_path = strrchr(mpc_file, '/');
		if (filename_without_path != NULL)
			filename_without_path++;
		else
			filename_without_path = mpc_file;

		filename_without_path = charset_filename_convert_alloc(filename_without_path);
		strncpy(ti->title, filename_without_path, SIZE_TITLE-1);
		free(filename_without_path);

		strncpy(ti->file_type, "Musepack", SIZE_FILE_TYPE-1);*/
		id3_read_id3v1(file, &ti, "Musepack");
		fseek(file, 0, SEEK_SET);

		/* initialize our reader_data tag the reader will carry around with it */
		data.file = file;
		data.seekable = TRUE;
		fseek(data.file, 0, SEEK_END);
		data.size = ftell(data.file);
		fseek(data.file, 0, SEEK_SET);

		/* set up an mpc_reader linked to our function implementations */
		reader.read = read_impl;
		reader.seek = seek_impl;
		reader.tell = tell_impl;
		reader.get_size = get_size_impl;
		reader.canseek = canseek_impl;
		reader.data = &data;

		/* read file's streaminfo data */
		mpc_streaminfo_init(&info);
    	if (mpc_streaminfo_read(&info, &reader) != ERROR_CODE_OK) {
			wdprintf(V_WARNING, "musepack", "Not a valid musepack file: \"%s\"\n", filename);
			result = 0;
		} else {
			sample_rate = info.sample_freq;
			channels    = info.channels;
			bitrate     = info.average_bitrate;
			length      = info.total_file_length / (int)(info.average_bitrate / 8);

			/* instantiate a decoder with our file reader */
			mpc_decoder_setup(&decoder, &reader);
			if (!mpc_decoder_initialize(&decoder, &info)) {
				wdprintf(V_ERROR, "musepack", "Error initializing decoder.\n");
				result = 0;
			}
		}
	}
	return result;
}

static int close_file(void)
{
	fclose(file);
	trackinfo_clear(&ti);
	return 0;
}

static int decode_data(char *target, int max_size)
{
	int               size = MPC_DECODER_BUFFER_LENGTH;
	unsigned          total_samples = 0;
	mpc_bool_t        successful = FALSE;
	MPC_SAMPLE_FORMAT sample_buffer[MPC_DECODER_BUFFER_LENGTH];
	mpc_int16_t       pcmbuf[MPC_DECODER_BUFFER_LENGTH];
	unsigned          status;

	memset(sample_buffer, 0, sizeof(MPC_SAMPLE_FORMAT) * MPC_DECODER_BUFFER_LENGTH);

	status = mpc_decoder_decode(&decoder, sample_buffer, 0, 0);
	if (status == (unsigned)(-1)) {
		size = 0;
		wdprintf(V_ERROR, "musepack", "Error decoding file.\n");
	} else if (status == 0) { /* EOF */
		successful = TRUE;
		size = 0;
	} else { /* status > 0 */
		unsigned n;
		int m_bps = 16; /* ? */
		int clip_min = -1 << (m_bps - 1),
			clip_max = (1 << (m_bps - 1)) - 1;
#ifndef MPC_FIXED_POINT
		int float_scale = 1 << (m_bps - 1);
#endif
		for (n = 0; n < MPC_DECODER_BUFFER_LENGTH; n++) {
			int val;
#ifdef MPC_FIXED_POINT
			val = shift_signed(sample_buffer[n], m_bps - MPC_FIXED_POINT_SCALE_SHIFT);
#else
			val = (int)(sample_buffer[n] * float_scale);
#endif
			if (val < clip_min)
				val = clip_min;
			else if (val > clip_max)
				val = clip_max;
			pcmbuf[n] = val;
		}

		if (max_size > MPC_DECODER_BUFFER_LENGTH) {
			memcpy(target, pcmbuf, MPC_DECODER_BUFFER_LENGTH);
		} else {
			wdprintf(V_ERROR, "musepack", "Target buffer too small: %d < %d\n", max_size, MPC_DECODER_BUFFER_LENGTH);
			size = 0;
		}

		total_samples += status;
		/*if (seek_to_sample_offset) {*/
			/* do seeking */
		/*	if (audio_get_sample_count() + seek_to_sample_offset <= 0)
				seek_to_sample_offset = -audio_get_sample_count();
			mpc_decoder_seek_sample(&decoder, audio_get_sample_count() + seek_to_sample_offset);
			audio_increase_sample_counter(seek_to_sample_offset);
			seek_to_sample_offset = 0;
		}*/
	}

	return size;
}

static int seek(int second)
{
	int seek_to_sample = (second > 0 ? second * sample_rate : 0);
	mpc_decoder_seek_sample(&decoder, seek_to_sample);
	return 1;
}

static int get_decoder_buffer_size(void)
{
	return MPC_DECODER_BUFFER_LENGTH;
}

static const char* get_file_extensions(void)
{
	return ".mpc;.mp+";
}

static int get_current_bitrate(void)
{
	return 0;
}

static int get_length(void)
{
	return length;
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

	if (for_current_file) {
		switch (gmdt) {
			case GMU_META_ARTIST:
				result = trackinfo_get_artist(&ti);
				break;
			case GMU_META_TITLE:
				result = trackinfo_get_title(&ti);
				break;
			case GMU_META_ALBUM:
				result = trackinfo_get_album(&ti);
				break;
			case GMU_META_TRACKNR:
				result = trackinfo_get_tracknr(&ti);
				break;
			case GMU_META_DATE:
				result = trackinfo_get_date(&ti);
				break;
			default:
				break;
		}
	} else {
		switch (gmdt) {
			case GMU_META_ARTIST:
				result = trackinfo_get_artist(&ti_metaonly);
				break;
			case GMU_META_TITLE:
				result = trackinfo_get_title(&ti_metaonly);
				break;
			case GMU_META_ALBUM:
				result = trackinfo_get_album(&ti_metaonly);
				break;
			case GMU_META_TRACKNR:
				result = trackinfo_get_tracknr(&ti_metaonly);
				break;
			case GMU_META_DATE:
				result = trackinfo_get_date(&ti_metaonly);
				break;
			default:
				break;
		}
	}
	return result;
}

static int meta_data_close(void)
{
	trackinfo_clear(&ti_metaonly);
	return 1;
}

static const char *get_file_type(void)
{
	return "Musepack";
}

static int meta_data_load(const char *filename)
{
	FILE *file = fopen(filename, "r");
	int   result = 0;
	if (file) {
		result = id3_read_id3v1(file, &ti_metaonly, "Musepack");
		fclose(file);
	}
	return result;
}

static GmuCharset meta_data_get_charset(void)
{
	return M_CHARSET_ISO_8859_1;
}

static GmuDecoder gd = {
	"mpc_decoder",
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
