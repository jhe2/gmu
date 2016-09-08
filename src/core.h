/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2016 Johannes Heimansberg (wej.k.vu)
 *
 * File: core.h  Created: 081115
 *
 * Description: The Gmu core
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _CORE_H
#define _CORE_H
#include "playlist.h"
#include "pbstatus.h"
#include "trackinfo.h"
#include "wejconfig.h"
#include "audio.h"
#include "eventqueue.h"
#include "medialib.h"

#define VERSION_NUMBER "0.10.1"

#define DEFAULT_THREAD_STACK_SIZE (128 * 1024)

#define GMU_CORE_HW_VOLUME_MAX 100
#define GMU_CORE_SW_VOLUME_MAX AUDIO_MAX_SW_VOLUME

typedef enum GmuFeatures {
	GMU_FEATURE_MEDIALIB = 1
} GmuFeatures;

ConfigFile      *gmu_core_get_config(void);
int              gmu_core_config_acquire_lock(void);
int              gmu_core_config_release_lock(void);
GmuFeatures      gmu_core_get_features(void);
int              gmu_core_play(void);
int              gmu_core_play_pause(void);
int              gmu_core_pause(void);
int              gmu_core_next(void);
int              gmu_core_previous(void);
int              gmu_core_stop(void);
int              gmu_core_play_pl_item(int item);
int              gmu_core_play_file(const char *filename);
int              gmu_core_play_medialib_item(size_t id);
int              gmu_core_playback_is_paused(void);
int              gmu_core_get_length_current_track(void);
void             gmu_core_quit(void);
TrackInfo       *gmu_core_get_current_trackinfo_ref(void);
void             gmu_core_set_volume(int volume);
int              gmu_core_get_volume(void);
int              gmu_core_get_volume_max(void);
int              gmu_core_export_playlist(const char *file);
const char      *gmu_core_get_device_model_name(void);
char           **gmu_core_get_file_extensions(void);
int              gmu_core_get_status(void);
void             gmu_core_set_shutdown_time(int value);
int              gmu_core_get_shutdown_time_total(void);
int              gmu_core_get_shutdown_time_remaining(void);
EventQueue      *gmu_core_get_event_queue(void);
/* Playlist wrapper functions:
 * Most playlist wrapper functions acquire a lock for playlist access,
 * except functions working on playlist Entry objects. A lock must be 
 * acquired manually before calling those functions and released afterwards.
 */
void             gmu_core_playlist_acquire_lock(void);
void             gmu_core_playlist_release_lock(void);
int              gmu_core_playlist_set_current(Entry *entry);
int              gmu_core_playlist_add_item(const char *file, const char *name);
void             gmu_core_playlist_reset_random(void);
PlayMode         gmu_core_playlist_get_play_mode(void);
int              gmu_core_playlist_add_dir(const char *dir);
Entry           *gmu_core_playlist_get_first(void);
size_t           gmu_core_playlist_get_length(void);
int              gmu_core_playlist_insert_file_after(Entry *entry, const char *filename_with_path);
int              gmu_core_playlist_add_file(const char *filename_with_path);
char            *gmu_core_playlist_get_entry_filename(Entry *entry);
PlayMode         gmu_core_playlist_cycle_play_mode(void);
void             gmu_core_playlist_set_play_mode(PlayMode pm);
int              gmu_core_playlist_entry_enqueue(Entry *entry);
int              gmu_core_playlist_get_current_position(void);
void             gmu_core_playlist_clear(void);
Entry           *gmu_core_playlist_get_entry(int item);
int              gmu_core_playlist_entry_delete(Entry *entry);
Entry           *gmu_core_playlist_item_delete(int item);
Entry           *gmu_core_playlist_get_current(void);
char            *gmu_core_playlist_get_entry_name(Entry *entry);
Entry           *gmu_core_playlist_get_last(void);
void             gmu_core_add_m3u_contents_to_playlist(const char *filename);
void             gmu_core_add_pls_contents_to_playlist(const char *filename);
char            *gmu_core_get_base_dir(void);
char            *gmu_core_get_config_dir(void);
int              gmu_core_playlist_is_recursive_directory_add_in_progress(void);
Entry           *gmu_core_playlist_get_next(Entry *entry);
Entry           *gmu_core_playlist_get_prev(Entry *entry);
int              gmu_core_playlist_get_played(Entry *entry);
int              gmu_core_playlist_entry_get_queue_pos(Entry *entry);
/* Media library wrapper functions: */
void             gmu_core_medialib_start_refresh(void);
int              gmu_core_medialib_search_find(GmuMedialibDataType type, const char *str);
TrackInfo        gmu_core_medialib_search_fetch_next_result(void);
void             gmu_core_medialib_search_finish(void);
int              gmu_core_medialib_add_id_to_playlist(size_t id);
int              gmu_core_medialib_browse_artists(void);
int              gmu_core_medialib_browse_albums_by_artist(const char *artist);
const char      *gmu_core_medialib_browse_fetch_next_result(void);
void             gmu_core_medialib_browse_finish(void);
void             gmu_core_medialib_path_add(const char *path);
#endif
