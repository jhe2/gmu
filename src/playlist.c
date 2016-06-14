/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2016 Johannes Heimansberg (wej.k.vu)
 *
 * File: playlist.c  Created: 060930
 *
 * Description: Playlist
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
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "playlist.h"
#include "dir.h"
#include "trackinfo.h"
#include "util.h"
#include "charset.h"
#include "debug.h"
#include "core.h"
#include "metadatareader.h"
#include "dirparser.h"
#include "pthread_helper.h"

#define PLAYLIST_MAX_LENGTH 9999

static int recursive_directory_add_in_progress = 0;

void playlist_init(Playlist *pl)
{
	pl->length       = 0;
	pl->current      = NULL;
	pl->first        = NULL;
	pl->last         = NULL;
	pl->play_mode    = PM_CONTINUE;
	pl->played_items = 0;
	pl->queue_start  = NULL;
	srand(time(NULL));
	pthread_mutex_init(&(pl->mutex), NULL);
}

void playlist_free(Playlist *pl)
{	
	pthread_mutex_lock(&(pl->mutex));
	playlist_clear(pl);
	pthread_mutex_unlock(&(pl->mutex));
	pthread_mutex_destroy(&(pl->mutex));
}

void playlist_get_lock(Playlist *pl)
{
	pthread_mutex_lock(&(pl->mutex));
}

void playlist_release_lock(Playlist *pl)
{
	pthread_mutex_unlock(&(pl->mutex));
}

void playlist_clear(Playlist *pl)
{
	Entry *entry, *next;
	
	entry = pl->first;
	while (entry != NULL) {
		next = entry->next;
		free(entry);
		entry = next;
	}
	pl->length  = 0;
	pl->current = NULL;
	pl->first   = NULL;
	pl->last    = NULL;
	pl->played_items = 0;
	pl->queue_start = NULL;
}

int playlist_add_item(Playlist *pl, const char *file, const char *name)
{
	int    result = 1;
	Entry *entry = NULL;

	if (pl->length < PLAYLIST_MAX_LENGTH) {
		if (pl->first == NULL) { /* playlist empty */
			pl->first = malloc(sizeof(Entry));
			if (pl->first) {
				entry = pl->first;
				entry->prev = NULL;
				pl->last = pl->first;
			}
		} else {
			pl->last->next = malloc(sizeof(Entry));
			if (pl->last->next) {
				entry = pl->last->next;
				entry->prev = pl->last;
				pl->last = entry;
			}
		}
		if (entry) {
			entry->played = 0;
			entry->next = NULL;
			if (file[0] != '/' && strncmp(file, "http://", 7) != 0) {
				char path[256];
				if (getcwd(path, 253)) { /* do we still need this? */
					snprintf(entry->filename, 255, "%s/%s", path, file);
				} else {
					entry->filename[0] = '\0';
					result = 0;
				}
			} else {
				strncpy(entry->filename, file, 255);
			}
			if (result) {
				entry->filename[255] = '\0';
				playlist_entry_set_name(entry, name);
				entry->queue_pos = 0;
				entry->next_in_queue = NULL;
				pl->length++;
			}
		} else {
			result = 0;
		}
	} else {
		result = 0;
	}
	return result;
}

/**
 * If 'entry' is NULL, the file is added at the end of the playlist.
 * If 'entry' is a valid playlist entry, the file is inserted after 
 * 'entry' in the playlist.
 * Returns 1 on success, 0 otherwise.
 */
int playlist_add_file(Playlist *pl, const char *filename_with_path, Entry *entry)
{
	char        filetype[16];
	const char *tmp = get_file_extension(filename_with_path);
	TrackInfo   ti;
	int         result = 0;

	trackinfo_init(&ti, 0);
	filetype[0] = '\0';
	if (tmp != NULL) strtoupper(filetype, tmp, 15);
	if (strncmp(filetype, "M3U", 3) != 0 && strncmp(filetype, "PLS", 3) != 0) {
		if (metadatareader_read(filename_with_path, filetype, &ti)) {
			char temp[256];
			trackinfo_get_full_title(&ti, temp, 255);
			if (!charset_is_valid_utf8_string(temp)) {
				wdprintf(V_WARNING, "playlist", "WARNING: Failed to create a valid UTF-8 title string. :(\n");
			} else {
				if (entry)
					result = playlist_insert_item_after(pl, entry, filename_with_path, temp);
				else
					result = playlist_add_item(pl, filename_with_path, temp);
			}
		} else {
			const char *filename = strrchr(filename_with_path, '/');
			if (filename) {
				char buf[256];

				filename = filename + 1;
				if (charset_is_valid_utf8_string(filename)) {
					strncpy(buf, filename, 255);
					buf[255] = '\0';
				} else {
					if (!charset_iso8859_1_to_utf8(buf, filename, 255)) {
						wdprintf(V_WARNING, "playlist", "ERROR: Failed to convert filename text to UTF-8.\n");
						snprintf(buf, 255, "[Filename with unsupported encoding]");
					}
				}
				if (entry)
					result = playlist_insert_item_after(pl, entry, filename_with_path, buf);
				else
					result = playlist_add_item(pl, filename_with_path, buf);
			}
		}
	}
	trackinfo_clear(&ti);
	return result;
}

typedef struct _thread_params {
	Playlist *pl;
	char     *directory;
	void     (*finished_callback)(size_t pl_len);
} _thread_params;

static int internal_add_file(void *pl, const char *file)
{
	return playlist_add_file((Playlist *)pl, file, NULL);
}

static void *thread_add_dir(void *udata)
{
	struct _thread_params *tp = (struct _thread_params *)udata;
	size_t                 prev_len = playlist_get_length(tp->pl);

	wdprintf(V_INFO, "playlist", "Recursive directory add thread created.\n");
	dirparser_walk_through_directory_tree(tp->directory, internal_add_file, (void *)tp->pl, 0);
	free(tp->directory);
	wdprintf(V_INFO, "playlist", "Recursive directory add thread finished.\n");
	recursive_directory_add_in_progress = 0;
	if (tp->finished_callback) (tp->finished_callback)(prev_len);
	return NULL;
}

int playlist_add_dir(
	Playlist   *pl,
	const char *directory,
	void       (*finished_callback)(size_t pl_len)
)
{
	static pthread_t       thread;
	static _thread_params  tp;
	int                    res = 0;

	if (!recursive_directory_add_in_progress) {
		size_t len = strlen(directory);
		recursive_directory_add_in_progress = 1;
		tp.directory = malloc(len+1);
		if (tp.directory) {
			memcpy(tp.directory, directory, len+1);
			tp.pl = pl;
			tp.finished_callback = finished_callback;
			pthread_create_with_stack_size(&thread, DEFAULT_THREAD_STACK_SIZE, thread_add_dir, &tp);
			pthread_detach(thread);
			res = 1;
		}
	}
	return res;
}

int playlist_is_recursive_directory_add_in_progress(void)
{
	return recursive_directory_add_in_progress;
}

int playlist_insert_item_after(Playlist *pl, Entry *entry, const char *file, const char *name)
{
	Entry *new_entry;
	int    result = 0;

	if (entry != NULL) {
		new_entry = malloc(sizeof(Entry));
		if (new_entry) {
			strncpy(new_entry->filename, file, 255);
			new_entry->filename[255] = '\0';
			playlist_entry_set_name(new_entry, name);
			new_entry->played = 0;
			new_entry->queue_pos = 0;
			new_entry->next_in_queue = NULL;
			new_entry->prev = entry;
			if (entry->next != NULL)
				new_entry->next = entry->next;
			else
				new_entry->next = NULL;
			entry->next = new_entry;
			if (new_entry->next != NULL)
				new_entry->next->prev = new_entry;
			pl->length++;
			result = 1;
		}
	}
	return result;
}

Entry *playlist_get_first(Playlist *pl)
{
	return pl->first;
}

Entry *playlist_get_last(Playlist *pl)
{
	return pl->last;
}

Entry *playlist_get_next(Entry *entry)
{
	return entry->next;
}

Entry *playlist_get_prev(Entry *entry)
{
	return entry->prev;
}

PlayMode playlist_get_play_mode(Playlist *pl)
{
	return pl->play_mode;
}

int playlist_set_play_mode(Playlist *pl, PlayMode mode)
{
	int res = 0;
	if (mode >= PM_CONTINUE && mode <= PM_RANDOM_REPEAT) {
		pl->play_mode = mode;
		res = 1;
	}
	return res;
}

PlayMode playlist_cycle_play_mode(Playlist *pl)
{
	if (pl->play_mode + 1 > PM_RANDOM_REPEAT)
		pl->play_mode = PM_CONTINUE;
	else
		pl->play_mode += 1;
	return pl->play_mode;
}

void playlist_reset_random(Playlist *pl)
{
	Entry *entry;

	entry = pl->first;
	while (entry) {
		entry->played = 0;
		entry = entry->next;
	}
	pl->played_items = 0;
}

int playlist_entry_delete(Playlist *pl, Entry *entry)
{
	int result = 1;
	if (entry != NULL && pl->length > 0) {
		if (pl->current == entry) { /* We try to remove the currently playing entry */
			pl->current = entry->prev;
		}
		if (entry->prev == NULL && entry->next == NULL) { /* remove last remaining entry */
			pl->first = NULL;
			pl->last = NULL;
		} else if (entry->prev == NULL) { /* remove first entry */
			pl->first = entry->next;
			entry->next->prev = NULL;
		} else if (entry->next == NULL) { /* remove last entry */
			pl->last = entry->prev;
			entry->prev->next = NULL;
		} else { /* remove entry in the middle */
			entry->prev->next = entry->next;
			entry->next->prev = entry->prev;
		}
		pl->length--;
		if (pl->length == 0) {
			pl->first = NULL;
			pl->last = NULL;
		}
		free(entry);
		entry = NULL;
	} else {
		result = 0;
	}
	return result;
}

Entry *playlist_item_delete(Playlist *pl, size_t item)
{
	size_t i;
	Entry *entry = pl->first, *next = NULL;

	for (i = 0; entry != NULL && i != item; i++) {
		entry = playlist_get_next(entry);
	}
	if (entry) {
		next = entry->next;
		playlist_entry_delete(pl, entry);
	}
	return next;
}

char *playlist_get_entry_name(Playlist *pl, Entry *entry)
{
	char *result = NULL;
	if (entry != NULL)
		result = entry->name;
	return result;
}

char *playlist_get_entry_filename(Playlist *pl, Entry *entry)
{
	char *result = NULL;
	if (entry != NULL)
		result = entry->filename;
	return result;
}

char *playlist_get_name(Playlist *pl, size_t item)
{
	char  *result = NULL;
	Entry *entry = pl->first;

	if (item < pl->length) {
		size_t i;
		for (i = 0; i < item && entry->next != NULL; i++)
			entry = entry->next;
		result = entry->name;
	}
	return result;
}

char *playlist_get_filename(Playlist *pl, size_t item)
{
	char  *result = NULL;
	Entry *entry = pl->first;

	if (item < pl->length) {
		size_t i;
		for (i = 0; i < item && entry->next != NULL; i++)
			entry = entry->next;
		result = entry->filename;
	}
	return result;
}

size_t playlist_get_length(Playlist *pl)
{
	return pl->length;
}

int playlist_next(Playlist *pl)
{
	int    result = 0;
	size_t i, next_item;
	Entry *entry;

	if (pl->queue_start != NULL) { /* Queue not empty? */
		Entry *iter = pl->queue_start;
		pl->current = pl->queue_start;
		pl->queue_start = pl->current->next_in_queue;
		for (i = 0; iter != NULL; iter = iter->next_in_queue, i++)
			iter->queue_pos = i;
		result = 1;
	} else {
		switch (pl->play_mode) {
			case PM_CONTINUE:
				if (pl->current != NULL && pl->current->next != NULL) {
					pl->current = pl->current->next;
					result = 1;
				} else if (pl->current != NULL && pl->current->next == NULL) {
					result = 0; /* we have reached the end of the playlist */
				} else if (pl->current == NULL) {
					pl->current = pl->first;
					if (pl->first) result = 1;
				}
				break;
			case PM_REPEAT_ALL:
				if (pl->current != NULL) {
					if (pl->current->next != NULL)
						pl->current = pl->current->next;
					else
						pl->current = pl->first;
					result = 1;
				} else if (pl->current == NULL) {
					pl->current = pl->first;
					if (pl->current != NULL) result = 1;
				}
				break;
			case PM_REPEAT_1:
				if (!pl->current) pl->current = pl->first;
				if (pl->current)
					result = 1;
				break;
			case PM_RANDOM:
			case PM_RANDOM_REPEAT:
				entry = pl->first;
				if (pl->length > 0) {
					next_item = rand() / (RAND_MAX / pl->length);

					for (i = 0; i < next_item; i++)
						entry = entry->next;

					if (entry->played && pl->played_items < pl->length) {
						result = playlist_next(pl);
					} else if (pl->played_items < pl->length) {
						playlist_set_current(pl, entry);
						result = 1;
					}
					if (pl->play_mode == PM_RANDOM_REPEAT) {
						if (pl->played_items >= pl->length) { /* all tracks played? */
							playlist_reset_random(pl);
						}
					}
				}
				break;
		}
	}
	return result;
}

int playlist_prev(Playlist *pl)
{
	int result = 0;

	switch (pl->play_mode) {
		case PM_CONTINUE:
			if (pl->current != NULL && pl->current->prev != NULL &&
			    pl->play_mode != PM_RANDOM && pl->play_mode != PM_RANDOM_REPEAT) {
				pl->current = pl->current->prev;
				result = 1;
			}
			break;
		case PM_REPEAT_1:
			if (pl->current) result = 1;
			break;
		case PM_REPEAT_ALL:
			if (pl->current != NULL) {
				if (pl->current->prev != NULL)
					pl->current = pl->current->prev;
				else
					pl->current = pl->last;
				if (pl->current) result = 1;
			}
			break;
		case PM_RANDOM:
		case PM_RANDOM_REPEAT:
			break;
	}
	return result;
}

int playlist_set_current(Playlist *pl, Entry *entry)
{
	pl->current = entry;
	if (pl->current != NULL) {
		if (pl->play_mode == PM_RANDOM || pl->play_mode == PM_RANDOM_REPEAT)
			pl->current->played = 1;
		pl->played_items++;
	}
	return 1;
}

Entry *playlist_get_current(Playlist *pl)
{
	return pl->current;
}

int playlist_get_current_position(Playlist *pl)
{
	int    res = -1;
	Entry *entry = pl->first;

	if (entry != NULL) {
		for (res = 0; entry != playlist_get_current(pl) && res < playlist_get_length(pl); res++)
			entry = playlist_get_next(entry);
	}
	return res;
}

int playlist_get_played(Entry *entry)
{
	return entry->played;
}

size_t playlist_entry_get_queue_pos(Entry *entry)
{
	return entry->queue_pos;
}

int playlist_entry_enqueue(Playlist *pl, Entry *entry)
{
	if (entry) {
		if (entry->queue_pos == 0) { /* Item not yet in queue -> enqueue item */
			if (pl->queue_start != NULL) { /* Queue is not empty */
				Entry *iter = pl->queue_start;
				int    prev_queue_pos = 1;
				for (; iter->next_in_queue != NULL; iter = iter->next_in_queue)
					prev_queue_pos++;
				iter->next_in_queue = entry;
				iter->next_in_queue->queue_pos = prev_queue_pos+1;
			} else { /* Queue is empty */
				pl->queue_start = entry;
				pl->queue_start->queue_pos = 1;
			}
			entry->next_in_queue = NULL;
		} else { /* Item already in queue -> dequeue item */
			Entry *iter, *prev = NULL;
			int    removed = 0, cont = 1;

			prev = iter = pl->queue_start;
			do {
				if (iter == entry) {
					Entry *tmp = NULL;
					if (iter == pl->queue_start) pl->queue_start = iter->next_in_queue;
					if (prev) prev->next_in_queue = iter->next_in_queue;
					iter->queue_pos = 0;
					tmp = iter->next_in_queue;
					iter->next_in_queue = NULL;
					iter = tmp;
					removed = 1;
				}
				if (removed && iter != NULL) iter->queue_pos--;
				prev = iter;
				if (iter && iter->next_in_queue)
					iter = iter->next_in_queue;
				else
					cont = 0;
			} while (cont);
		}
	}
	return 0;
}

Entry *playlist_get_entry(Playlist *pl, size_t item)
{
	Entry *entry;
	size_t i = 0;

	entry = pl->first;
	if (entry != NULL) {
		while (i < item && entry) {
			entry = playlist_get_next(entry);
			i++;
		}
	}
	return entry;
}

int playlist_entry_set_name(Entry *entry, const char *name)
{
	int res = 0;
	if (entry) {
		strncpy(entry->name, name, PL_ENTRY_NAME_MAX_LENGTH-1);
		entry->name[PL_ENTRY_NAME_MAX_LENGTH-1] = '\0';
		charset_fix_broken_utf8_string(entry->name);
		res = 1;
	}
	return res;
}
