/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
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
#include "wejpconfig.h"
#include FILE_HW_H
#include "util.h"
#include "reader.h" /* for reader_set_cache_size_kb() */
#include "debug.h"
#define MAX_FILE_EXTENSIONS 255

typedef enum GlobalCommand { NO_CMD, PLAY, PAUSE, STOP, NEXT, 
                             PREVIOUS, PLAY_ITEM, PLAY_FILE } GlobalCommand;

static GlobalCommand global_command = NO_CMD;
static int           global_param = 0;
static char          global_filename[256];
static int           gmu_running = 0;
static Playlist      pl;
static TrackInfo     current_track_ti;
static EventQueue    event_queue;
static ConfigFile    config;
static char         *file_extensions[MAX_FILE_EXTENSIONS+1];
static int           volume_max;
static unsigned int  player_status = STOPPED;
static int           shutdown_timer = 0;
static int           remaining_time;
static char          base_dir[256], *config_dir;


static void init_sdl(void)
{
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

static int add_m3u_contents_to_playlist(Playlist *pl, char *filename)
{
	M3u m3u;
	int res = 0;
	if (m3u_open_file(&m3u, filename)) {
		int len;
		len = playlist_get_length(pl);
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

void gmu_core_add_m3u_contents_to_playlist(char *filename)
{
	playlist_get_lock(&pl);
	add_m3u_contents_to_playlist(&pl, filename);
	playlist_release_lock(&pl);
}

static int add_pls_contents_to_playlist(Playlist *pl, char *filename)
{
	PLS pls;
	int res = 0;
	if (pls_open_file(&pls, filename)) {
		int len;
		len = playlist_get_length(pl);
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

void gmu_core_add_pls_contents_to_playlist(char *filename)
{
	playlist_get_lock(&pl);
	add_pls_contents_to_playlist(&pl, filename);
	playlist_release_lock(&pl);
}

static int play_next(Playlist *pl, TrackInfo *ti, int skip_current)
{
	int result = 0;
	playlist_get_lock(pl);
	if (playlist_next(pl)) {
		file_player_play_file(playlist_get_entry_filename(pl, playlist_get_current(pl)), ti, skip_current);
		result = 1;
		event_queue_push(&event_queue, GMU_TRACK_CHANGE);
		player_status = PLAYING;
	}
	playlist_release_lock(pl);
	return result;
}

static int play_previous(Playlist *pl, TrackInfo *ti)
{
	int result = 0;
	playlist_get_lock(pl);
	if (playlist_prev(pl)) {
		Entry *entry = playlist_get_current(pl);
		if (entry != NULL) {
			file_player_play_file(playlist_get_entry_filename(pl, entry), ti, 1);
			result = 1;
			event_queue_push(&event_queue, GMU_TRACK_CHANGE);
			player_status = PLAYING;
		}
	}
	playlist_release_lock(pl);
	return result;
}

static void add_default_cfg_settings(ConfigFile *config)
{
	cfg_add_key(config, "DefaultPlayMode", "continue");
	cfg_add_key(config, "RememberLastPlaylist", "yes");
	cfg_add_key(config, "RememberSettings", "yes");
	cfg_add_key(config, "EnableCoverArtwork", "yes");
	cfg_add_key(config, "CoverArtworkFilePattern", "*.jpg");
	cfg_add_key(config, "LoadEmbeddedCoverArtwork", "first");
	cfg_add_key(config, "LyricsFilePattern", "$.txt;*.txt");
	cfg_add_key(config, "FileSystemCharset", "UTF-8");
	cfg_add_key(config, "PlaylistSavePresets",
						 "playlist1.m3u;playlist2.m3u;playlist3.m3u;playlist4.m3u");
	cfg_add_key(config, "TimeDisplay", "elapsed");
	cfg_add_key(config, "DefaultFileBrowserPath", ".");
	cfg_add_key(config, "VolumeControl", "Software"); /* Software, Hardware, Hardware+Software */
	cfg_add_key(config, "VolumeHardwareMixerChannel", "0");
	cfg_add_key(config, "Volume", "7");
	cfg_add_key(config, "DefaultSkin", "default-modern");
	cfg_add_key(config, "AutoSelectCurrentPlaylistItem", "no");
	cfg_add_key(config, "AutoPlayOnProgramStart", "no");
	cfg_add_key(config, "Scroll", "always");
	cfg_add_key(config, "FileBrowserFoldersFirst", "yes");
	cfg_add_key(config, "BacklightPowerOnOnTrackChange", "no");
	cfg_add_key(config, "KeyMap", "default.keymap");
	cfg_add_key(config, "AllowVolumeControlInHoldState", "no");
	cfg_add_key(config, "SecondsUntilBacklightPowerOff", "30");
	cfg_add_key(config, "CoverArtworkLarge", "no");
	cfg_add_key(config, "SmallCoverArtworkAlignment", "right");
	cfg_add_key(config, "FirstRun", "yes");
	cfg_add_key(config, "ResumePlayback", "yes");
	cfg_add_key(config, "ReaderCache", "512");
	cfg_add_key(config, "ReaderCachePrebufferSize", "256");
}

int gmu_core_export_playlist(char *file)
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
	playlist_get_lock(pl);
	if (strncmp(cfg_get_key_value(*config, "DefaultPlayMode"), "random", 6) == 0)
		playlist_set_play_mode(pl, PM_RANDOM);
	else if (strncmp(cfg_get_key_value(*config, "DefaultPlayMode"), "repeat1", 11) == 0)
		playlist_set_play_mode(pl, PM_REPEAT_1);
	else if (strncmp(cfg_get_key_value(*config, "DefaultPlayMode"), "repeatall", 9) == 0)
		playlist_set_play_mode(pl, PM_REPEAT_ALL);
	else if (strncmp(cfg_get_key_value(*config, "DefaultPlayMode"), "random+repeat", 13) == 0)
		playlist_set_play_mode(pl, PM_RANDOM_REPEAT);
	playlist_release_lock(pl);
}

ConfigFile *gmu_core_get_config(void)
{
	return &config;
}

/* Start playback */
int gmu_core_play(void)
{
	int res = 0;
	if (file_player_get_playback_status() == STOPPED)
		res = gmu_core_next();
	else
		audio_set_pause(0);
	player_status = PLAYING;
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
	audio_set_pause(1);
	player_status = PAUSED;
	event_queue_push(&event_queue, GMU_PLAYBACK_STATE_CHANGE);
	return 0;
}

int gmu_core_next(void)
{
	return play_next(&pl, &current_track_ti, 1);
}

int gmu_core_previous(void)
{
	return play_previous(&pl, &current_track_ti);
}

static int stop_playback(void)
{
	int res = 0;
	if (player_status != STOPPED) {
		file_player_stop_playback();
		gmu_core_playlist_reset_random();
		gmu_core_playlist_set_current(NULL);
		player_status = STOPPED;
		event_queue_push(&event_queue, GMU_PLAYBACK_STATE_CHANGE);
		res = 1;
	}
	return res;
}

int gmu_core_stop(void)
{
	audio_set_pause(1);
	return stop_playback();
}

int gmu_core_play_pl_item(int item)
{
	global_command = PLAY_ITEM;
	global_param = item;
	player_status = PLAYING;
	event_queue_push(&event_queue, GMU_TRACK_CHANGE);
	return 0;
}

int gmu_core_play_file(const char *filename)
{
	strncpy(global_filename, filename, 255);
	global_command = PLAY_FILE;
	player_status = PLAYING;
	event_queue_push(&event_queue, GMU_TRACK_CHANGE);
	return 0;
}

/* Playlist wrapper functions: */
int gmu_core_playlist_set_current(Entry *entry)
{
	int res;
	playlist_get_lock(&pl);
	res = playlist_set_current(&pl, entry);
	playlist_release_lock(&pl);
	/*event_queue_push(&event_queue, GMU_PLAYLIST_CHANGE);*/
	return res;
}

int gmu_core_playlist_add_item(char *file, char *name)
{
	int res, len;
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

static void add_dir_finish_callback(int pos)
{
	playlist_get_lock(&pl);
	playlist_release_lock(&pl);
	wdprintf(V_DEBUG, "gmu", "In callback: Recursive directory add done.\n");
	event_queue_push_with_parameter(&event_queue, GMU_PLAYLIST_CHANGE, pos);
}

int gmu_core_playlist_add_dir(char *dir)
{
	int res;
	playlist_get_lock(&pl);
	res = playlist_add_dir(&pl, dir, add_dir_finish_callback);
	playlist_release_lock(&pl);
	return res;
}

Entry *gmu_core_playlist_get_first(void)
{
	Entry *first;
	playlist_get_lock(&pl);
	first = playlist_get_first(&pl);
	playlist_release_lock(&pl);
	return first;
}

int gmu_core_playlist_get_length(void)
{
	int len;
	playlist_get_lock(&pl);
	len = playlist_get_length(&pl);
	playlist_release_lock(&pl);
	return len;
}

int gmu_core_playlist_insert_file_after(Entry *entry, char *filename_with_path)
{
	int res;
	playlist_get_lock(&pl);
	res = playlist_insert_file_after(&pl, entry, filename_with_path);
	playlist_release_lock(&pl);
	event_queue_push(&event_queue, GMU_PLAYLIST_CHANGE);
	return res;
}

int gmu_core_playlist_add_file(char *filename_with_path)
{
	int   res, len;
	char  filetype[16] = "(none)";
	char *tmp = filename_with_path ? get_file_extension(filename_with_path) : NULL;
	if (tmp != NULL) strtoupper(filetype, tmp, 15);
	filetype[15] = '\0';
	playlist_get_lock(&pl);
	len = playlist_get_length(&pl);
	if (strcmp(filetype, "M3U") == 0) {
		res = add_m3u_contents_to_playlist(&pl, filename_with_path);
	} else if (strcmp(filetype, "PLS") == 0) {
		res = add_pls_contents_to_playlist(&pl, filename_with_path);
	} else {
		res = playlist_add_file(&pl, filename_with_path);
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
	playlist_get_lock(&pl);
	res = playlist_entry_enqueue(&pl, entry);
	playlist_release_lock(&pl);
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
	playlist_get_lock(&pl);
	e = playlist_get_entry(&pl, item);
	playlist_release_lock(&pl);
	return e;
}

int gmu_core_playlist_entry_delete(Entry *entry)
{
	int res;
	playlist_get_lock(&pl);
	res = playlist_entry_delete(&pl, entry);
	playlist_release_lock(&pl);
	event_queue_push(&event_queue, GMU_PLAYLIST_CHANGE);
	return res;
}

Entry *gmu_core_playlist_item_delete(int item)
{
	Entry *next = NULL;
	playlist_get_lock(&pl);
	next = playlist_item_delete(&pl, item);
	playlist_release_lock(&pl);
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
	Entry *e;
	playlist_get_lock(&pl);
	e = playlist_get_next(entry);
	playlist_release_lock(&pl);
	return e;
}

Entry *gmu_core_playlist_get_prev(Entry *entry)
{
	Entry *e;
	playlist_get_lock(&pl);
	e = playlist_get_prev(entry);
	playlist_release_lock(&pl);
	return e;
}

int gmu_core_playlist_get_played(Entry *entry)
{
	int res;
	playlist_get_lock(&pl);
	res = playlist_get_played(entry);
	playlist_release_lock(&pl);
	return res;
}

int gmu_core_playlist_entry_get_queue_pos(Entry *entry)
{
	return playlist_entry_get_queue_pos(entry);
}

int gmu_core_get_length_current_track(void)
{
	return current_track_ti.length;
}

EventQueue *gmu_core_get_event_queue(void)
{
	return &event_queue;
}

void gmu_core_quit(void)
{
	wdprintf(V_INFO, "gmu", "Shutting down...\n");
	event_queue_push(&event_queue, GMU_QUIT);
	gmu_running = 0;
}

TrackInfo *gmu_core_get_current_trackinfo_ref(void)
{
	return &current_track_ti;
}

static int volume = 0;

void gmu_core_set_volume(int vol) /* 0..gmu_core_get_volume_max() */
{
	if (vol >= 0 && vol <= gmu_core_get_volume_max()) {
		volume = vol;
		if (strncmp(cfg_get_key_value(config, "VolumeControl"), "Software+Hardware", 17) == 0) {
			if (vol >= GMU_CORE_SW_VOLUME_MAX-1) {
				audio_set_volume(GMU_CORE_SW_VOLUME_MAX-1);
				hw_set_volume(vol-GMU_CORE_SW_VOLUME_MAX+2);
			} else {
				audio_set_volume(vol);
				hw_set_volume(1);
			}
		} else if (strncmp(cfg_get_key_value(config, "VolumeControl"), "Software", 8) == 0) {
			audio_set_volume(vol);
		} else if (strncmp(cfg_get_key_value(config, "VolumeControl"), "Hardware", 8) == 0) {
			hw_set_volume(vol);
		}
	}
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

const char **gmu_core_get_file_extensions(void)
{
	return (const char **)file_extensions;
}

int gmu_core_get_status(void)
{
	return player_status;
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
	cfg_add_key(&config, "Shutdown", tmp);
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

static void print_cmd_help(char *prog_name)
{
	printf("Gmu Music Player " VERSION_NUMBER "\n");
	printf("Copyright (c) 2006-2013 Johannes Heimansberg\n");
	printf("http://wejp.k.vu/projects/gmu/\n");
	printf("\nUsage:\n%s [-h] [-r] [-v V] [-c file.conf] [-s theme_name] [music_file.ext] [...]\n", prog_name);
	printf("-h : Print this help\n");
	printf("-d /path/to/config/dir: Use the specified path as configuration dir.\n");
	printf("-e : Store user configuration in user's home directory (~/.config/gmu/)\n");
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

static int init_user_config_dir(char *user_config_dir, char *sys_config_dir, char *config_file)
{
	int   result = 0;
	char *home = getenv("HOME");

	if (home) {
		char       target[384], source[384], *filename = NULL;
		ConfigFile cf;

		/* Try to create the config directory */
		snprintf(user_config_dir, 255, "%s/.config", home);
		mkdir(user_config_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
		snprintf(user_config_dir, 255, "%s/.config/gmu", home);
		wdprintf(V_DEBUG, "gmu", "User config directory: %s\n", user_config_dir);
		mkdir(user_config_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

		/* Copy all missing config files from system config dirto home config dir */
		if (config_file[0] == '/') {
			filename = strrchr(config_file, '/');
			filename++;
		} else {
			filename = config_file;
		}
		snprintf(target, 383, "%s/%s", user_config_dir, filename);
		if (!file_exists(target)) {
			wdprintf(V_INFO, "gmu", "Copying file: %s\n", filename);
			snprintf(source, 383, "%s/%s", sys_config_dir, filename);
			file_copy(target, source);
		}
		cfg_init_config_file_struct(&cf);
		if (cfg_read_config_file(&cf, target) == 0) {
			char *file = cfg_get_key_value(cf, "KeyMap");
			if (file) {
				snprintf(target, 383, "%s/%s", user_config_dir, file);
				if (!file_exists(target)) {
					wdprintf(V_INFO, "gmu", "Copying file: %s\n", file);
					snprintf(source, 383, "%s/%s", sys_config_dir, file);
					if (file_copy(target, source)) result = 1;
				} else {
					result = 1;
				}
			}
			file = cfg_get_key_value(cf, "InputConfigFile");
			if (result) {
				if (!file) file = "gmuinput.conf";
				snprintf(target, 383, "%s/%s", user_config_dir, file);
				if (!file_exists(target)) {
					wdprintf(V_INFO, "gmu", "Copying file: %s\n", file);
					snprintf(source, 383, "%s/%s", sys_config_dir, file);
					if (!file_copy(target, source)) result = 0;
				}
			}				
		}
		cfg_free_config_file_struct(&cf);
	} else {
		wdprintf(V_ERROR, "gmu", "Unable to find user's home directory.\n");
	}
	return result;
}

static void sig_handler(int sig)
{
	wdprintf(V_DEBUG, "gmu", "Exit requested.\n");
	gmu_core_quit();
}

static void file_extensions_load(void)
{
	const char *exts = decloader_get_all_extensions();
	int start, item, i;
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
	int i;
	for (i = 2; file_extensions[i]; i++)
		free(file_extensions[i]);
}

#define MAX_FRONTEND_PLUGIN_BY_CMD_ARG 16

int main(int argc, char **argv)
{
	char        *skin_file = "";
	char         user_config_dir[256] = "0";
	char        *config_file = "gmu.conf", *config_file_path, *sys_config_dir = NULL;
	char         temp[512];
	int          disksync = 0;
	int          i;
	unsigned int statu = STOPPED;
	int          auto_shutdown = 0;
	time_t       start, end;
	Verbosity    v = V_INFO;
	char        *frontend_plugin_by_cmd_arg[MAX_FRONTEND_PLUGIN_BY_CMD_ARG];
	int          frontend_plugin_by_cmd_arg_counter = 0;
	int          pb_time = -1;
	char        *alt_playlist = NULL;

	for (i = 0; i < MAX_FRONTEND_PLUGIN_BY_CMD_ARG; i++)
		frontend_plugin_by_cmd_arg[i] = NULL;
	hw_detect_device_model();

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

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
				case 'e': /* Store config in user's home directory */
					user_config_dir[0] = '1';
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
	if (user_config_dir[0] == '1') {
		if (init_user_config_dir(user_config_dir, sys_config_dir, config_file)) {
			/* Set config_dir to the user config directory */
			config_dir = user_config_dir;
		}
	}

	if (config_file[0] != '/') { /* Relative path to config file given? */
		config_file_path = alloca(strlen(base_dir)+strlen(config_file)+2);
		snprintf(config_file_path, 255, "%s/%s", config_dir, config_file);
	} else {
		config_file_path = config_file;
	}

	wdprintf(V_INFO, "gmu", "Base directory: %s\n", base_dir);
	wdprintf(V_INFO, "gmu", "System config directory: %s\n", sys_config_dir);

	event_queue_init(&event_queue);
	wdprintf(V_INFO, "gmu", "Loading configuration %s...\n", config_file_path);
	cfg_init_config_file_struct(&config);
	add_default_cfg_settings(&config);
	if (cfg_read_config_file(&config, config_file_path) != 0) {
		wdprintf(V_ERROR, "gmu", "Could not read %s. Assuming defaults.\n", config_file_path);
		/* In case of a missing default config file create a new one: */
		if (strncmp(config_file, config_file_path, 8) == 0) {
			wdprintf(V_INFO, "gmu", "Creating config file.\n");
			cfg_write_config_file(&config, config_file_path);
		}
	}
	
	if (skin_file[0] != '\0') cfg_add_key(&config, "DefaultSkin", skin_file);

	/* Check for shutdown timer */
	{
		char *tmp = cfg_get_key_value(config, "Shutdown");
		if (tmp) shutdown_timer = atoi(tmp);
		remaining_time = shutdown_timer > 0 ? shutdown_timer : 1;
	}

	/* Reader cache size */
	{
		char *tmp = cfg_get_key_value(config, "ReaderCache");
		if (tmp) {
			int size = atoi(tmp), prebuffer_size = size / 2;
			tmp = cfg_get_key_value(config, "ReaderCachePrebufferSize");
			if (tmp) prebuffer_size = atoi(tmp);
			reader_set_cache_size_kb(size, prebuffer_size);
		}
	}

	if (strncmp(cfg_get_key_value(config, "VolumeControl"), "Software+Hardware", 17) == 0)
		volume_max = GMU_CORE_HW_VOLUME_MAX + GMU_CORE_SW_VOLUME_MAX - 1;
	else if (strncmp(cfg_get_key_value(config, "VolumeControl"), "Software", 8) == 0)
		volume_max = GMU_CORE_SW_VOLUME_MAX - 1;
	else if (strncmp(cfg_get_key_value(config, "VolumeControl"), "Hardware", 8) == 0)
		volume_max = GMU_CORE_HW_VOLUME_MAX;

#if STATIC
	decloader_load_builtin_decoders();
#else
	snprintf(temp, 511, "%s/decoders", base_dir);
	wdprintf(V_DEBUG, "gmu", "Searching for decoders in %s.\n", temp);
	wdprintf(V_DEBUG, "gmu", "%d decoders loaded successfully.\n", decloader_load_all(temp));
#endif

	/* Put available file extensions in an array */
	file_extensions_load();

	if (strncmp(cfg_get_key_value(config, "VolumeControl"), "Hardware", 8) == 0 ||
	    strncmp(cfg_get_key_value(config, "VolumeControl"), "Software+Hardware", 17) == 0) {
		if (cfg_get_key_value(config, "VolumeHardwareMixerChannel"))
			hw_open_mixer(atoi(cfg_get_key_value(config, "VolumeHardwareMixerChannel")));
	}

	audio_buffer_init();
	trackinfo_init(&current_track_ti);
	playlist_init(&pl);
	if (alt_playlist) { /* Load user playlist if it has been specified with the -l cmd option */
		if (!add_pls_contents_to_playlist(&pl, alt_playlist))
			if (!add_m3u_contents_to_playlist(&pl, alt_playlist))
				wdprintf(V_WARNING, "gmu", "Unable to load user playlist: %s\n", alt_playlist);
	} else {
		/* Load playlist from playlist.m3u */
		snprintf(temp, 255, "%s/playlist.m3u", config_dir);
		add_m3u_contents_to_playlist(&pl, temp);
	}
	wdprintf(V_INFO, "gmu", "Playlist length: %d items\n", playlist_get_length(&pl));

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

	file_player_init();
	file_player_set_lyrics_file_pattern(cfg_get_key_value(config, "LyricsFilePattern"));

	set_default_play_mode(&config, &pl);
	gmu_core_set_volume(atoi(cfg_get_key_value(config, "Volume")));

	if (strncmp(cfg_get_key_value(config, "AutoPlayOnProgramStart"), "yes", 3) == 0) {
		global_command = NEXT;
	}

	if (strncmp(cfg_get_key_value(config, "ResumePlayback"), "yes", 3) == 0) {
		char *itemstr =  cfg_get_key_value(config, "LastPlayedPlaylistItem");
		char *seekposstr = cfg_get_key_value(config, "LastPlayedPlaylistItemTime");
		int   item = (itemstr ? atoi(itemstr) : 0);
		int   seekpos = (seekposstr ? atoi(seekposstr) : 0);
		if (item > 0) {
			global_command = NO_CMD;
			file_player_seek(seekpos);
			gmu_core_play_pl_item(item-1);
		}
	}

	time(&start);
	/* Main loop */
	gmu_running = 1;
	while (gmu_running || event_queue_is_event_waiting(&event_queue)) {
		GmuFrontend *fe = NULL;

		if (global_command == NO_CMD)
			event_queue_wait_for_event(&event_queue, 500);
		if (global_command == PLAY_ITEM && global_param >= 0) {
			Entry *tmp_item;
			
			playlist_get_lock(&pl);
			tmp_item = playlist_get_entry(&pl, global_param);
			wdprintf(V_DEBUG, "gmu", "Playing item %d from current playlist!\n", global_param);
			if (tmp_item != NULL) {
				playlist_set_current(&pl, tmp_item);
				file_player_play_file(playlist_get_entry_filename(&pl, tmp_item), &current_track_ti, 1);
			}
			playlist_release_lock(&pl);
			global_command = NO_CMD;
			global_param = 0;
		} else if (global_command == PLAY_FILE && global_filename[0] != '\0') {
			wdprintf(V_DEBUG, "gmu", "Direct file playback: %s\n", global_filename);
			playlist_get_lock(&pl);
			playlist_set_current(&pl, NULL);
			playlist_release_lock(&pl);
			file_player_play_file(global_filename, &current_track_ti, 1);
			global_command = NO_CMD;
		} else if ((file_player_get_item_status() == FINISHED && 
		            file_player_get_playback_status() == PLAYING) || 
		           global_command == NEXT) {
			if (global_filename[0] != '\0' || !play_next(&pl, &current_track_ti, 0)) {
				statu = STOPPED; /* no next track? -> STOP */
				stop_playback();
				if (shutdown_timer == -1) { /* Shutdown after last track */
					auto_shutdown = 1;
					event_queue_push(&event_queue, GMU_QUIT);
					gmu_running = 0;
				}
			}
			global_filename[0] = '\0';
			global_command = NO_CMD;
			global_param = 0;
		}
		if (trackinfo_is_updated(&current_track_ti)) {
			wdprintf(V_INFO, "gmu", "Track info:\n\tArtist: %s\n\tTitle : %s\n\tAlbum : %s\n",
			         trackinfo_get_artist(&current_track_ti),
				     trackinfo_get_title(&current_track_ti),
				     trackinfo_get_album(&current_track_ti));
		}
		if (statu != file_player_get_item_status()) {
			statu = file_player_get_item_status();
			wdprintf(V_DEBUG, "gmu", "Item status: %s\n", statu == PLAYING  ? "PLAYING"  : 
			                                              statu == FINISHED ? "FINISHED" : 
			                                              statu == STOPPED  ? "STOPPED"  : "PAUSED");
			event_queue_push(&event_queue, GMU_PLAYBACK_STATE_CHANGE);
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
			event_queue_push(&event_queue, GMU_QUIT);
			gmu_running = 0;
		}

		if (pb_time != file_player_playback_get_time()) {
			pb_time = file_player_playback_get_time();
			event_queue_push_with_parameter(&event_queue, GMU_PLAYBACK_TIME_CHANGE, pb_time);
		}

		while (event_queue_is_event_waiting(&event_queue)) {
			int      event_param = event_queue_get_parameter(&event_queue);
			GmuEvent event = event_queue_pop(&event_queue);

			wdprintf(V_DEBUG, "gmu", "Got event %d with param %d\n", event, event_param);
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

	wdprintf(V_DEBUG, "gmu", "Playback state: %s playlist pos: %d time: %d\n",
	         file_player_get_item_status() == PLAYING ? "playing" : "stopped", 
	         gmu_core_playlist_get_current_position(),
	         file_player_playback_get_time());

	if (file_player_get_item_status() == PLAYING) {
		snprintf(temp, 10, "%d", gmu_core_playlist_get_current_position()+1);
		cfg_add_key(&config, "LastPlayedPlaylistItem", temp);
		snprintf(temp, 10, "%d", file_player_playback_get_time() / 1000);
		cfg_add_key(&config, "LastPlayedPlaylistItemTime", temp);
	} else {
		cfg_add_key(&config, "LastPlayedPlaylistItem", "None");
		cfg_add_key(&config, "LastPlayedPlaylistItemTime", "0");
	}

	file_player_shutdown();
	while (file_player_is_thread_running())	usleep(50);
	while (file_player_is_thread_running())	usleep(50); /* In case another thread had been started in the meantime */
	audio_device_close();
	audio_buffer_free();

	if (strncmp(cfg_get_key_value(config, "VolumeControl"), "Hardware", 8) == 0 ||
	    strncmp(cfg_get_key_value(config, "VolumeControl"), "Software+Hardware", 17) == 0)
		hw_close_mixer();

	wdprintf(V_INFO, "gmu", "Unloading frontends...\n");
	feloader_free();

	if (strncmp(cfg_get_key_value(config, "RememberLastPlaylist"), "yes", 3) == 0) {
		wdprintf(V_INFO, "gmu", "Saving playlist...\n");
		snprintf(temp, 255, "%s/playlist.m3u", config_dir);
		wdprintf(V_INFO, "gmu", "Playlist file: %s\n", temp);
		gmu_core_export_playlist(temp);
		disksync = 1;
	}
	if (strncmp(cfg_get_key_value(config, "RememberSettings"), "yes", 3) == 0) {
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
		cfg_add_key(&config, "DefaultPlayMode", playmode);

		snprintf(volume_str, 19, "%d", gmu_core_get_volume());
		cfg_add_key(&config, "Volume", volume_str);

		cfg_add_key(&config, "FirstRun", "no");
		cfg_write_config_file(&config, config_file_path);
		disksync = 1;
	}
	if (disksync) {
		wdprintf(V_DEBUG, "gmu", "Syncing disc...\n");
		sync();
	}

	wdprintf(V_INFO, "gmu", "Unloading decoders...\n");
	decloader_free();
	wdprintf(V_DEBUG, "gmu", "Freeing playlist...\n");
	playlist_free(&pl);
	wdprintf(V_DEBUG, "gmu", "Freeing file extensions...\n");
	file_extensions_free();
	wdprintf(V_DEBUG, "gmu", "File extensions freed.\n");

	if (auto_shutdown) {
		char *tmp = cfg_get_key_value(config, "ShutdownCommand");
		if (tmp) {
			wdprintf(V_INFO, "gmu", "Executing shutdown command: \'%s\'\n", tmp);
			wdprintf(V_INFO, "gmu", "Shutdown command completed with return code %d.\n", system(tmp));
		}
	}
	cfg_free_config_file_struct(&config);
	SDL_Quit();
	event_queue_free(&event_queue);
	wdprintf(V_INFO, "gmu", "Shutdown complete.\n");
	return 0;
}
