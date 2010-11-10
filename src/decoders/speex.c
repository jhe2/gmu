/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: speex.c  Created: 101109
 *
 * Description: Speex decoder
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
#include <ogg/ogg.h>
#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#include <speex/speex_callbacks.h>

#define MAX_FRAME_SIZE 2000

static ogg_sync_state   oy;
static ogg_page         og;
static ogg_packet       op;
static ogg_stream_state os;
static SpeexBits bits;
static int packet_count = 0;
static FILE *file;
static void *st = NULL;
static ogg_int64_t page_granule = 0, last_granule = 0;
static int frame_size = 0, granule_frame_size = 0;
static int skip_samples = 0, page_nb_packets;
static int nframes = 2, eos = 0, enh_enabled;
static int speex_serialno = -1;
static int rate = 0, channels = -1;
static SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT;
static int extra_headers = 0;
static int audio_size = 0;
static short output[MAX_FRAME_SIZE];
static int nb_read;
static int packet_no;
static int lookahead;
static int stream_init;

static const char *get_name(void)
{
	return "Speex decoder v0.1";
}

static void *process_header(ogg_packet *op, spx_int32_t enh_enabled, spx_int32_t *frame_size, int *granule_frame_size, spx_int32_t *rate, int *nframes, int *channels, SpeexStereoState *stereo, int *extra_headers, int quiet)
{
	void            *st;
	const SpeexMode *mode;
	SpeexHeader     *header;
	int              modeID;
	SpeexCallback    callback;

	header = speex_packet_to_header((char*)op->packet, op->bytes);
	if (!header) {
		fprintf(stderr, "speex: Cannot read header\n");
		return NULL;
	}
	if (header->mode >= SPEEX_NB_MODES || header->mode<0) {
		fprintf(stderr, "speex: Mode number %d does not (yet/any longer) exist in this version\n", 
		        header->mode);
		free(header);
		return NULL;
	}

	modeID = header->mode;
	mode = speex_lib_get_mode(modeID);

	if (header->speex_version_id > 1) {
		fprintf(stderr, "speex: This file was encoded with Speex bit-stream version %d, which I don't know how to decode\n", header->speex_version_id);
		free(header);
		return NULL;
	}

	if (mode->bitstream_version < header->mode_bitstream_version) {
		fprintf(stderr, "speex: The file was encoded with a newer version of Speex. You need to upgrade in order to play it.\n");
		free(header);
		return NULL;
	}
	if (mode->bitstream_version > header->mode_bitstream_version) {
		fprintf(stderr, "speex: The file was encoded with an older version of Speex. You would need to downgrade the version in order to play it.\n");
		free(header);
		return NULL;
	}

	st = speex_decoder_init(mode);
	if (!st) {
		fprintf(stderr, "speex: Decoder initialization failed.\n");
		free(header);
		return NULL;
	}
	speex_decoder_ctl(st, SPEEX_SET_ENH, &enh_enabled);
	speex_decoder_ctl(st, SPEEX_GET_FRAME_SIZE, frame_size);
	*granule_frame_size = *frame_size;

   if (!*rate) *rate = header->rate;

   speex_decoder_ctl(st, SPEEX_SET_SAMPLING_RATE, rate);

   *nframes = header->frames_per_packet;

   if (*channels == -1)
      *channels = header->nb_channels;

   if (!(*channels == 1)) {
      *channels = 2;
      callback.callback_id = SPEEX_INBAND_STEREO;
      callback.func = speex_std_stereo_request_handler;
      callback.data = stereo;
      speex_decoder_ctl(st, SPEEX_SET_HANDLER, &callback);
   }
   
   if (!quiet) {
      fprintf(stderr, "Decoding %d Hz audio using %s mode", 
              *rate, mode->modeName);

      if (*channels == 1)
         fprintf (stderr, " (mono");
      else
         fprintf (stderr, " (stereo");
      
      if (header->vbr)
         fprintf (stderr, ", VBR)\n");
      else
         fprintf(stderr, ")\n");
      /*fprintf(stderr, "Decoding %d Hz audio at %d bps using %s mode\n", 
              *rate, mode->bitrate, mode->modeName);*/
   }

   *extra_headers = header->extra_headers;

   free(header);
   return st;
}

static int read_block(void)
{
	int res = 0;
	char *data;
	
	/* Get the ogg buffer for writing */
	data = ogg_sync_buffer(&oy, 200);
	if (data) {
		/* Read bitstream from input file */
		nb_read = fread(data, sizeof(char), 200, file);      
		ogg_sync_wrote(&oy, nb_read);
		if (nb_read > 0) res = 1;
	}
	return res;
}

static int process_block_data(void)
{
	int res = 0;
	/* Read one complete page */
	if (ogg_sync_pageout(&oy, &og) == 1) {
		if (stream_init == 0) {
			printf("speex: Initializing Ogg stream.\n");
			ogg_stream_init(&os, ogg_page_serialno(&og));
			stream_init = 1;
		}
		if (ogg_page_serialno(&og) != os.serialno) {
			/* so all streams are read. */
			ogg_stream_reset_serialno(&os, ogg_page_serialno(&og));
		}
		/* Add page to the bitstream */
		ogg_stream_pagein(&os, &og);
		page_granule = ogg_page_granulepos(&og);
		page_nb_packets = ogg_page_packets(&og);
		if (page_granule > 0 && frame_size) {
			skip_samples = frame_size * (page_nb_packets * granule_frame_size * nframes - (page_granule - last_granule)) / granule_frame_size;
			if (ogg_page_eos(&og))
				skip_samples = -skip_samples;
			/*else if (!ogg_page_bos(&og))
				skip_samples = 0;*/
		} else {
			skip_samples = 0;
		}
		printf("speex: page granulepos: %d %d %d\n", skip_samples, page_nb_packets, (int)page_granule);
		last_granule = page_granule;
		packet_no = 0;
		res = 1;
	}
	return res;
}

static int open_file(char *filename)
{
	int res = 1;

	ogg_sync_init(&oy);
	speex_bits_init(&bits);

	eos = 0;
	packet_no = 0;
	audio_size = 0;
	stream_init = 0;
	printf("speex: Opening %s...\n", filename);
	if (!(file = fopen(filename, "rb"))) {
		printf("speex: Could not open file.\n");
		res = 0;
	} else {
		st = 0;
		read_block();
		process_block_data();
		if (ogg_stream_packetout(&os, &op) == 1) { /* Process first packet as Speex header */
			st = process_header(&op, enh_enabled, &frame_size, &granule_frame_size, &rate, &nframes, &channels, &stereo, &extra_headers, 0);
			if (op.bytes >= 5 && !memcmp(op.packet, "Speex", 5)) {
				speex_serialno = os.serialno;
			}
		}
		if (!st) {
			res = 0;
			fclose(file);
			printf("speex: Problem with Speex file.\n");
		} else {
			speex_decoder_ctl(st, SPEEX_GET_LOOKAHEAD, &lookahead);
			if (!nframes) nframes = 1;
			printf("speex: File opened okay!\n");
			printf("speex: Found file with %d channel(s) and %d Hz\n", channels, rate);
		}
	}
	return res;
}

static int close_file(void)
{
	if (st) speex_decoder_destroy(st);
	speex_bits_destroy(&bits);
	if (stream_init) ogg_stream_clear(&os);
	ogg_sync_clear(&oy);
	if (file) fclose(file);
	printf("speex: File closed.\n");
	return 0;
}

static int decode_data(char *target, int max_size)
{
	int size = 1, ret = 1;
	int i, j;

	if (!eos) {
		int osp = ogg_stream_packetout(&os, &op);
		while (osp == 0) {
			if (!process_block_data()) {
				if (!read_block()) break;
			}
			osp = ogg_stream_packetout(&os, &op);
		}
		if (op.bytes >= 5 && !memcmp(op.packet, "Speex", 5)) {
			speex_serialno = os.serialno;
		}
		if (speex_serialno == -1 || os.serialno != speex_serialno) {
			/* nothing */
		} else {
			if (packet_count == 1) { /* comments packet */
				/*print_comments((char*)op.packet, op.bytes);*/
			} else if (packet_count <= 1 + extra_headers) {
				/* Ignore extra headers */
			} else {
				packet_no++;

				/* End of stream condition */
				if (op.e_o_s && os.serialno == speex_serialno) /* don't care for anything except speex eos */
					eos = 1;

				/* Copy Ogg packet to Speex bitstream */
				speex_bits_read_from(&bits, (char*)op.packet, op.bytes);
				for (j = 0; j != nframes; j++) {
					int ret;
					/* Decode frame */
					ret = speex_decode_int(st, &bits, output);

					if (ret == -1) {
						break;
					}
					if (ret == -2) {
						fprintf(stderr, "speex: Decoding error: corrupted stream?\n");
						break;
					}
					if (speex_bits_remaining(&bits) < 0) {
						fprintf(stderr, "speex: Decoding overflow: corrupted stream?\n");
						break;
					}
					if (channels == 2)
						speex_decode_stereo_int(output, frame_size, &stereo);

					if (0) { // print bitrate
						spx_int32_t tmp;
						char ch = 13;
						speex_decoder_ctl(st, SPEEX_GET_BITRATE, &tmp);
						fputc(ch, stderr);
						fprintf(stderr, "speex: Bitrate is: %d bps\n", tmp);
					}

					{
						int frame_offset = 0;
						int new_frame_size = frame_size;
						/*printf ("packet %d %d\n", packet_no, skip_samples);*/
						/*fprintf (stderr, "packet %d %d %d\n", packet_no, skip_samples, lookahead);*/
						if (packet_no == 1 && j == 0 && skip_samples > 0) {
							printf("speex: Chopping first packet.\n");
							new_frame_size -= skip_samples + lookahead;
							frame_offset = skip_samples + lookahead;
						}
						if (packet_no == page_nb_packets && skip_samples < 0) {
							int packet_length = nframes * frame_size + skip_samples + lookahead;
							new_frame_size = packet_length - j * frame_size;
							if (new_frame_size < 0)
							   new_frame_size = 0;
							if (new_frame_size > frame_size)
							   new_frame_size = frame_size;
							printf ("speex: Chopping end: %d %d %d\n", new_frame_size, packet_length, packet_no);
						}

						if (new_frame_size > 0) {
							char *audio = (char *)output;
							for (i = 0; i < frame_size * channels * 2 && i < max_size; i++)
								target[i] = audio[i];
							size = i;
							//printf("\nnew_frame_size = %d size = %d max_size = %d\n", new_frame_size, size, max_size);
							audio_size += sizeof(short) * new_frame_size * channels;
							//printf("audio_size = %d\n", audio_size);
						}
					}
				}
			}
			packet_count++;
		}
	} else {
		size = 0;
	}
	return size;
}

static int seek(int seconds)
{
	int  unsuccessful = 1;
	/*long pos = seconds * 1000;

	if (pos <= 0) pos = 0;*/
	return !unsuccessful;
}

static int get_decoder_buffer_size(void)
{
	return 2000;
}

static const char* get_file_extensions(void)
{
	return ".spx";
}

static int get_current_bitrate(void)
{
	return 0; //ov_bitrate_instant(&vf);
}

static int get_length(void)
{
	return 0; //ov_time_total(&vf, -1) / 1000;
}

static int get_samplerate(void)
{
	return rate;
}

static int get_channels(void)
{
	return channels;
}

static int get_bitrate(void)
{
	return 0; //ov_bitrate(&vf, -1);
}

static const char *get_meta_data(GmuMetaDataType gmdt, int for_current_file)
{
	char *result = NULL;
	char **ptr   = NULL;

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
	}*/
	return result;
}

const char *get_file_type(void)
{
	return "Ogg Speex";
}

int meta_data_load(const char *filename)
{
	FILE *file;
	int   result = 0;

	/*if ((file = fopen(filename, "r"))) {
		if (ov_open(file, &vf_metaonly, NULL, 0) < 0) {
			printf("vorbis: Input does not appear to be an Ogg bitstream.\n");
			result = 0;
		}
	} else {
		result = 0;
	}*/
	return result;
}

int meta_data_close(void)
{
	//ov_clear(&vf_metaonly);
	return 1;
}

GmuCharset meta_data_get_charset(void)
{
	return M_CHARSET_UTF_8;
}

static GmuDecoder gd = {
	"speex_decoder",
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

GmuDecoder *gmu_register_decoder(void)
{
	return &gd;
}
