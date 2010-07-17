/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: plmanager.h  Created: 061223
 *
 * Description: Playlist save dialog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "SDL.h"
#include "kam.h"
#include "skin.h"
#ifndef _PLMANAGER_H
#define _PLMANAGER_H
#define PLMANAGER_MAX_ITEMS 32
#define PLMANAGER_MAX_ITEM_SIZE 256
typedef struct PlaylistManager
{
	Skin *skin;
	int   selection;
	int   items;
	int   flag;
	char  filenames[PLMANAGER_MAX_ITEMS][PLMANAGER_MAX_ITEM_SIZE];
} PlaylistManager;

enum { PLMANAGER_NOTHING, PLMANAGER_SAVE_LIST, PLMANAGER_LOAD_LIST, PLMANAGER_APPEND_LIST };

int   plmanager_get_flag(PlaylistManager *ps);
void  plmanager_reset_flag(PlaylistManager *ps);
int   plmanager_process_action(PlaylistManager *ps, View *view, int user_key_action);
void  plmanager_draw(PlaylistManager *ps, SDL_Surface *sdl_target);
void  plmanager_init(PlaylistManager *ps, char *filenames_list, Skin *skin);
char *plmanager_get_selection(PlaylistManager *ps);
void  plmanager_move_selection_down(PlaylistManager *ps);
void  plmanager_move_selection_up(PlaylistManager *ps);
#endif
