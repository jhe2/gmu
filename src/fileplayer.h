/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: fileplayer.h  Created: 070107
 *
 * Description: Functions for audio file playback
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _FILEPLAYER_H
#define _FILEPLAYER_H
#include "trackinfo.h"
#include "pbstatus.h"

void      file_player_set_lyrics_file_pattern(char *pattern);
int       file_player_playback_get_time(void);
PB_Status file_player_get_playback_status(void);
PB_Status file_player_get_item_status(void);
void      file_player_stop_playback(void);
int       file_player_play_file(char *file, TrackInfo *ti, int skip_current);
int       file_player_read_tags(char *file, char *file_type, TrackInfo *ti);
int       file_player_seek(long offset);
int       file_player_is_metadata_loaded(void);
int       file_player_is_thread_running(void);
void      file_player_shutdown(void);
void      file_player_set_filename(char *filename);
void      file_player_start_playback(void);
int       file_player_init(void);
#endif
