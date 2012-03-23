/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: playerdisplay.h  Created: 061109
 *
 * Description: Gmu's display
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "SDL.h"
#include "textrenderer.h"
#include "trackinfo.h"
#include "pbstatus.h"

#ifndef _PLAYERDISPLAY_H
#define _PLAYERDISPLAY_H

enum { SCROLL_AUTO, SCROLL_ALWAYS, SCROLL_NEVER };

void player_display_draw(TextRenderer *tr, TrackInfo *ti,PB_Status player_status,
                         int ptime_msec, int ptime_remaining, int volume,
                         int busy, int shutdown_time, SDL_Surface *buffer);
void player_display_set_notice_message(char *message, int timeout);
void player_display_set_scrolling(int s);
void player_display_set_playback_symbol_blinking(int blink);
#endif
