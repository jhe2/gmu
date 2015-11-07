/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
 *
 * File: trackinfo.h  Created: 060929
 *
 * Description: Track info struct and functions to store track meta data
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _TRACKINFO_H
#define _TRACKINFO_H
#include <pthread.h>

#define SIZE_ARTIST    128
#define SIZE_TITLE     128
#define SIZE_ALBUM     64
#define SIZE_COMMENT   64
#define SIZE_DATE      32
#define SIZE_FILE_TYPE 64
#define SIZE_MIME_TYPE 64
#define SIZE_FILE_NAME 256
#define SIZE_TRACKNR   32
#define SIZE_LYRICS    16384

typedef struct Image
{
	char *data;
	int   data_size;
	char  mime_type[SIZE_MIME_TYPE];
} Image;

struct _TrackInfo
{
	char   artist[SIZE_ARTIST];
	char   title[SIZE_TITLE];
	char   album[SIZE_ALBUM];
	char   comment[SIZE_COMMENT];
	char   date[SIZE_DATE];
	char   file_type[SIZE_FILE_TYPE];
	char   file_name[SIZE_FILE_NAME];
	char   tracknr[SIZE_TRACKNR];
	char   lyrics[SIZE_LYRICS];
	Image  image;

	long   bitrate, recent_bitrate;
	int    samplerate;
	int    channels;
	size_t length;
	int    vbr;
	int    has_cover_artwork, has_lyrics;
	size_t file_size;
	int    updated;

	int    id;
	pthread_mutex_t mutex;
	int    with_locking;
};

typedef struct _TrackInfo TrackInfo;

void  trackinfo_init(TrackInfo *ti, int with_locking);
/* When the locking capability is enabled, trackinfo_destroy() must be
 * called when done with the TrackInfo object to clean up properly */
void  trackinfo_destroy(TrackInfo *ti);
void  trackinfo_clear(TrackInfo *ti);
void  trackinfo_set(
	TrackInfo  *ti,
	const char *artist,
	const char *title,
	const char *album,
	const char *tracknr,
	long        bitrate,
	int         samplerate,
	int         channels
);
void  trackinfo_set_artist(TrackInfo *ti, const char *artist);
void  trackinfo_set_title(TrackInfo *ti, const char *title);
void  trackinfo_set_album(TrackInfo *ti, const char *album);
void  trackinfo_set_trackid(TrackInfo *ti, int id);
void  trackinfo_set_filename(TrackInfo *ti, const char *file);
void  trackinfo_set_file_type(TrackInfo *ti, const char *file_type);
char *trackinfo_get_artist(TrackInfo *ti);
char *trackinfo_get_title(TrackInfo *ti);
char *trackinfo_get_album(TrackInfo *ti);
char *trackinfo_get_file_type(TrackInfo *ti);
char *trackinfo_get_file_name(TrackInfo *ti);
char *trackinfo_get_date(TrackInfo *ti);
char *trackinfo_get_tracknr(TrackInfo *ti);
char *trackinfo_get_lyrics(TrackInfo *ti);
long  trackinfo_get_bitrate(TrackInfo *ti);
int   trackinfo_get_samplerate(TrackInfo *ti);
int   trackinfo_get_channels(TrackInfo *ti);
void  trackinfo_get_full_title(TrackInfo *ti, char *target, size_t length);
int   trackinfo_has_lyrics(TrackInfo *ti);
void  trackinfo_set_image(
	TrackInfo  *ti,
	const char *image_data,
	size_t      image_data_size,
	const char *mime_type
);
int   trackinfo_is_vbr(TrackInfo *ti);
int   trackinfo_has_cover_artwork(TrackInfo *ti);
int   trackinfo_get_length_minutes(TrackInfo *ti);
int   trackinfo_get_length_seconds(TrackInfo *ti);
int   trackinfo_load_lyrics_from_file(TrackInfo *ti, const char *file_name);
int   trackinfo_is_updated(TrackInfo *ti);
void  trackinfo_set_updated(TrackInfo *ti);
char *trackinfo_get_image_data(TrackInfo *ti);
int   trackinfo_get_image_data_size(TrackInfo *ti);
char *trackinfo_get_image_mime_type(TrackInfo *ti);
/* The locking/unlocking functions return 1 on success and 0 otherwise.
 * The functions can fail either because of an error inside of the
 * pthread library or because the TrackInfo object has not been
 * initialized with locking enabled. */
int   trackinfo_acquire_lock(TrackInfo *ti);
int   trackinfo_release_lock(TrackInfo *ti);
/* Copies the content of a TrackInfo object */
int   trackinfo_copy(TrackInfo *dest, TrackInfo *src);
#endif
