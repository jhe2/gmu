/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: fileplayer.c  Created: 070107
 *
 * Description: Functions for audio file playback
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "SDL/SDL.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "fileplayer.h"
#include "audio.h"
#include "trackinfo.h"
#include "id3.h"
#include "util.h"
#include "pbstatus.h"
#include "gmudecoder.h"
#include "decloader.h"
#include "charset.h"
#include "debug.h"
#include "reader.h"
#include "core.h"
#include "eventqueue.h"

#define BUF_SIZE 65536

static char      lyrics_file_pattern[256];
static long      seek_second;
static int       meta_data_loaded = 0;
static int       thread_running = 0;
static int       file_player_shut_down = 0;

/* Overall playback status. When this is set to PLAYING, it indicates that 
 * the file player should continue playing with the next track in the
 * playlist, after the current one has finished. */
static PB_Status playback_status = STOPPED;

/* Playback status for current item. This is set to PLAYING as soon as a
 * track starts playing (decode_audio_thread is started) and is set to
 * STOPPED as soon as the track finishes. When setting this to STOPPED
 * the decoder thread stops as soon as possible. Use 
 * file_player_stop_playback() to stop playback. */
static PB_Status item_status;

typedef struct decode_audio_params
{
	GmuDecoder *gd;
	char       *file;
	TrackInfo  *ti;
	Reader     *r;
} decode_audio_params; 

void file_player_set_lyrics_file_pattern(char *pattern)
{
	strncpy(lyrics_file_pattern, pattern, 255);
}

int file_player_playback_get_time(void)
{
 	return audio_get_playtime();
}

PB_Status file_player_get_playback_status(void)
{
	return playback_status;
}

PB_Status file_player_get_item_status(void)
{
	return item_status;
}

int file_player_is_thread_running(void)
{
	return thread_running;
}

void file_player_shutdown(void)
{
	playback_status = STOPPED;
	item_status = STOPPED;
	file_player_shut_down = 1;
}

void file_player_stop_playback(void)
{
	playback_status = STOPPED;
	item_status = STOPPED;
	wdprintf(V_INFO, "fileplayer", "Stop playback!\n");
}

static int strncpy_charset_conv(char *target, const char* source, int target_size,
                                int source_size, GmuCharset charset)
{
	switch (charset) {
		case M_CHARSET_ISO_8859_1:
		case M_CHARSET_ISO_8859_15:
			strncpy(target, source, target_size);
			break;
		case M_CHARSET_UTF_8:
			charset_utf8_to_iso8859_1(target, source, target_size);
			break;
		case M_CHARSET_UTF_16_BOM:
			charset_utf16_to_iso8859_1(target, target_size, source, source_size, BOM);
			break;
		case M_CHARSET_UTF_16_BE:
			charset_utf16_to_iso8859_1(target, target_size, source, source_size, BE);
			break;
		case M_CHARSET_UTF_16_LE:
			charset_utf16_to_iso8859_1(target, target_size, source, source_size, LE);
			break;
		case M_CHARSET_AUTODETECT:
			wdprintf(V_DEBUG, "fileplayer", "Charset autodetect!\n");
			if (!charset_utf8_to_iso8859_1(target, source, target_size))
				if (!charset_utf16_to_iso8859_1(target, target_size, source, source_size, BOM))
					strncpy(target, source, target_size);
			break;
	}
	return 0;
}

/* Return 1 when new meta data differs from previous data, 0 otherwise */
static int update_metadata(GmuDecoder *gd, TrackInfo *ti, GmuCharset charset)
{
	TrackInfo ti_tmp;
	int       differ = 0;

	if (*gd->get_meta_data) {
		memcpy(&ti_tmp, ti, sizeof(TrackInfo));
		if ((*gd->get_meta_data)(GMU_META_ARTIST, 1))
			strncpy_charset_conv(ti_tmp.artist,  (*gd->get_meta_data)(GMU_META_ARTIST, 1), SIZE_ARTIST-1, 0, charset);
		if ((*gd->get_meta_data)(GMU_META_TITLE, 1))
			strncpy_charset_conv(ti_tmp.title,   (*gd->get_meta_data)(GMU_META_TITLE, 1), SIZE_TITLE-1, 0, charset);
		if ((*gd->get_meta_data)(GMU_META_ALBUM, 1))
			strncpy_charset_conv(ti_tmp.album,   (*gd->get_meta_data)(GMU_META_ALBUM, 1), SIZE_ALBUM-1, 0, charset);
		if ((*gd->get_meta_data)(GMU_META_TRACKNR, 1))
			strncpy_charset_conv(ti_tmp.tracknr, (*gd->get_meta_data)(GMU_META_TRACKNR, 1), SIZE_TRACKNR-1, 0, charset);
		if ((*gd->get_meta_data)(GMU_META_DATE, 1))
			strncpy_charset_conv(ti_tmp.date,    (*gd->get_meta_data)(GMU_META_DATE, 1), SIZE_DATE-1, 0, charset);
		/* Check if new dataset differs from old dataset: */
		if (strncmp(ti_tmp.title, ti->title, SIZE_TITLE) != 0)
			differ = 1;
		else if (strncmp(ti_tmp.artist, ti->artist, SIZE_ARTIST) != 0)
			differ = 1;
		else if (strncmp(ti_tmp.album, ti->album, SIZE_ALBUM) != 0)
			differ = 1;

		if (differ) {
			memcpy(ti, &ti_tmp, sizeof(TrackInfo));
			if (*gd->get_meta_data_int) {
				if ((*gd->get_meta_data_int)(GMU_META_IMAGE_DATA_SIZE, 1) &&
				   ((*gd->get_meta_data)(GMU_META_IMAGE_DATA, 1)) &&
				   (*gd->get_meta_data)(GMU_META_IMAGE_MIME_TYPE, 1))
					trackinfo_set_image(ti, (char *)((*gd->get_meta_data)(GMU_META_IMAGE_DATA, 1)),
										(*gd->get_meta_data_int)(GMU_META_IMAGE_DATA_SIZE, 1),
										(char *)((*gd->get_meta_data)(GMU_META_IMAGE_MIME_TYPE, 1)));
			}
			trackinfo_set_updated(ti);
		}
	}
	return differ;
}

static void *decode_audio_thread(void *udata)
{
	char       *file = ((decode_audio_params *)udata)->file;
	TrackInfo  *ti   = ((decode_audio_params *)udata)->ti;
	GmuDecoder *gd   = ((decode_audio_params *)udata)->gd;
	Reader     *r    = ((decode_audio_params *)udata)->r;
	long        ret  = 1;
	static char pcmout[BUF_SIZE];
	GmuCharset  charset = M_CHARSET_AUTODETECT;

	if (!file_player_shut_down) {
		if (*gd->meta_data_get_charset) charset = (*gd->meta_data_get_charset)();

		if (thread_running) {
			wdprintf(V_DEBUG, "fileplayer", "Waiting for other thread to finish...\n");
			item_status = STOPPED;
			while (thread_running) SDL_Delay(50); /* if thread_running never gets false this threads deadlocks here, should not happen though! */
			wdprintf(V_DEBUG, "fileplayer", "Other thread finished. Continuing with new thread!\n");
		}
		thread_running = 1;
		playback_status = item_status = PLAYING;

		if (gd->set_reader_handle) (*gd->set_reader_handle)(r);

		if (!file_player_shut_down && (*gd->open_file)(file)) {
			trackinfo_clear(ti);
			strncpy(ti->file_name, file, SIZE_FILE_NAME-1);

			/* Assume 44.1 kHz stereo as default */
			ti->samplerate = 44100;
			ti->channels   = 2;
			ti->bitrate    = 0;

			if (*gd->get_samplerate)
				ti->samplerate = (*gd->get_samplerate)();
			if (*gd->get_channels)
				ti->channels   = (*gd->get_channels)();
			if (*gd->get_bitrate)
				ti->bitrate    = (*gd->get_bitrate)();
			if (*gd->get_length)
				ti->length     = (*gd->get_length)();
			if (*gd->get_file_type)
				strncpy_charset_conv(ti->file_type, (*gd->get_file_type)(),
									 SIZE_FILE_TYPE-1, 0, charset);

			if (ti->channels > 0) {
				wdprintf(V_INFO, "fileplayer", "Found %s stream w/ %d channel(s), %d Hz, %ld bps, %d seconds\n",
					     ti->file_type, ti->channels, ti->samplerate, ti->bitrate, ti->length);

				if (!trackinfo_has_lyrics(ti)) {
					char *lyrics_file = get_file_matching_given_pattern_alloc(file, lyrics_file_pattern);
					if (lyrics_file) {
						wdprintf(V_DEBUG, "fileplayer", "Trying to load lyrics from file %s...\n", lyrics_file);
						if (trackinfo_load_lyrics_from_file(ti, lyrics_file))
							wdprintf(V_DEBUG, "fileplayer", "Loading lyrics was successful.\n");
						else
							wdprintf(V_WARNING, "fileplayer", "Loading lyrics from file failed.\n");
						free(lyrics_file);
					}
					/*wdprintf(V_DEBUG, "fileplayer", "LYRICS:%s\n",ti->lyrics);*/
				}

				if (audio_device_open(ti->samplerate, ti->channels) < 0) {
					wdprintf(V_ERROR, "fileplayer", "Couldn't open audio: %s\n", SDL_GetError());
				} else {
					wdprintf(V_DEBUG, "fileplayer", "Audio device ready!\n");
				}
				charset_utf8_to_iso8859_1(ti->file_name, file, SIZE_FILE_NAME-1);

				/* read meta data */
				if (update_metadata(gd, ti, charset))
					event_queue_push(gmu_core_get_event_queue(), GMU_TRACKINFO_CHANGE);
				meta_data_loaded = 1;

				audio_set_pause(0);
				SDL_PauseAudio(1);

				while (ret && item_status == PLAYING && !file_player_shut_down) {
					int size = 0, ret = 1, br = 0;

					if (seek_second) {
						if (item_status == PLAYING && (!gd->set_reader_handle || reader_is_seekable(r))) {
							if (seek_second < 0) seek_second = 0;
							if (*gd->seek)
								if ((*gd->seek)(seek_second))
									audio_set_sample_counter(seek_second * ti->samplerate);
						}
						seek_second = 0;
					}
					while (ret > 0 && size < BUF_SIZE / 2 && item_status == PLAYING) {
						ret = (*gd->decode_data)(pcmout+size, BUF_SIZE-size);
						size += ret;
					}
					if (gd->get_current_bitrate) br = (*gd->get_current_bitrate)();
					if (br > 0) ti->recent_bitrate = br;
					if (ret == 0) {
						audio_set_pause(1);
						item_status = FINISHED;
						break;
					} else if (ret < 0) {
						wdprintf(V_ERROR, "fileplayer", "Error. Code: %d\n", ret);
						audio_set_pause(1);
						item_status = FINISHED;
						break;
					} else {
						int r = 0;
						while (!r && item_status == PLAYING) {
							r = audio_fill_buffer(pcmout, size);
							SDL_Delay(10);
							/*usleep(5);*/
						}
						if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING &&
							!audio_get_pause() && playback_status == PLAYING &&
							audio_buffer_get_fill() > audio_buffer_get_size() / 2) {
							wdprintf(V_DEBUG, "fileplayer", "UNPAUSE AUDIO!\n");
							SDL_PauseAudio(0);
						}
					}
					if (*gd->get_meta_data_int) {
						if ((*gd->get_meta_data_int)(GMU_META_IS_UPDATED, 1)) {
							if (update_metadata(gd, ti, charset)) {
								event_queue_push(gmu_core_get_event_queue(), GMU_TRACKINFO_CHANGE);
								wdprintf(V_DEBUG, "fileplayer", "Meta data change detected!\n");
							}
						}
					}
				}
				wdprintf(V_INFO, "fileplayer", "Playback stopped.\n");
				audio_set_pause(1);
			} else {
				wdprintf(V_WARNING, "fileplayer", "Broken audio stream.\n");
			}
			(*gd->close_file)();
		} else {
			wdprintf(V_DEBUG, "fileplayer", "Unable to open file.\n");
		}
		if (item_status == STOPPED) audio_buffer_clear();
		if (item_status != STOPPED) item_status = FINISHED;
		wdprintf(V_DEBUG, "fileplayer", "Decoder thread: Playback done.\n");
	}
	if (gd->set_reader_handle) {
		if (r) reader_close(r);
		(*gd->set_reader_handle)(NULL);
	}
	wdprintf(V_DEBUG, "fileplayer", "Decoder thread finished.\n");
	thread_running = 0;
	return NULL;
}

int file_player_play_file(char *file, TrackInfo *ti)
{
	char       *tmp = get_file_extension(file);
	static char filename[256];
	pthread_t   thread;
	static decode_audio_params dap;
	Reader     *r = NULL;

	strncpy(filename, file, 255);
	filename[255] = '\0';

	item_status = STOPPED;
	playback_status = PLAYING;

	wdprintf(V_INFO, "fileplayer", "Trying to play %s...\n", filename);
	dap.gd = decloader_get_decoder_for_extension(tmp);
	if (!(dap.gd && dap.gd->identifier)) { /* No decoder found by extension, try mime-type check by data chunk */
		wdprintf(V_WARNING, "fileplayer", "No suitable decoder available for extension %s. Trying mime type check.\n", tmp);
		r = reader_open(filename);
		if (r && reader_read_bytes(r, 4096)) {
			dap.gd = decloader_get_decoder_for_data_chunk(reader_get_buffer(r), reader_get_number_of_bytes_in_buffer(r));
		}
	}
	if (dap.gd && dap.gd->identifier) {
		if (!file_player_shut_down) {
			wdprintf(V_INFO, "fileplayer", "Selected decoder: %s\n", dap.gd->identifier);
			dap.file = filename;
			dap.ti = ti;
			if (dap.gd->set_reader_handle) {
				if (!r) {
					r = reader_open(filename);
					if (r) reader_read_bytes(r, 4096);
				}
			} else {
				if (r) {
					reader_close(r);
					r = NULL;
				}
			}
			dap.r = r;
			pthread_create(&thread, NULL, decode_audio_thread, &dap);
			pthread_detach(thread);
			/*wdprintf(V_INFO, "fileplayer", "Prebuffering...\n");
			while (audio_buffer_get_fill() < MIN_BUFFER_FILL * 2 && item_status != STOPPED)
				SDL_Delay(10);
			wdprintf(V_INFO, "fileplayer", "Prebuffering done.\n");
			if (item_status != STOPPED) audio_set_pause(0);*/
		} else {
			wdprintf(V_DEBUG, "fileplayer", "Shutdown in progress.\n");
			if (r) reader_close(r);
		}
	} else {
		wdprintf(V_WARNING, "fileplayer", "No suitable decoder available.\n");
		if (r) {
			reader_close(r);
			r = NULL;
		}
		item_status = FINISHED;
	}
	return playback_status;
}

int file_player_read_tags(char *file, char *file_type, TrackInfo *ti)
{
	int         result = 0;
	GmuDecoder *gd = decloader_get_decoder_for_extension(file_type);
	GmuCharset  charset = M_CHARSET_AUTODETECT;

	if (gd && *gd->meta_data_get_charset)
		charset = (*gd->meta_data_get_charset)();

	trackinfo_clear(ti);
	if (gd && *gd->meta_data_load && (*gd->meta_data_load)(file)) {
		if (*gd->get_meta_data) {
			if ((*gd->get_meta_data)(GMU_META_ARTIST, 0))
				strncpy_charset_conv(ti->artist,  (*gd->get_meta_data)(GMU_META_ARTIST, 0), SIZE_ARTIST-1, 0, charset);
			if ((*gd->get_meta_data)(GMU_META_TITLE, 0))
				strncpy_charset_conv(ti->title,   (*gd->get_meta_data)(GMU_META_TITLE, 0), SIZE_TITLE-1, 0, charset);
			if ((*gd->get_meta_data)(GMU_META_ALBUM, 0))
				strncpy_charset_conv(ti->album,   (*gd->get_meta_data)(GMU_META_ALBUM, 0), SIZE_ALBUM-1, 0, charset);
			if ((*gd->get_meta_data)(GMU_META_TRACKNR, 0))
				strncpy_charset_conv(ti->tracknr, (*gd->get_meta_data)(GMU_META_TRACKNR, 0), SIZE_TRACKNR-1, 0, charset);
			if ((*gd->get_meta_data)(GMU_META_DATE, 0))
				strncpy_charset_conv(ti->date,    (*gd->get_meta_data)(GMU_META_DATE, 0), SIZE_DATE-1, 0, charset);
			trackinfo_set_updated(ti);
			result = 1;
		}
		if (*gd->meta_data_close) (*gd->meta_data_close)();
	}
	return result;
}

int file_player_seek(long offset)
{
	seek_second = audio_get_playtime() / 1000 + offset;
	return 0;
}

int file_player_is_metadata_loaded(void)
{
	int result = meta_data_loaded;
	meta_data_loaded = 0;
	return result;
}
