/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
 *
 * File: trackinfo.c  Created: 060929
 *
 * Description: Track info struct and functions to store track meta data
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trackinfo.h"
#include "charset.h"
#include "debug.h"
#include <pthread.h>

void trackinfo_set(
	TrackInfo  *ti,
	const char *artist,
	const char *title,
	const char *album,
	const char *tracknr,
	long        bitrate,
	int         samplerate,
	int         channels
)
{
	strncpy(ti->artist, artist, SIZE_ARTIST);
	ti->artist[SIZE_ARTIST-1] = '\0';
	strncpy(ti->title, title, SIZE_TITLE);
	ti->title[SIZE_TITLE-1] = '\0';
	strncpy(ti->album, album, SIZE_ALBUM);
	ti->album[SIZE_ALBUM-1] = '\0';
	strncpy(ti->tracknr, tracknr, SIZE_TRACKNR);
	ti->tracknr[SIZE_TRACKNR-1] = '\0';
	ti->bitrate = bitrate;
	ti->samplerate = samplerate;
	ti->channels = channels;
	ti->lyrics[0] = '\0';
	ti->updated = 1;
}

void trackinfo_set_artist(TrackInfo *ti, const char *artist)
{
	strncpy(ti->artist, artist, SIZE_ARTIST);
	ti->artist[SIZE_ARTIST-1] = '\0';
}

void trackinfo_set_title(TrackInfo *ti, const char *title)
{
	strncpy(ti->title, title, SIZE_TITLE);
	ti->title[SIZE_TITLE-1] = '\0';
}

void trackinfo_set_album(TrackInfo *ti, const char *album)
{
	strncpy(ti->album, album, SIZE_ALBUM-1);
	ti->album[SIZE_ALBUM-1] = '\0';
}

void trackinfo_init(TrackInfo *ti, int with_locking)
{
	ti->artist[0] = '\0';
	ti->title[0] = '\0';
	ti->album[0] = '\0';
	ti->comment[0] = '\0';
	ti->date[0] = '\0';
	ti->file_type[0] = '\0';
	ti->file_name[0] = '\0';
	ti->tracknr[0] = '\0';
	ti->lyrics[0] = '\0';
	ti->bitrate = 0;
	ti->recent_bitrate = 0;
	ti->samplerate = 0;
	ti->channels = 0;
	ti->vbr = 0;
	ti->length = 0;
	ti->file_size = 0;
	ti->has_lyrics = 0;
	ti->image.data = NULL;
	ti->image.data_size = 0;
	ti->image.mime_type[0] = '\0';
	ti->updated = 0;
	ti->with_locking = with_locking;
	if (with_locking) {
		pthread_mutex_init(&(ti->mutex), NULL);
	}
}

void trackinfo_destroy(TrackInfo *ti)
{
	trackinfo_clear(ti);
	if (ti->with_locking) {
		pthread_mutex_destroy(&(ti->mutex));
	}
}

void trackinfo_clear(TrackInfo *ti)
{
	ti->artist[0] = '\0';
	ti->title[0] = '\0';
	ti->album[0] = '\0';
	ti->comment[0] = '\0';
	ti->date[0] = '\0';
	ti->file_type[0] = '\0';
	ti->file_name[0] = '\0';
	ti->tracknr[0] = '\0';
	ti->lyrics[0] = '\0';
	ti->bitrate = 0;
	ti->recent_bitrate = 0;
	ti->samplerate = 0;
	ti->channels = 0;
	ti->vbr = 0;
	ti->length = 0;
	ti->file_size = 0;
	ti->has_lyrics = 0;
	if (ti->image.data != NULL) free(ti->image.data);
	ti->image.data = NULL;
	ti->image.data_size = 0;
	ti->image.mime_type[0] = '\0';
	ti->updated = 0;
}

void trackinfo_set_image(
	TrackInfo  *ti,
	const char *image_data,
	size_t      image_data_size,
	const char *mime_type
)
{
	if ((ti->image.data = malloc(image_data_size)) != NULL) {
		strncpy(ti->image.mime_type, mime_type, SIZE_MIME_TYPE);
		ti->image.mime_type[SIZE_MIME_TYPE-1] = '\0';
		memcpy(ti->image.data, image_data, image_data_size);
		ti->image.data_size = image_data_size;
		ti->updated = 1;
	}
}

char *trackinfo_get_image_data(TrackInfo *ti)
{
	return ti->image.data;
}

int trackinfo_get_image_data_size(TrackInfo *ti)
{
	return ti->image.data_size;
}

char *trackinfo_get_image_mime_type(TrackInfo *ti)
{
	return ti->image.mime_type;
}

char *trackinfo_get_artist(TrackInfo *ti)
{
	return ti->artist;
}

char *trackinfo_get_title(TrackInfo *ti)
{
	return ti->title;
}

char *trackinfo_get_album(TrackInfo *ti)
{
	return ti->album;
}

char *trackinfo_get_file_type(TrackInfo *ti)
{
	return ti->file_type;
}

char *trackinfo_get_tracknr(TrackInfo *ti)
{
	return ti->tracknr;
}

char *trackinfo_get_lyrics(TrackInfo *ti)
{
	return ti->lyrics;
}

int trackinfo_has_lyrics(TrackInfo *ti)
{
	return ti->has_lyrics;
}

long trackinfo_get_bitrate(TrackInfo *ti)
{
	return ti->bitrate;
}

int trackinfo_get_samplerate(TrackInfo *ti)
{
	return ti->samplerate;
}

int trackinfo_get_channels(TrackInfo *ti)
{
	return ti->channels;
}

void trackinfo_get_full_title(TrackInfo *ti, char *target, size_t length)
{
	snprintf(
		target,
		length,
		"%s%s%s",
		ti->artist,
		(strlen(ti->artist) > 0 && strlen(ti->title) > 0 ? " - " : ""),
		ti->title
	);
}

int trackinfo_is_vbr(TrackInfo *ti)
{
	return ti->vbr;
}

int trackinfo_has_cover_artwork(TrackInfo *ti)
{
	return ti->has_cover_artwork;
}

int trackinfo_get_length_minutes(TrackInfo *ti)
{
	return ti->length / 60;
}

int trackinfo_get_length_seconds(TrackInfo *ti)
{
	return ti->length % 60;
}

char *trackinfo_get_file_name(TrackInfo *ti)
{
	return ti->file_name;
}

char *trackinfo_get_date(TrackInfo *ti)
{
	return ti->date;
}

int trackinfo_load_lyrics_from_file(TrackInfo *ti, const char *file_name)
{
	int   result = 1;
	FILE *file;
	char  buffer[SIZE_LYRICS] = "";

	file = fopen(file_name, "r");
	if (file) {
		size_t read_counter = 0;
		char   prev = '\0';

		buffer[0] = '\0';
		while (!feof(file) && read_counter < SIZE_LYRICS && SIZE_LYRICS-1-read_counter > 1) {
			char curr = fgetc(file);
			if (curr != '\r') {
				if (!feof(file))
					buffer[read_counter] = curr;
			} else {
				if (prev != '\n')
					buffer[read_counter] = '\n';
			}
			prev = curr;
			read_counter++;
			/*fgets(buffer+read_counter, SIZE_LYRICS-1-read_counter, file);
			read_counter = strlen(buffer);*/
		}
		fclose(file);
		if (charset_is_valid_utf8_string(buffer)) {
			strncpy(ti->lyrics, buffer, SIZE_LYRICS);
			ti->lyrics[SIZE_LYRICS-1] = '\0';
			wdprintf(V_DEBUG, "trackinfo", "Lyrics text looks like it is UTF-8 encoded.\n");
		} else { /* Try to convert ISO-8859-1 to UTF-8. */
			wdprintf(V_DEBUG, "trackinfo", "Lyrics text looks like it is ISO-8859-1 encoded.\n");
			if (!charset_iso8859_1_to_utf8(ti->lyrics, buffer, SIZE_LYRICS-1)) {
				wdprintf(V_WARNING, "trackinfo", "ERROR: Failed to convert lyrics text to UTF-8.\n");
				snprintf(ti->lyrics, SIZE_LYRICS-1, "[Text file file with unknown encoding found.]");
			}
		}
		ti->has_lyrics = 1;
	} else {
		result = 0;
	}
	return result;
}

int trackinfo_is_updated(TrackInfo *ti)
{
	int res = ti->updated;
	ti->updated = 0;
	return res;
}

void trackinfo_set_updated(TrackInfo *ti)
{
	ti->updated = 1;
}

void trackinfo_set_trackid(TrackInfo *ti, int id)
{
	ti->id = id;
}

void trackinfo_set_filename(TrackInfo *ti, const char *file)
{
	strncpy(ti->file_name, file, SIZE_FILE_NAME-1);
	ti->file_name[SIZE_FILE_NAME-1] = '\0';
}

void trackinfo_set_file_type(TrackInfo *ti, const char *file_type)
{
	strncpy(ti->file_type, file_type, SIZE_FILE_TYPE-1);
	ti->file_type[SIZE_FILE_TYPE-1] = '\0';
}

int trackinfo_acquire_lock(TrackInfo *ti)
{
	int res = 0;
	if (ti->with_locking && pthread_mutex_lock(&(ti->mutex)) == 0)
		res = 1;
	return res;
}

int trackinfo_release_lock(TrackInfo *ti)
{
	int res = 0;
	if (ti->with_locking && pthread_mutex_unlock(&(ti->mutex)) == 0)
		res = 1;
	return res;
}

/* Copies the content of a TrackInfo object */
int trackinfo_copy(TrackInfo *dest, TrackInfo *src)
{
	int res = 0;
	strncpy(dest->artist, src->artist, SIZE_ARTIST);
	strncpy(dest->title, src->title, SIZE_TITLE);
	strncpy(dest->album, src->album, SIZE_ALBUM);
	strncpy(dest->comment, src->comment, SIZE_COMMENT);
	strncpy(dest->date, src->date, SIZE_DATE);
	strncpy(dest->file_type, src->file_type, SIZE_FILE_TYPE);
	strncpy(dest->file_name, src->file_name, SIZE_FILE_NAME);
	strncpy(dest->tracknr, src->tracknr, SIZE_TRACKNR);
	strncpy(dest->lyrics, src->lyrics, SIZE_LYRICS);
	dest->image = src->image;
	dest->bitrate = src->bitrate;
	dest->recent_bitrate = src->recent_bitrate;
	dest->samplerate = src->samplerate;
	dest->channels = src->channels;
	dest->length = src->length;
	dest->vbr = src->vbr;
	dest->has_cover_artwork = src->has_cover_artwork;
	dest->has_lyrics = src->has_lyrics;
	dest->file_size = src->file_size;
	dest->updated = src->updated;
	dest->id = src->id;
	return res;
}
