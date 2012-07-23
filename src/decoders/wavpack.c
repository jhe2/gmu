/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: wavpack.c  Created: 081102
 *
 * Description: Wavpack decoder
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
#include "wavpack/wavpack.h"
#include "../debug.h"

static int32_t         temp_buffer[256];
static WavpackContext *wpc;
static long            total_unpacked_samples;
static int             get_channels(void);
static FILE           *file;

static int32_t read_bytes(void *buff, int32_t bcount)
{
    return fread(buff, 1, bcount, file);
}

static uchar *format_samples(int bps, uchar *dst, int32_t *src, uint32_t samcnt)
{
    int32_t temp;

    switch (bps) {
        case 1:
            while (samcnt--)
                *dst++ = *src++ + 128;
            break;
        case 2:
            while (samcnt--) {
                *dst++ = (uchar)(temp = *src++);
                *dst++ = (uchar)(temp >> 8);
            }
            break;
        case 3:
            while (samcnt--) {
                *dst++ = (uchar)(temp = *src++);
                *dst++ = (uchar)(temp >> 8);
                *dst++ = (uchar)(temp >> 16);
            }
            break;
        case 4:
            while (samcnt--) {
                *dst++ = (uchar)(temp = *src++);
                *dst++ = (uchar)(temp >> 8);
                *dst++ = (uchar)(temp >> 16);
                *dst++ = (uchar)(temp >> 24);
            }
            break;
    }

    return dst;
}

static const char *get_name(void)
{
	return "WavPack decoder v0.2";
}

static int open_file(char *filename)
{
	char  error[80];
	wdprintf(V_INFO, "wavpack", "Opening %s ...", filename);
	wpc = 0;
	if ((file = fopen(filename, "r"))) {
		wpc = WavpackOpenFileInput(read_bytes, error);
		total_unpacked_samples = 0;
		wdprintf(V_DEBUG, "wavpack", "Status: %s", wpc ? "OK" : "Error");
		/*if (wpc) {
			total_samples = WavpackGetNumSamples (wpc);
			bps = WavpackGetBytesPerSample (wpc);
		}*/
	}
	return (wpc ? 1 : 0);
}

static int close_file(void)
{
	/*WavpackCloseFile(wpc);*/
	fclose(file);
	return 0;
}

static int decode_data(char *target, int max_size)
{
	int      bps, channels;
	uint32_t samples_unpacked = 0;

	channels = get_channels();
	bps = WavpackGetBytesPerSample(wpc);
	if (max_size >= 1024) {
		samples_unpacked = WavpackUnpackSamples(wpc, temp_buffer, 256 / channels);
		total_unpacked_samples += samples_unpacked;
		if (samples_unpacked)
            format_samples(bps, (uchar *)target, temp_buffer, samples_unpacked * channels);
	} else {
		wdprintf(V_ERROR, "wavpack", "Target buffer too small: %d < 1024\n", max_size);
	}
	return samples_unpacked * (WavpackGetBitsPerSample(wpc) / 4);
}

static int seek(int seconds)
{
	int  unsuccessful = 1;
	long pos = seconds * 1000;

	if (pos <= 0) pos = 0;
	/*unsuccessful = ov_time_seek_page(&vf, pos);*/
	return !unsuccessful;
}

static int get_decoder_buffer_size(void)
{
	return 1024;
}

static const char* get_file_extensions(void)
{
	return ".wv;.wvc";
}

static int get_current_bitrate(void)
{
	return 0; /* WavpackGetInstantBitrate(wpc);*/
}

static int get_length(void)
{
	return WavpackGetNumSamples(wpc) / WavpackGetSampleRate(wpc);
}

static int get_samplerate(void)
{
	return WavpackGetSampleRate(wpc);
}

static int get_channels(void)
{
	return WavpackGetReducedChannels(wpc);
}

static int get_bitrate(void)
{
	return 0; /*WavpackGetAverageBitrate(wpc);*/
}

static const char *get_meta_data(GmuMetaDataType gmdt, int for_current_file)
{
	char *result = NULL;
	/*char **ptr   = NULL;*/

	/*if (for_current_file) {
		ptr = ov_comment(&vf, -1)->user_comments;
	} else {
		ptr = ov_comment(&vf_metaonly, -1)->user_comments;
	}

	while (*ptr) {
		char buf[80];
		strtoupper(buf, *ptr, 79);				
		switch (gmdt) {
			case GMU_META_ARTIST:
				if (strstr(buf, "ARTIST=") != NULL)
					result = *ptr+7;
				break;
			case GMU_META_TITLE:
				if (strstr(buf, "TITLE=") != NULL)
					result = *ptr+6;
				break;
			case GMU_META_ALBUM:
				if (strstr(buf, "ALBUM=") != NULL)
					result = *ptr+6;
				break;
			case GMU_META_TRACKNR:
				if (strstr(buf, "TRACKNUMBER=") != NULL)
					result = *ptr+12;
				break;
			case GMU_META_DATE:
				if (strstr(buf, "DATE=") != NULL)
					result = *ptr+5;
				break;
			default:
				break;
		}
		ptr++;
	}*/
	return result;
}

static const char *get_file_type(void)
{
	return "WavPack";
}

static int meta_data_load(const char *filename)
{
	/*FILE *file;*/
	int   result = 0;

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

static GmuDecoder gd = {
	"wavpack_decoder",
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
