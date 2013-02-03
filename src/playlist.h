/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: playlist.h  Created: 060930
 *
 * Description: Playlist
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _PLAYLIST_H
#define _PLAYLIST_H
#include <pthread.h>
typedef enum PlayMode { 
	PM_CONTINUE, PM_REPEAT_ALL, PM_REPEAT_1, PM_RANDOM, PM_RANDOM_REPEAT
} PlayMode;

typedef struct _Entry Entry;

#define PL_ENTRY_NAME_MAX_LENGTH 64

struct _Entry
{
	Entry *next, *prev;
	char   filename[256];
	char   name[PL_ENTRY_NAME_MAX_LENGTH];
	short  played;
	int    queue_pos;
	Entry *next_in_queue;
};

struct _Playlist
{
	int             length;
	int             played_items;
	PlayMode        play_mode;
	Entry          *current;
	Entry          *first, *last;
	Entry          *queue_start;
	pthread_mutex_t mutex;
};

typedef struct _Playlist Playlist;

void     playlist_init(Playlist *pl);
void     playlist_free(Playlist *pl);
void     playlist_get_lock(Playlist *pl);
void     playlist_release_lock(Playlist *pl);
void     playlist_clear(Playlist *pl);
int      playlist_add_item(Playlist *pl, char *file, char *name);
int      playlist_add_file(Playlist *pl, char *filename_with_path);
int      playlist_insert_item_after(Playlist *pl, Entry *entry, char *file, char *name);
int      playlist_insert_file_after(Playlist *pl, Entry *entry, char *filename_with_path);
char    *playlist_get_name(Playlist *pl, int item);
int      playlist_entry_set_name(Entry *entry, char *name);
char    *playlist_get_filename(Playlist *pl, int item);
int      playlist_get_length(Playlist *pl);
int      playlist_next(Playlist *pl);
int      playlist_prev(Playlist *pl);
int      playlist_set_current(Playlist *pl, Entry *entry);
Entry   *playlist_get_current(Playlist *pl);
Entry   *playlist_get_entry(Playlist *pl, int item);
Entry   *playlist_get_first(Playlist *pl);
Entry   *playlist_get_last(Playlist *pl);
Entry   *playlist_get_next(Entry *entry);
Entry   *playlist_get_prev(Entry *entry);
int      playlist_entry_delete(Playlist *pl, Entry *entry);
/* Deletes playlist item at position 'item' and returns a reference to the next pl entry */
Entry   *playlist_item_delete(Playlist *pl, int item);
char    *playlist_get_entry_name(Playlist *pl, Entry *entry);
char    *playlist_get_entry_filename(Playlist *pl, Entry *entry);
PlayMode playlist_get_play_mode(Playlist *pl);
int      playlist_set_play_mode(Playlist *pl, PlayMode mode);
PlayMode playlist_cycle_play_mode(Playlist *pl);
int      playlist_toggle_random_mode(Playlist *pl);
void     playlist_reset_random(Playlist *pl);
int      playlist_get_played(Entry *entry);
int      playlist_add_dir(Playlist *pl, char *directory, void (*finished_callback)(int pl_len));
int      playlist_get_current_position(Playlist *pl);
int      playlist_entry_get_queue_pos(Entry *entry);
int      playlist_entry_enqueue(Playlist *pl, Entry *entry);
int      playlist_is_recursive_directory_add_in_progress(void);
#endif
