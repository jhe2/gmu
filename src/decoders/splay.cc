/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2008 Johannes Heimansberg (wejp.k.vu)
 *
 * File: splay.cc  Created: 081028
 *
 * Description: splay mp3 decoder
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
extern "C"  {
#include "../gmudecoder.h"
#include "../trackinfo.h"
#include "../id3.h"
#include "../util.h"
}
#include "splay/mpegsound.h"

static Mpegfileplayer *player = NULL;
static int             sample_rate, channels, length, bitrate;
static TrackInfo       ti, ti_metaonly;

static const char *get_name(void)
{
	return "Splay MP3 decoder v0.1";
}

static int open_file(char *filename)
{
	int result = 1;

	player = new Mpegfileplayer;

	id3_read_tag(filename, &ti, "MP3");
	if (!player->openfile(filename, (char *)"-")) {
		fprintf(stderr, "splay: Error opening file \"%s\".\n", filename);
		delete player;
		result = 0;
	} else if (!player->init_mpeg_layer_3()) {
		fprintf(stderr, "splay: Error initializing decoder.\n");
		delete player;
		result = 0;
	} else {
		sample_rate = player->get_frequency();
		channels    = player->get_channels();
		bitrate     = player->get_avg_bitrate();
		length      = player->get_length();
		//ti->vbr        = player->is_vbr() ? 1 : 0;
		fprintf(stderr, "splay: Length = %d sec (%d frames)\n",
		        player->get_length(), player->get_frames());
	}
	return result;
}

static int close_file(void)
{
	delete player;
	trackinfo_clear(&ti);
	return 0;
}

static int decode_data(char *target, int max_size)
{
	int ret = 0;
	ret = player->decode_frame();
	if (player->get_buffer_size() <= max_size)
		memcpy(target, (char *)player->get_buffer(), player->get_buffer_size());
	else
		printf("splay: Target buffer too small.\n");
	return ret ? player->get_buffer_size() : 0;
}

static int seek(int offset)
{
	int  unsuccessful = 1;
	return !unsuccessful;
}

static int get_decoder_buffer_size(void)
{
	return player->get_buffer_size();
}

static const char* get_file_extensions(void)
{
	return ".mp3";
}

static int get_current_bitrate(void)
{
	return player->get_bitrate() * 1000;
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

const char *get_file_type(void)
{
	return "MP3";
}

int meta_data_load(const char *filename)
{
	return id3_read_tag(filename, &ti_metaonly, "MP3");
}

int meta_data_close(void)
{
	trackinfo_clear(&ti_metaonly);
	return 1;
}

GmuCharset meta_data_get_charset(void)
{
	return M_CHARSET_ISO_8859_1;
}

static GmuDecoder gd = {
	"splay_mp3_decoder",
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
	meta_data_get_charset
};

extern "C" GmuDecoder *gmu_register_decoder(void)
{
	return &gd;
}
