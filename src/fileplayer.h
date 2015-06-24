/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
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

int       file_player_check_shutdown(void);
void      file_player_set_lyrics_file_pattern(const char *pattern);
int       file_player_playback_get_time(void);
PB_Status file_player_get_item_status(void);
void      file_player_stop_playback(void);
int       file_player_play_file(char *file, int skip_current, int fade_out_on_skip);
int       file_player_read_tags(char *file, char *file_type, TrackInfo *ti);
int       file_player_seek(long offset);
int       file_player_is_thread_running(void);
void      file_player_shutdown(void);
void      file_player_set_filename(char *filename);
void      file_player_start_playback(void);
int       file_player_init(TrackInfo *ti_ref, int device_close_asap);
TrackInfo *file_player_get_trackinfo_ref(void);
int       file_player_request_playback_state_change(PB_Status_Request request);
#endif
