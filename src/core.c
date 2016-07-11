/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: core.c  Created: 081115
 *
 * Description: The Gmu core
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
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include "SDL.h" /* For audio output */
#include "playlist.h"
#include "pbstatus.h"
#include "fileplayer.h"
#include "decloader.h"
#include "feloader.h"
#include "audio.h"
#include "m3u.h"
#include "pls.h"
#include "trackinfo.h"
#include "core.h"
#include "eventqueue.h"
#include "wejconfig.h"
#include FILE_HW_H
#include "util.h"
#include "reader.h" /* for reader_set_cache_size_kb() */
#include "medialib.h"
#include "debug.h"
#include "gmuerror.h"
#define MAX_FILE_EXTENSIONS 255

typedef enum GlobalCommand { NO_CMD, PLAY, PAUSE, STOP, NEXT, 
                             PREVIOUS, PLAY_ITEM, PLAY_FILE } GlobalCommand;

static GlobalCommand   global_command = NO_CMD;
static int             global_param = 0;
static char            global_filename[256];
static int             gmu_running = 0;
static pthread_mutex_t gmu_running_mutex;
static Playlist        pl;
static TrackInfo       current_track_ti;
static EventQueue      event_queue;
static ConfigFile     *config;
static pthread_mutex_t config_mutex;
static char           *file_extensions[MAX_FILE_EXTENSIONS+1];
static int             volume_max;
static unsigned int    player_status = STOPPED;
static int             shutdown_timer = 0;
static int             remaining_time;
static char            base_dir[256], *config_dir;
static volatile sig_atomic_t signal_received = 0;

#ifdef GMU_MEDIALIB
static GmuMedialib     gm;
#endif

static void init_sdl(void)
{
	setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1", 0);
#ifndef CORE_WITH_SDL_VIDEO
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
#else
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
#endif
		wdprintf(V_ERROR, "gmu", "ERROR: Could not initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	wdprintf(V_DEBUG, "gmu", "SDL init done.\n");
}

GmuFeatures gmu_core_get_features(void)
{
	GmuFeatures features = 0;
#ifdef GMU_MEDIALIB
	features |= GMU_FEATURE_MEDIALIB; 
#endif
	return features;
}

static int add_m3u_contents_to_playlist(Playlist *pl, const char *filename)
{
	M3u m3u;
	int res = 0;
	if (m3u_open_file(&m3u, filename)) {
		size_t len = playlist_get_length(pl);
		while (m3u_read_next_item(&m3u)) {
			playlist_add_item(pl,
			                  m3u_current_item_get_full_path(&m3u),
			                  m3u_current_item_get_title(&m3u));
		}
		m3u_close_file(&m3u);
		event_queue_push_with_parameter(&event_queue, GMU_PLAYLIST_CHANGE, len);
		res = 1;
	}
	return res;
}

void gmu_core_add_m3u_contents_to_playlist(const char *filename)
{
	playlist_get_lock(&pl);
	add_m3u_contents_to_playlist(&pl, filename);
	playlist_release_lock(&pl);
}

static int add_pls_contents_to_playlist(Playlist *pl, const char *filename)
{
	PLS pls;
	int res = 0;
	if (pls_open_file(&pls, filename)) {
		size_t len = playlist_get_length(pl);
		while (pls_read_next_item(&pls)) {
		   playlist_add_item(pl,
		                     pls_current_item_get_full_path(&pls),
		                     pls_current_item_get_title(&pls));
		}
		pls_close_file(&pls);
		event_queue_push_with_parameter(&event_queue, GMU_PLAYLIST_CHANGE, len);
		res = 1;
	}
	return res;
}

void gmu_core_add_pls_contents_to_playlist(const char *filename)
{
	playlist_get_lock(&pl);
	add_pls_contents_to_playlist(&pl, filename);
	playlist_release_lock(&pl);
}

/**
 * Checks if the 'Gmu.FadeOutOnSkip' option has been enabled and returns
 * 1 if that is the case. The function acquires a lock on the global
 * Gmu config.
 */
static int check_fade_out_on_skip(void)
{
	int res = 0;
	gmu_core_config_acquire_lock();
	if (cfg_get_boolean_value(config, "Gmu.FadeOutOnSkip")) res = 1;
	gmu_core_config_release_lock();
	return res;
}

static int play_next(Playlist *pl, int skip_current)
{
	int result = 0;
	int fade_out_on_skip = check_fade_out_on_skip();
	playlist_get_lock(pl);
	if (playlist_next(pl)) {
		int ppos = playlist_get_current_position(pl);
		if (ppos >= 0) ppos++;
		file_player_play_file(
			playlist_get_entry_filename(pl, playlist_get_current(pl)),
			skip_current,
			fade_out_on_skip
		);
		result = 1;
		event_queue_push_with_parameter(
			&event_queue,
			GMU_TRACK_CHANGE,
			ppos
		);
		player_status = PLAYING;
	}
	playlist_release_lock(pl);
	return result;
}

static int play_previous(Playlist *pl)
{
	int result = 0;
	int fade_out_on_skip = check_fade_out_on_skip();
	playlist_get_lock(pl);
	if (playlist_prev(pl)) {
		Entry *entry = playlist_get_current(pl);
		int    ppos = playlist_get_current_position(pl);
		if (ppos >= 0) ppos++;
		if (entry != NULL) {
			file_player_play_file(
				playlist_get_entry_filename(pl, entry),
				1,
				fade_out_on_skip
			);
			result = 1;
			event_queue_push_with_parameter(
				&event_queue,
				GMU_TRACK_CHANGE,
				ppos
			);
			player_status = PLAYING;
		}
	}
	playlist_release_lock(pl);
	return result;
}

static void add_default_cfg_settings(ConfigFile *config)
{
	cfg_add_key(config, "Gmu.DefaultPlayMode", "continue");
	cfg_add_key(config, "Gmu.RememberLastPlaylist", "yes");
	cfg_key_add_presets(config, "Gmu.RememberLastPlaylist", "yes", "no", NULL);
	cfg_add_key(config, "Gmu.RememberSettings", "yes");
	cfg_key_add_presets(config, "Gmu.RememberSettings", "yes", "no", NULL);
	cfg_add_key(config, "Gmu.FileSystemCharset", "UTF-8");
	cfg_key_add_presets(config, "Gmu.FileSystemCharset", "UTF-8", "ISO-8859-15", NULL);
	cfg_add_key(config, "Gmu.PlaylistSavePresets",
	                    "playlist1.m3u;playlist2.m3u;playlist3.m3u;playlist4.m3u");
	cfg_add_key(config, "Gmu.DefaultFileBrowserPath", ".");
	cfg_add_key(config, "Gmu.VolumeControl", "Software"); /* Software, Hardware, Hardware+Software */
	cfg_key_add_presets(config, "Gmu.VolumeControl", "Software", "Hardware", "Software+Hardware", NULL);
	cfg_add_key(config, "Gmu.VolumeHardwareMixerChannel", "0");
	cfg_add_key(config, "Gmu.Volume", "7");
	cfg_add_key(config, "Gmu.AutoPlayOnProgramStart", "no");
	cfg_key_add_presets(config, "Gmu.AutoPlayOnProgramStart", "yes", "no", NULL);
	cfg_add_key(config, "Gmu.FileBrowserFoldersFirst", "yes");
	cfg_key_add_presets(config, "Gmu.FileBrowserFoldersFirst", "yes", "no", NULL);
	cfg_add_key(config, "Gmu.FirstRun", "yes");
	cfg_add_key(config, "Gmu.ResumePlayback", "yes");
	cfg_key_add_presets(config, "Gmu.ResumePlayback", "yes", "no", NULL);
	cfg_add_key(config, "Gmu.ReaderCache", "512");
	cfg_key_add_presets(config, "Gmu.ReaderCache", "256", "512", "1024", NULL);
	cfg_add_key(config, "Gmu.ReaderCachePrebufferSize", "256");
	cfg_key_add_presets(config, "Gmu.ReaderCachePrebufferSize", "128", "256", "512", "768", NULL);
	cfg_add_key(config, "Gmu.LyricsFilePattern", "*.txt");
	cfg_add_key(config, "Gmu.FadeOutOnSkip", "no");
	cfg_key_add_presets(config, "Gmu.FadeOutOnSkip", "yes", "no", NULL);
	cfg_add_key(config, "Gmu.DeviceCloseASAP", "no");
	cfg_key_add_presets(config, "Gmu.DeviceCloseASAP", "yes", "no", NULL);
}

int gmu_core_export_playlist(const char *file)
{
	M3u m3u_export;
	int result = 0;

	if (m3u_export_file(&m3u_export, file)) {
		Entry *entry;

		playlist_get_lock(&pl);
		entry = playlist_get_first(&pl);
		while (entry != NULL) {
			m3u_export_write_entry(&m3u_export, 
			                       playlist_get_entry_filename(&pl, entry), 
			                       playlist_get_entry_name(&pl, entry), 0);
			entry = playlist_get_next(entry);
		}
		playlist_release_lock(&pl);
		m3u_export_close_file(&m3u_export);
		result = 1;
	}
	return result;
}

static void set_default_play_mode(ConfigFile *config, Playlist *pl)
{
	PlayMode    pmode = PM_CONTINUE;
	const char *dpm;

	gmu_core_config_acquire_lock();
	dpm = cfg_get_key_value(config, "Gmu.DefaultPlayMode");
	if (strncmp(dpm, "random", 6) == 0)
		pmode = PM_RANDOM;
	else if (strncmp(dpm, "repeat1", 11) == 0)
		pmode = PM_REPEAT_1;
	else if (strncmp(dpm, "repeatall", 9) == 0)
		pmode = PM_REPEAT_ALL;
	else if (strncmp(dpm, "random+repeat", 13) == 0)
		pmode = PM_RANDOM_REPEAT;
	gmu_core_config_release_lock();
	playlist_get_lock(pl);
	playlist_set_play_mode(pl, pmode);
	playlist_release_lock(pl);
}

ConfigFile *gmu_core_get_config(void)
{
	return config;
}

/**
 * Used for locking the config for access. Within a config lock region,
 * NO other locks MUST be acquired or released.
 */
int gmu_core_config_acquire_lock(void)
{
	return pthread_mutex_lock(&config_mutex);
}

/**
 * Used for releasing the config lock.
 */
int gmu_core_config_release_lock(void)
{
	return pthread_mutex_unlock(&config_mutex);
}

/* Start playback */
int gmu_core_play(void)
{
	int res = 0;
	if (player_status == STOPPED)
		res = gmu_core_next();
	else
		file_player_request_playback_state_change(PBRQ_PLAY);
	event_queue_push(&event_queue, GMU_PLAYBACK_STATE_CHANGE);
	return res;
}

/* Toggle between pause and playback states */
int gmu_core_play_pause(void)
{
	if (gmu_core_playback_is_paused())
		gmu_core_play();
	else
		gmu_core_pause();
	return audio_get_pause();
}

/* Pause playback */
int gmu_core_pause(void)
{
	file_player_request_playback_state_change(PBRQ_PAUSE);
	event_queue_push(&event_queue, GMU_PLAYBACK_STATE_CHANGE);
	return 0;
}

int gmu_core_next(void)
{
	int res;
	int pbsc = file_player_request_playback_state_change(PBRQ_PLAY);
	res = play_next(&pl, 1);
	if (pbsc) event_queue_push(&event_queue, GMU_PLAYBACK_STATE_CHANGE);
	return res;
}

int gmu_core_previous(void)
{
	int res;
	int pbsc = file_player_request_playback_state_change(PBRQ_PLAY);
	res = play_previous(&pl);
	if (pbsc) event_queue_push(&event_queue, GMU_PLAYBACK_STATE_CHANGE);
	return res;
}

static int stop_playback(void)
{
	int res = 0;
	if (player_status != STOPPED) {
		gmu_core_playlist_reset_random();
		gmu_core_playlist_set_current(NULL);
		file_player_stop_playback();
		player_status = STOPPED;
		event_queue_push(&event_queue, GMU_PLAYBACK_STATE_CHANGE);
		res = 1;
	}
	return res;
}

int gmu_core_stop(void)
{
	int res = stop_playback();
	file_player_request_playback_state_change(PBRQ_STOP);
	return res;
}

int gmu_core_play_pl_item(int item)
{
	global_command = PLAY_ITEM;
	global_param = item;
	player_status = PLAYING;
	file_player_request_playback_state_change(PBRQ_PLAY);
	event_queue_push_with_parameter(
		&event_queue,
		GMU_TRACK_CHANGE,
		item >= 0 ? item + 1 : 0
	);
	return 0;
}

/**
 * Plays a media file, identified by its full path, without
 * adding it to the playlist.
 */
int gmu_core_play_file(const char *filename)
{
	strncpy(global_filename, filename, 255);
	global_filename[255] = '\0';
	global_command = PLAY_FILE;
	player_status = PLAYING;
	file_player_request_playback_state_change(PBRQ_PLAY);
	event_queue_push_with_parameter(&event_queue, GMU_TRACK_CHANGE, -1);
	return 1;
}

/**
 * Plays an item from the media library, identified by its ID, without
 * adding it to the playlist.
 */
int gmu_core_play_medialib_item(size_t id)
{
	int res = 0;
#ifdef GMU_MEDIALIB
	TrackInfo ti = medialib_get_data_for_id(&gm, id);
	if (ti.id > 0 && ti.file_name[0]) {
		res = gmu_core_play_file(ti.file_name);
	}
	wdprintf(V_DEBUG, "core", "Playing track from media library with ID %d...\n", id);
#endif
	return res;
}

/* Playlist wrapper functions: */
void gmu_core_playlist_acquire_lock(void)
{
	playlist_get_lock(&pl);
}

void gmu_core_playlist_release_lock(void)
{
	playlist_release_lock(&pl);
}

int gmu_core_playlist_set_current(Entry *entry)
{
	int res;
	res = playlist_set_current(&pl, entry);
	/*event_queue_push(&event_queue, GMU_PLAYLIST_CHANGE);*/
	return res;
}

int gmu_core_playlist_add_item(const char *file, const char *name)
{
	int    res;
	size_t len;
	playlist_get_lock(&pl);
	len = playlist_get_length(&pl);
	res = playlist_add_item(&pl, file, name);
	playlist_release_lock(&pl);
	event_queue_push_with_parameter(&event_queue, GMU_PLAYLIST_CHANGE, len);
	return res;
}

void gmu_core_playlist_reset_random(void)
{
	playlist_get_lock(&pl);
	playlist_reset_random(&pl);
	playlist_release_lock(&pl);
}

PlayMode gmu_core_playlist_get_play_mode(void)
{
	PlayMode pm;
	playlist_get_lock(&pl);
	pm = playlist_get_play_mode(&pl);
	playlist_release_lock(&pl);
	return pm;
}

static void add_dir_finish_callback(size_t pos)
{
	wdprintf(V_DEBUG, "gmu", "In callback: Recursive directory add done.\n");
	event_queue_push_with_parameter(&event_queue, GMU_PLAYLIST_CHANGE, pos);
}

int gmu_core_playlist_add_dir(const char *dir)
{
	int res;
	playlist_get_lock(&pl);
	res = playlist_add_dir(&pl, dir, add_dir_finish_callback);
	playlist_release_lock(&pl);
	return res;
}

Entry *gmu_core_playlist_get_first(void)
{
	return playlist_get_first(&pl);
}

size_t gmu_core_playlist_get_length(void)
{
	size_t len;
	playlist_get_lock(&pl);
	len = playlist_get_length(&pl);
	playlist_release_lock(&pl);
	return len;
}

int gmu_core_playlist_insert_file_after(Entry *entry, const char *filename_with_path)
{
	int res = playlist_add_file(&pl, filename_with_path, entry);
	if (res) event_queue_push(&event_queue, GMU_PLAYLIST_CHANGE);
	return res;
}

int gmu_core_playlist_add_file(const char *filename_with_path)
{
	int         res;
	size_t      len;
	char        filetype[16] = "(none)";
	const char *tmp = filename_with_path ? get_file_extension(filename_with_path) : NULL;

	if (tmp != NULL) strtoupper(filetype, tmp, 15);
	filetype[15] = '\0';
	playlist_get_lock(&pl);
	len = playlist_get_length(&pl);
	if (strcmp(filetype, "M3U") == 0) {
		res = add_m3u_contents_to_playlist(&pl, filename_with_path);
	} else if (strcmp(filetype, "PLS") == 0) {
		res = add_pls_contents_to_playlist(&pl, filename_with_path);
	} else {
		res = playlist_add_file(&pl, filename_with_path, NULL);
	}
	playlist_release_lock(&pl);
	event_queue_push_with_parameter(&event_queue, GMU_PLAYLIST_CHANGE, len);
	return res;
}

char *gmu_core_playlist_get_entry_filename(Entry *entry)
{
	return playlist_get_entry_filename(&pl, entry);
}

PlayMode gmu_core_playlist_cycle_play_mode(void)
{
	PlayMode res;
	playlist_get_lock(&pl);
	res = playlist_cycle_play_mode(&pl);
	playlist_release_lock(&pl);
	event_queue_push_with_parameter(&event_queue, GMU_PLAYMODE_CHANGE, res);
	return res;
}

void gmu_core_playlist_set_play_mode(PlayMode pm)
{
	int res;
	playlist_get_lock(&pl);
	res = playlist_set_play_mode(&pl, pm);
	playlist_release_lock(&pl);
	if (res)
		event_queue_push_with_parameter(&event_queue, GMU_PLAYMODE_CHANGE, pm);
}

int gmu_core_playlist_entry_enqueue(Entry *entry)
{
	int res;
	res = playlist_entry_enqueue(&pl, entry);
	event_queue_push(&event_queue, GMU_QUEUE_CHANGE);
	return res;
}

int gmu_core_playlist_get_current_position(void)
{
	int res;
	playlist_get_lock(&pl);
	res = playlist_get_current_position(&pl);
	playlist_release_lock(&pl);
	return res;
}

void gmu_core_playlist_clear(void)
{
	playlist_get_lock(&pl);
	playlist_clear(&pl);
	playlist_release_lock(&pl);
	event_queue_push(&event_queue, GMU_PLAYLIST_CHANGE);
}

Entry *gmu_core_playlist_get_entry(int item)
{
	Entry *e;
	e = playlist_get_entry(&pl, item);
	return e;
}

int gmu_core_playlist_entry_delete(Entry *entry)
{
	int res = playlist_entry_delete(&pl, entry);
	event_queue_push(&event_queue, GMU_PLAYLIST_CHANGE);
	return res;
}

Entry *gmu_core_playlist_item_delete(int item)
{
	Entry *next = NULL;
	next = playlist_item_delete(&pl, item);
	event_queue_push_with_parameter(&event_queue, GMU_PLAYLIST_CHANGE, item);
	return next;
}

Entry *gmu_core_playlist_get_current(void)
{
	return playlist_get_current(&pl);
}

char *gmu_core_playlist_get_entry_name(Entry *entry)
{
	return playlist_get_entry_name(&pl, entry);
}

Entry *gmu_core_playlist_get_last(void)
{
	return playlist_get_last(&pl);
}

int gmu_core_playback_is_paused(void)
{
	return audio_get_pause();
}

int gmu_core_playlist_is_recursive_directory_add_in_progress(void)
{
	return playlist_is_recursive_directory_add_in_progress();
}

Entry *gmu_core_playlist_get_next(Entry *entry)
{
	return playlist_get_next(entry);
}

Entry *gmu_core_playlist_get_prev(Entry *entry)
{
	return playlist_get_prev(entry);
}

int gmu_core_playlist_get_played(Entry *entry)
{
	return playlist_get_played(entry);
}

int gmu_core_playlist_entry_get_queue_pos(Entry *entry)
{
	return playlist_entry_get_queue_pos(entry);
}

int gmu_core_get_length_current_track(void)
{
	int len = 0;
	if (trackinfo_acquire_lock(&current_track_ti)) {
		len = current_track_ti.length;
		trackinfo_release_lock(&current_track_ti);
	}
	return len;
}

EventQueue *gmu_core_get_event_queue(void)
{
	return &event_queue;
}

static void set_gmu_running(int bool)
{
	pthread_mutex_lock(&gmu_running_mutex);
	gmu_running = bool;
	pthread_mutex_unlock(&gmu_running_mutex);
}

void gmu_core_quit(void)
{
	wdprintf(V_INFO, "gmu", "Shutting down...\n");
	event_queue_push(&event_queue, GMU_QUIT);
	set_gmu_running(0);
}

static int gmu_is_running(void)
{
	int res;
	pthread_mutex_lock(&gmu_running_mutex);
	res = gmu_running;
	pthread_mutex_unlock(&gmu_running_mutex);
	return res;
}

TrackInfo *gmu_core_get_current_trackinfo_ref(void)
{
	return &current_track_ti;
}

static int volume = 0;

/**
 * Sets the volume level. Valid values are either
 * 0..gmu_core_get_volume_max() or -1. If -1 is specified, the volume
 * is set to the volume value stored in the configuration key
 * Gmu.Volume. Acquires CONFIG LOCK.
 */
void gmu_core_set_volume(int vol)
{
	gmu_core_config_acquire_lock();
	if (vol == -1) vol = cfg_get_int_value(config, "Gmu.Volume");
	if (vol >= 0 && vol <= gmu_core_get_volume_max()) {
		const char *vc = cfg_get_key_value(config, "Gmu.VolumeControl");
		volume = vol;
		if (strncmp(vc, "Software+Hardware", 17) == 0) {
			if (vol >= GMU_CORE_SW_VOLUME_MAX-1) {
				audio_set_volume(GMU_CORE_SW_VOLUME_MAX-1);
				hw_set_volume(vol-GMU_CORE_SW_VOLUME_MAX+2);
			} else {
				audio_set_volume(vol);
				hw_set_volume(1);
			}
		} else if (strncmp(vc, "Software", 8) == 0) {
			audio_set_volume(vol);
		} else if (strncmp(vc, "Hardware", 8) == 0) {
			hw_set_volume(vol);
		}
	}
	gmu_core_config_release_lock();
	event_queue_push(&event_queue, GMU_VOLUME_CHANGE);
}

int gmu_core_get_volume(void)
{
	return volume; /*audio_get_volume();*/
}

int gmu_core_get_volume_max(void)
{
	return volume_max;
}

const char *gmu_core_get_device_model_name(void)
{
	return hw_get_device_model_name();
}

char **gmu_core_get_file_extensions(void)
{
	return file_extensions;
}

int gmu_core_get_status(void)
{
	return player_status == PLAYING ? file_player_get_item_status() : STOPPED;
}

/* This function sets the time (in minutes) after which Gmu shuts itself down
 * and executes a command to shut down the machine. value can be either any
 * positive number to shut down after that many minutes or 0 to not shut down
 * automatically at all or -1 to shut down after all tracks in the playlist
 * have been played. */
void gmu_core_set_shutdown_time(int value)
{
	char tmp[32];
	shutdown_timer = value;
	remaining_time = (value > 0 ? value : 1);
	snprintf(tmp, 31, "%d", shutdown_timer);
	cfg_add_key(config, "Gmu.Shutdown", tmp);
}

/* Returns the remaining time until shutdown in minutes. Returns 0 if
 * no shutdown timer has been enabled. Returns -1 if shutdown is to be
 * issued after the last track has been played. */
int gmu_core_get_shutdown_time_remaining(void)
{
	return shutdown_timer > 0 ? remaining_time : shutdown_timer;
}

int gmu_core_get_shutdown_time_total(void)
{
	return shutdown_timer;
}

char *gmu_core_get_base_dir(void)
{
	return base_dir;
}

char *gmu_core_get_config_dir(void)
{
	return config_dir;
}

#ifdef GMU_MEDIALIB
static void medialib_refresh_finish_callback(void)
{
	wdprintf(V_DEBUG, "gmu", "In callback: Medialib refresh done.\n");
	event_queue_push(&event_queue, GMU_MEDIALIB_REFRESH_DONE);
}
#endif

void gmu_core_medialib_start_refresh(void)
{
#ifdef GMU_MEDIALIB
	medialib_start_refresh(&gm, medialib_refresh_finish_callback);
#endif
}

int gmu_core_medialib_search_find(GmuMedialibDataType type, const char *str)
{
#ifdef GMU_MEDIALIB
	event_queue_push(&event_queue, GMU_MEDIALIB_SEARCH_START);
	return medialib_search_find(&gm, type, str);
#else
	return 0;
#endif
}

TrackInfo gmu_core_medialib_search_fetch_next_result(void)
{
	TrackInfo res;
#ifdef GMU_MEDIALIB
	res = medialib_search_fetch_next_result(&gm);
#else
	trackinfo_init(&res, 0);
#endif
	return res;
}

void gmu_core_medialib_search_finish(void)
{
#ifdef GMU_MEDIALIB
	medialib_search_finish(&gm);
	event_queue_push(&event_queue, GMU_MEDIALIB_SEARCH_DONE);
#endif
}

int gmu_core_medialib_add_id_to_playlist(size_t id)
{
	int res = 0;
#ifdef GMU_MEDIALIB
	TrackInfo ti = medialib_get_data_for_id(&gm, id);
	if (ti.id > 0 && ti.file_name[0]) {
		gmu_core_playlist_add_file(ti.file_name);
		res = 1;
	}
	wdprintf(V_DEBUG, "core", "Added track with ID %d to playlist: %d\n", id, res);
#endif
	return res;
}

int gmu_core_medialib_browse_artists(void)
{
	int res = 0;
#ifdef GMU_MEDIALIB
	res = medialib_browse(&gm, "artist", NULL);
#endif
	return res;
}

int gmu_core_medialib_browse_albums_by_artist(const char *artist)
{
	int res = 0;
#ifdef GMU_MEDIALIB
	res = medialib_browse(&gm, "album", "artist", artist, NULL);
#endif
	return res;
}

const char *gmu_core_medialib_browse_fetch_next_result(void)
{
	const char *res = NULL;
#ifdef GMU_MEDIALIB
	res = medialib_browse_fetch_next_result(&gm);
#endif
	return res;
}

void gmu_core_medialib_browse_finish(void)
{
#ifdef GMU_MEDIALIB
	medialib_browse_finish(&gm);
#endif
}

void gmu_core_medialib_path_add(const char *path)
{
#ifdef GMU_MEDIALIB
	medialib_path_add(&gm, path);
#endif
}

static void print_cmd_help(const char *prog_name)
{
	printf("Gmu Music Player " VERSION_NUMBER "\n");
	printf("Copyright (c) 2006-2015 Johannes Heimansberg\n");
	printf("http://wej.k.vu/projects/gmu/\n");
	printf("\nUsage:\n%s [-h] [-r] [-v V] [-c file.conf] [-s theme_name] [music_file.ext] [...]\n", prog_name);
	printf("-h : Print this help\n");
	printf("-d /path/to/config/dir: Use the specified path as configuration dir.\n");
	printf("-c configfile.conf: Use the specified config file instead of gmu.conf\n");
	printf("-s theme_name: Use theme \"theme_name\"\n");
	printf("-v V : Set verbosity level to V, where V is an integer between 0 (silent) and 5 (debug).\n");
	printf("-p /path/to/feplugin.so : Load the given frontend plugin. Can be used multiple times.\n");
	printf("-l /path/to/playlistfile.m3u: Load the given playlist file instead of the default playlist.\n");
	printf("If you append files to the command line\n");
	printf("they will be added to the playlist\n");
	printf("and playback is started automatically.\n");
	printf("You can add as many files as you like.\n\n");
}

static void sig_handler(int sig)
{
	signal_received = 1;
}

static void file_extensions_load(void)
{
	const char *exts = decloader_get_all_extensions();
	size_t      start, item, i;
	file_extensions[0] = ".m3u";
	file_extensions[1] = ".pls";
	for (i = 0, start = 0, item = 2; exts[i] != '\0' && item < MAX_FILE_EXTENSIONS; i++) {
		if (exts[i] == ';') {
			file_extensions[item] = malloc(i-start+1);
			strncpy(file_extensions[item], exts+start, i-start);
			file_extensions[item][i-start] = '\0';
			item++;
			start = i+1;
		}
	}
	file_extensions[item] = NULL;
}

static void file_extensions_free(void)
{
	size_t i;
	for (i = 2; file_extensions[i]; i++)
		free(file_extensions[i]);
}

#define MAX_FRONTEND_PLUGIN_BY_CMD_ARG 16

int main(int argc, char **argv)
{
	char        *skin_file = "";
	char        *config_file = "gmu.conf", *config_file_path, *sys_config_dir = NULL;
	char         temp[512];
	int          disksync = 0;
	size_t       i;
	PB_Status    current_file_player_status = STOPPED;
	int          auto_shutdown = 0;
	time_t       start, end;
	Verbosity    v = V_INFO;
	char        *frontend_plugin_by_cmd_arg[MAX_FRONTEND_PLUGIN_BY_CMD_ARG];
	size_t       frontend_plugin_by_cmd_arg_counter = 0;
	int          pb_time = -1;
	char        *alt_playlist = NULL;

	for (i = 0; i < MAX_FRONTEND_PLUGIN_BY_CMD_ARG; i++)
		frontend_plugin_by_cmd_arg[i] = NULL;
	hw_detect_device_model();

	assign_signal_handler(SIGINT, sig_handler);
	assign_signal_handler(SIGTERM, sig_handler);

	if (!getcwd(base_dir, 255)) snprintf(base_dir, 255, ".");
	sys_config_dir = base_dir;
	config_dir = base_dir;

	for (i = 1; argv[i]; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case '-':
				case 'h':
					print_cmd_help(argv[0]);
					exit(0);
					break;
				case 'v':
					if (argc >= i+2) {
						v = atoi(argv[i+1]);
						if (v <= V_SILENT)
							v = V_SILENT;
						else if (v > V_DEBUG)
							v = V_DEBUG;
						i++;
					} else {
						wdprintf(V_ERROR, "gmu", "Invalid usage of -v: Verbosity level (0..5) required.\n");
						exit(0);
					}
					break;
				case 's':
					if (argc >= i+2) {
						skin_file = argv[i+1];
						i++;
					} else {
						wdprintf(V_ERROR, "gmu", "Invalid usage of -s: Theme name required.\n");
						exit(0);
					}
					break;
				case 'c': /* Config file */
					if (argc >= i+2) {
						config_file = argv[i+1];
						i++;
					} else {
						wdprintf(V_ERROR, "gmu", "Invalid usage of -c: Config file required.\n");
						exit(0);
					}
					break;
				case 'd': /* Config directory */
					if (argc >= i+2) {
						if (argv[i+1][0] == '/') {
							sys_config_dir = argv[i+1];
						} else {
							wdprintf(V_INFO, "gmu", "Config directory: Invalid usage of -d. Absolute path required. Ignoring.\n");
						}
						i++;
					} else {
						wdprintf(V_ERROR, "gmu", "Invalid usage of -d: Directory required.\n");
						exit(0);
					}
					break;
				case 'p': /* Load given frontend plugin */
					if (argc >= i+2) {
						if (frontend_plugin_by_cmd_arg_counter < MAX_FRONTEND_PLUGIN_BY_CMD_ARG)
							frontend_plugin_by_cmd_arg[frontend_plugin_by_cmd_arg_counter++] = argv[i+1];
						i++;
					} else {
						wdprintf(V_ERROR, "gmu", "Invalid usage of -p: Frontend plugin name required.\n");
						exit(0);
					}
					break;
				case 'l': /* Load given playlist file as Gmu's playlist */
					if (argc >= i+2) {
						alt_playlist = argv[i+1];
						i++;
					} else {
						wdprintf(V_ERROR, "gmu", "Invalid usage of -l: Playlist file required.\n");
						exit(0);
					}
					break;
				default:
					wdprintf(V_ERROR, "gmu", "Unknown parameter (-%c). Try -h for help.\n", argv[i][1]);
					exit(0);
					break;
			}
		} else {
			/*printf("FILE:%s (index: %d)\n", argv[i], i);*/
		}
	}

	wdprintf_set_verbosity(v);
	wdprintf(V_INFO, "gmu", "Detected device: %s\n", hw_get_device_model_name());

#ifdef HW_INIT_DEVICE
	hw_init_device();
#endif

	config_dir = sys_config_dir;
	config_file_path = get_config_file_path_alloc("gmu", config_file);

	wdprintf(V_INFO, "gmu", "Base directory: %s\n", base_dir);
	wdprintf(V_INFO, "gmu", "System config directory: %s\n", sys_config_dir);

	if (!event_queue_init(&event_queue)) {
		wdprintf(V_ERROR, "gmu", "Failed to initialize event_queue.\n");
		exit(11);
	}
	if (pthread_mutex_init(&config_mutex, NULL) != 0) {
		wdprintf(V_ERROR, "gmu", "Failed to initialize config mutex.\n");
		exit(10);
	}
	if (pthread_mutex_init(&gmu_running_mutex, NULL) != 0) {
		wdprintf(V_ERROR, "gmu", "Failed to initialize gmu_running_mutex.\n");
		exit(10);
	}
	if (config_file_path)
		wdprintf(V_INFO, "gmu", "Loading configuration %s...\n", config_file_path);
	else
		wdprintf(V_WARNING, "gmu", "Unable to find configuration at specified location.\n");
	gmu_core_config_acquire_lock();
	config = cfg_init();
	add_default_cfg_settings(config);
	if (config_file_path && cfg_read_config_file(config, config_file_path) != 0) {
		wdprintf(V_WARNING, "gmu", "Could not read %s. Assuming defaults.\n", config_file_path);
	}
	free(config_file_path);

	/* Set output path for gmu.conf file, based on XDG spec, usually ~/.config/gmu/gmu.conf */
	{
		char *gmu_conf_user_config_path = get_config_dir_with_name_alloc("gmu", 1, "gmu.conf");
		cfg_set_output_config_file(config, gmu_conf_user_config_path);
		free(gmu_conf_user_config_path);
	}

	if (skin_file[0] != '\0') cfg_add_key(config, "SDL.DefaultSkin", skin_file);

	/* Check for shutdown timer */
	shutdown_timer = cfg_get_int_value(config, "Gmu.Shutdown");
	remaining_time = shutdown_timer > 0 ? shutdown_timer : 1;

	/* Reader cache size */
	{
		int size = cfg_get_int_value(config, "Gmu.ReaderCache");
		int prebuffer_size;
		
		if (size < 64) size = 64; /* Assume a minimum buffer size of 64 KB */
		prebuffer_size = cfg_get_int_value(config, "Gmu.ReaderCachePrebufferSize");
		if (prebuffer_size <= 0) prebuffer_size = size / 2;
		reader_set_cache_size_kb(size, prebuffer_size);
	}

	{
		const char *vc = cfg_get_key_value(config, "Gmu.VolumeControl");
		if (strncmp(vc, "Software+Hardware", 17) == 0)
			volume_max = GMU_CORE_HW_VOLUME_MAX + GMU_CORE_SW_VOLUME_MAX - 1;
		else if (strncmp(vc, "Software", 8) == 0)
			volume_max = GMU_CORE_SW_VOLUME_MAX - 1;
		else if (strncmp(vc, "Hardware", 8) == 0)
			volume_max = GMU_CORE_HW_VOLUME_MAX;

		if (strncmp(vc, "Hardware", 8) == 0 || strncmp(vc, "Software+Hardware", 17) == 0) {
			hw_open_mixer(cfg_get_int_value(config, "Gmu.VolumeHardwareMixerChannel"));
		}
	}

#if STATIC
	decloader_load_builtin_decoders();
#else
	snprintf(temp, 511, "%s/decoders", base_dir);
	wdprintf(V_DEBUG, "gmu", "Searching for decoders in %s.\n", temp);
	wdprintf(V_DEBUG, "gmu", "%d decoders loaded successfully.\n", decloader_load_all(temp));
#endif

	/* Put available file extensions in an array */
	file_extensions_load();

	gmu_core_config_release_lock();

	audio_buffer_init();
	trackinfo_init(&current_track_ti, 1);
	playlist_init(&pl);
	if (alt_playlist) { /* Load user playlist if it has been specified with the -l cmd option */
		if (!add_pls_contents_to_playlist(&pl, alt_playlist))
			if (!add_m3u_contents_to_playlist(&pl, alt_playlist))
				wdprintf(V_WARNING, "gmu", "Unable to load user playlist: %s\n", alt_playlist);
	} else {
		/* Load playlist from playlist.m3u */
		char *playlist_m3u = get_data_dir_with_name_alloc("gmu", 0, "playlist.m3u");
		if (playlist_m3u) {
			wdprintf(V_INFO, "gmu", "Loading playlist from '%s'.\n", playlist_m3u);
			add_m3u_contents_to_playlist(&pl, playlist_m3u);
			free(playlist_m3u);
		} else {
			wdprintf(V_ERROR, "gmu", "ERROR: Unable to load playlist. Failed to create path.\n");
		}
	}
	wdprintf(V_INFO, "gmu", "Playlist length: %d items\n", playlist_get_length(&pl));
#ifdef GMU_MEDIALIB
	medialib_open(&gm);
#endif
	init_sdl(); /* Initialize SDL audio */

	/* Load frontends */
	snprintf(temp, 511, "%s/frontends", base_dir);
	wdprintf(V_DEBUG, "gmu", "Searching for frontends in %s.\n", temp);
	/* If no plugins have been specified on the command line (-p), load all plugins: */
#if STATIC
	feloader_load_builtin_frontends();
#else
	if (frontend_plugin_by_cmd_arg_counter <= 0) {
		feloader_load_all(temp);
	/* Otherwise load only the specified plugins: */
	} else {
		for (i = 0; i < frontend_plugin_by_cmd_arg_counter; i++)
			feloader_load_single_frontend(frontend_plugin_by_cmd_arg[i]);
	}
#endif

	file_player_init(&current_track_ti, cfg_get_boolean_value(config, "Gmu.DeviceCloseASAP"));
	set_default_play_mode(config, &pl);

	gmu_core_set_volume(-1); /* Load from config */

	gmu_core_config_acquire_lock();
	file_player_set_lyrics_file_pattern(cfg_get_key_value(config, "Gmu.LyricsFilePattern"));

	if (cfg_get_boolean_value(config, "Gmu.AutoPlayOnProgramStart")) {
		global_command = NEXT;
	}

	if (cfg_get_boolean_value(config, "Gmu.ResumePlayback")) {
		int item    = cfg_get_int_value(config, "Gmu.LastPlayedPlaylistItem");
		int seekpos = cfg_get_int_value(config, "Gmu.LastPlayedPlaylistItemTime");
		if (item > 0) {
			global_command = NO_CMD;
			file_player_seek(seekpos);
			gmu_core_play_pl_item(item-1);
		}
	}
	gmu_core_config_release_lock();

	time(&start);
	/* Main loop */
	set_gmu_running(1);
	while (gmu_is_running() || event_queue_is_event_waiting(&event_queue)) {
		GmuFrontend *fe = NULL;

		if (signal_received) gmu_core_quit();

		if (global_command == NO_CMD)
			event_queue_wait_for_event(&event_queue, 500);
		if (global_command == PLAY_ITEM && global_param >= 0) {
			Entry *tmp_item;
			int    fade_out_on_skip = check_fade_out_on_skip();

			playlist_get_lock(&pl);
			tmp_item = playlist_get_entry(&pl, global_param);
			wdprintf(V_DEBUG, "gmu", "Playing item %d from current playlist!\n", global_param);
			if (tmp_item != NULL) {
				playlist_set_current(&pl, tmp_item);
				file_player_play_file(playlist_get_entry_filename(&pl, tmp_item), 1, fade_out_on_skip);
			}
			playlist_release_lock(&pl);
			global_command = NO_CMD;
			global_param = 0;
		} else if (global_command == PLAY_FILE && global_filename[0] != '\0') {
			wdprintf(V_DEBUG, "gmu", "Direct file playback: %s\n", global_filename);
			playlist_get_lock(&pl);
			playlist_set_current(&pl, NULL);
			playlist_release_lock(&pl);
			file_player_play_file(global_filename, 1, check_fade_out_on_skip());
			global_command = NO_CMD;
			global_filename[0] = '\0';
		} else if ((file_player_get_item_status() == FINISHED || 
		           global_command == NEXT) && player_status == PLAYING) {
			wdprintf(V_DEBUG, "gmu", "Trying to play next track in playlist...\n");
			if (global_filename[0] != '\0' || !play_next(&pl, 0)) {
				wdprintf(V_DEBUG, "gmu", "No more tracks to play. Stopping playback.\n");
				stop_playback();
				if (shutdown_timer == -1) { /* Shutdown after last track */
					auto_shutdown = 1;
					gmu_core_quit();
				}
			}
			global_filename[0] = '\0';
			global_command = NO_CMD;
			global_param = 0;
		}

		if (trackinfo_acquire_lock(&current_track_ti)) {
			if (trackinfo_is_updated(&current_track_ti)) {
				wdprintf(
					V_INFO, "gmu", "Track info:\n\tArtist: %s\n\tTitle : %s\n\tAlbum : %s\n",
					trackinfo_get_artist(&current_track_ti),
					trackinfo_get_title(&current_track_ti),
					trackinfo_get_album(&current_track_ti)
				);
			}
			trackinfo_release_lock(&current_track_ti);
		}
		if (current_file_player_status != file_player_get_item_status()) {
			current_file_player_status = file_player_get_item_status();
			wdprintf(
				V_DEBUG, "gmu", "Item status: %s\n",
				current_file_player_status == PLAYING  ? "PLAYING"  : 
				current_file_player_status == FINISHED ? "FINISHED" : 
				current_file_player_status == STOPPED  ? "STOPPED"  : "PAUSED"
			);
			event_queue_push_with_parameter(
				&event_queue,
				GMU_PLAYBACK_STATE_CHANGE,
				gmu_core_get_status()
			);
		}

		fe = feloader_frontend_list_get_next_frontend(1);
		while (fe) {
			/* call callback functions of the frontend plugins... */
			if (*fe->mainloop_iteration) {
				wdprintf(V_DEBUG, "gmu", "Running frontend mainloop for %s.\n", fe->identifier);
				(*fe->mainloop_iteration)();
				wdprintf(V_DEBUG, "gmu", "Frontend mainloop for %s done.\n", fe->identifier);
			}
			fe = feloader_frontend_list_get_next_frontend(0);
		}

		if (shutdown_timer > 0) {
			time(&end);
			if (end - start >= 60) {
				remaining_time--;
				time(&start);
			}
		}

		if (remaining_time == 0) {
			auto_shutdown = 1;
			gmu_core_quit();
		}

		if (pb_time != file_player_playback_get_time()) {
			pb_time = file_player_playback_get_time();
			event_queue_push_with_parameter(&event_queue, GMU_PLAYBACK_TIME_CHANGE, pb_time);
		}

		while (event_queue_is_event_waiting(&event_queue)) {
			int      event_param = event_queue_get_parameter(&event_queue);
			GmuEvent event = event_queue_pop(&event_queue);

			/*wdprintf(V_DEBUG, "gmu", "Got event %d with param %d\n", event, event_param);*/
			/*wdprintf(V_DEBUG, "gmu", "Pushing event to frontends:\n");*/
			fe = feloader_frontend_list_get_next_frontend(1);
			while (fe) {
				if (event != GMU_NO_EVENT && *fe->event_callback) {
					/*wdprintf(V_DEBUG, "gmu", "- Frontend: %s\n", fe->identifier);*/
					(*fe->event_callback)(event, event_param);
				}
				fe = feloader_frontend_list_get_next_frontend(0);
			}
			/*wdprintf(V_DEBUG, "gmu", "Done.\n");*/
		}
	}

	wdprintf(
		V_DEBUG, "gmu", "Playback state: %s playlist pos: %d time: %d\n",
		file_player_get_item_status() == PLAYING ? "playing" : "stopped", 
		gmu_core_playlist_get_current_position(),
		file_player_playback_get_time()
	);

	if (file_player_get_item_status() == PLAYING) {
		unsigned int item_time = file_player_playback_get_time() / 1000;
		snprintf(temp, 10, "%d", gmu_core_playlist_get_current_position()+1);
		gmu_core_config_acquire_lock();
		cfg_add_key(config, "Gmu.LastPlayedPlaylistItem", temp);
		snprintf(temp, 10, "%d", item_time);
		cfg_add_key(config, "Gmu.LastPlayedPlaylistItemTime", temp);
		gmu_core_config_release_lock();
	} else {
		gmu_core_config_acquire_lock();
		cfg_add_key(config, "Gmu.LastPlayedPlaylistItem", "None");
		cfg_add_key(config, "Gmu.LastPlayedPlaylistItemTime", "0");
		gmu_core_config_release_lock();
	}

	wdprintf(V_INFO, "gmu", "Unloading frontends...\n");
	feloader_free();
	wdprintf(V_INFO, "gmu", "Unloading frontends done.\n");

	file_player_shutdown();
	audio_device_close();
	audio_buffer_free();

	gmu_core_config_acquire_lock();
	if (strncmp(cfg_get_key_value(config, "Gmu.VolumeControl"), "Hardware", 8) == 0 ||
	    strncmp(cfg_get_key_value(config, "Gmu.VolumeControl"), "Software+Hardware", 17) == 0)
		hw_close_mixer();

	if (cfg_get_boolean_value(config, "Gmu.RememberLastPlaylist")) {
		char *playlist_m3u;
		gmu_core_config_release_lock();
		wdprintf(V_INFO, "gmu", "Saving playlist...\n");
		playlist_m3u = get_data_dir_with_name_alloc("gmu", 1, "playlist.m3u");
		if (playlist_m3u) {
			wdprintf(V_INFO, "gmu", "Playlist file: %s\n", playlist_m3u);
			gmu_core_export_playlist(playlist_m3u);
			free(playlist_m3u);
		} else {
			wdprintf(V_ERROR, "gmu", "ERROR: Unable to save playlist. Failed to create path.\n");
		}
		disksync = 1;
		gmu_core_config_acquire_lock();
	}
	if (cfg_get_boolean_value(config, "Gmu.RememberSettings")) {
		char *playmode = NULL;
		char  volume_str[20];

		wdprintf(V_INFO, "gmu", "Saving settings...\n");

		switch (playlist_get_play_mode(&pl)) {
			case PM_CONTINUE:
				playmode = "continue";
				break;
			case PM_REPEAT_ALL:
				playmode = "repeatall";
				break;
			case PM_REPEAT_1:
				playmode = "repeat1";
				break;
			case PM_RANDOM:
				playmode = "random";
				break;
			case PM_RANDOM_REPEAT:
				playmode = "random+repeat";
				break;
		}
		cfg_add_key(config, "Gmu.DefaultPlayMode", playmode);

		snprintf(volume_str, 19, "%d", gmu_core_get_volume());
		cfg_add_key(config, "Gmu.Volume", volume_str);

		cfg_add_key(config, "Gmu.FirstRun", "no");
		cfg_write_config_file(config, NULL);
		disksync = 1;
	}
	gmu_core_config_release_lock();
	if (disksync) {
		wdprintf(V_DEBUG, "gmu", "Syncing disc...\n");
		sync();
	}

#ifdef GMU_MEDIALIB
	medialib_close(&gm);
#endif

	wdprintf(V_INFO, "gmu", "Unloading decoders...\n");
	decloader_free();
	wdprintf(V_DEBUG, "gmu", "Freeing playlist...\n");
	playlist_free(&pl);
	wdprintf(V_DEBUG, "gmu", "Freeing file extensions...\n");
	file_extensions_free();
	wdprintf(V_DEBUG, "gmu", "File extensions freed.\n");

	gmu_core_config_acquire_lock();
	if (auto_shutdown) {
		const char *cmd = cfg_get_key_value(config, "Gmu.ShutdownCommand");
		if (cmd) {
			wdprintf(V_INFO, "gmu", "Executing shutdown command: \'%s\'\n", cmd);
			wdprintf(V_INFO, "gmu", "Shutdown command completed with return code %d.\n", system(cmd));
		}
	}
	cfg_free(config);
	gmu_core_config_release_lock();
	pthread_mutex_destroy(&config_mutex);
	pthread_mutex_destroy(&gmu_running_mutex);
	SDL_Quit();
	event_queue_free(&event_queue);
	wdprintf(V_INFO, "gmu", "Shutdown complete.\n");
	return 0;
}
