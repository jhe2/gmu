/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: setup.h  Created: 141102
 *
 * Description: Setup dialog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "kam.h"
#include "wejconfig.h"

#ifndef WEJ_SETUP_H
#define WEJ_SETUP_H
#define MAX_PRESETS_PER_KEY 7

typedef struct SetupDialog {
	Skin       *skin;
	ConfigFile *config;
	/**
	 * Stores the available config keys (in position 0) and up to five
	 * preset values for each key (in position 1 to 5).
	 */
	char       *keys[MAXKEYS][MAX_PRESETS_PER_KEY];
	/**
	 * Stores the selected value as an integer offset, or -1 if no value
	 * or an unknown value has been selected.
	 */
	int         selected_value[MAXKEYS];
	/**
	 * Stores the actual number of keys.
	 */
	int         key_count;
	/**
	 * Stores the line offset for the viewport
	 */
	int         offset;
	/**
	 * Stores the cursor (selection) position
	 */
	int         cursor_x, cursor_y;
	int         visible_lines;
} SetupDialog;

int  setup_process_action(SetupDialog *setup_dlg, View *view, View old_view, int user_key_action);
void setup_init(SetupDialog *setup_dlg, Skin *skin);
void setup_draw(SetupDialog *setup_dlg, SDL_Surface *sdl_target);
void setup_cursor_move(SetupDialog *setup_dlg, int x_offset, int y_offset);
void setup_shutdown(SetupDialog *setup_dlg);
#endif
