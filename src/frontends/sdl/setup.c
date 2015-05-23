/* 
 * Gmu GP2X Music Player
 *
 * Copyright (c) 2006-2015 Johannes Heimansberg (wejp.k.vu)
 *
 * File: setup.c  Created: 141102
 *
 * Description: Setup dialog
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
#include "setup.h"
#include "core.h"
#include "debug.h"
#include "wejconfig.h"

int setup_process_action(SetupDialog *setup_dlg, View *view, View old_view, int user_key_action)
{
	int update = 0;
	switch (user_key_action) {
		case OKAY:
		case SETUP_CLOSE:
			gmu_core_config_acquire_lock();
			cfg_write_config_file(setup_dlg->config, NULL);
			gmu_core_config_release_lock();
			*view = old_view;
			update = 1;
			break;
		case MOVE_CURSOR_DOWN:
			setup_cursor_move(setup_dlg, 0, 1);
			update = 1;
			break;
		case MOVE_CURSOR_UP:
			setup_cursor_move(setup_dlg, 0, -1);
			update = 1;
			break;
		case MOVE_CURSOR_LEFT:
			setup_cursor_move(setup_dlg, -1, 0);
			update = 1;
			break;
		case MOVE_CURSOR_RIGHT:
			setup_cursor_move(setup_dlg, 1, 0);
			update = 1;
			break;
		case SETUP_SELECT:
			wdprintf(V_DEBUG, "setup",
				"Selection: %s -> %s\n",
				setup_dlg->keys[setup_dlg->cursor_y][0],
				setup_dlg->keys[setup_dlg->cursor_y][setup_dlg->cursor_x+1]
			);
			setup_dlg->selected_value[setup_dlg->cursor_y] = setup_dlg->cursor_x;
			gmu_core_config_acquire_lock();
			cfg_add_key(
				setup_dlg->config,
				setup_dlg->keys[setup_dlg->cursor_y][0],
				setup_dlg->keys[setup_dlg->cursor_y][setup_dlg->cursor_x+1]
			);
			gmu_core_config_release_lock();
			update = 1;
			break;
	}
	return update;
}

void setup_init(SetupDialog *setup_dlg, Skin *skin)
{
	int i, c;
	char *k;
	setup_dlg->key_count = 0;
	setup_dlg->skin = skin;
	setup_dlg->config = gmu_core_get_config();
	setup_dlg->offset = 0;
	setup_dlg->cursor_x = 0;
	setup_dlg->cursor_y = 0;
	setup_dlg->visible_lines = 1;
	gmu_core_config_acquire_lock();
	for (i = 0, c = 0; (k = cfg_get_key(setup_dlg->config, i)); i++) {
		int j;
		char **presets = cfg_key_get_presets(setup_dlg->config, k);
		setup_dlg->selected_value[c] = -1; /* Default to no selection */
		if (presets) {
			int len = strlen(k);
			for (j = 0; j < MAX_PRESETS_PER_KEY; j++) {
				setup_dlg->keys[c][j] = NULL;
			}
			if (len > 0) setup_dlg->keys[c][0] = malloc(len+1);
			if (setup_dlg->keys[c][0]) {
				char *val = cfg_get_key_value(setup_dlg->config, k);
				strncpy(setup_dlg->keys[c][0], k, len+1);
				for (j = 0; presets[j] && j < MAX_PRESETS_PER_KEY-1; j++) {
					len = strlen(presets[j]);
					if (len > 0) {
						if (val && strcmp(val, presets[j]) == 0)
							setup_dlg->selected_value[c] = j;
						setup_dlg->keys[c][j+1] = malloc(len+1);
						if (setup_dlg->keys[c][j+1])
							strncpy(setup_dlg->keys[c][j+1], presets[j], len+1);
					}
				}
				setup_dlg->key_count++;
			}
			c++;
		}
	}
	gmu_core_config_release_lock();
}

/**
 * Draws a single key with its preset options. active_item is the
 * currently active preset value (if any), cursor_item is the item
 * where the cursor points to.
 */
static void draw_item(SetupDialog *setup_dlg, SDL_Surface *sdl_target,
                      char **key, int active_item, int cursor_item,
                      int row)
{
	int  i, size = 255;
	char str[256];

	memset(str, ' ', 32);
	str[32] = '\0';
	if (key[0]) {
		i = strlen(key[0]);
		if (i > 31) i = 31;
		strncpy(str, key[0], i);

		for (i = 1; key[i] != NULL; i++) {
			int len = strlen(key[i]);
			if (cursor_item >= 0 && cursor_item+1 == i && size > 0) {
				strncat(str, "**", size);
				size-=2;
			}
			if (active_item >= 0 && active_item+1 == i && size > 0) {
				strncat(str, "<", size);
				size--;
			}
			if (len > 0 && size >= len) {
				strncat(str, key[i], size);
				size -= len;
			}
			if (active_item >= 0 && active_item+1 == i && size > 0) {
				strncat(str, ">", size);
				size--;
			}
			if (cursor_item >= 0 && cursor_item+1 == i && size > 0) {
				strncat(str, "**", size);
				size -= 2;
			}
			if (size > 0) {
				strncat(str, " ", size);
				size--;
			}
		}
	}
	textrenderer_draw_string_with_highlight(
		&setup_dlg->skin->font1,
		&setup_dlg->skin->font2,
		str,
		0,
		sdl_target,
		gmu_widget_get_pos_x((GmuWidget *)&setup_dlg->skin->lv, 1) + 2,
		gmu_widget_get_pos_y((GmuWidget *)&setup_dlg->skin->lv, 1) + 1 +
		row * (setup_dlg->skin->font1_char_height + 2),
		skin_textarea_get_characters_per_line(setup_dlg->skin),
		RENDER_ARROW
	);
}

void setup_draw(SetupDialog *setup_dlg, SDL_Surface *sdl_target)
{
	int i, y, h;
	skin_draw_header_text(setup_dlg->skin, "Gmu Setup", sdl_target);
	h = gmu_widget_get_height((GmuWidget *)&setup_dlg->skin->lv, 1);
	setup_dlg->visible_lines = h / (setup_dlg->skin->font1_char_height + 2);
	for (i = setup_dlg->offset, y = 0;
	     y < setup_dlg->visible_lines && i < setup_dlg->key_count && setup_dlg->keys[i];
	     i++, y++) {
		draw_item(
			setup_dlg,
			sdl_target,
			setup_dlg->keys[i],
			setup_dlg->selected_value[i],
			setup_dlg->cursor_y == i ? setup_dlg->cursor_x : -1,
			y
		);
	}
}

void setup_cursor_move(SetupDialog *setup_dlg, int x_offset, int y_offset)
{
	int i, max_x = 0;
	setup_dlg->cursor_y += y_offset;
	if (setup_dlg->cursor_y < 0) setup_dlg->cursor_y = 0;
	if (setup_dlg->cursor_y >= setup_dlg->key_count)
		setup_dlg->cursor_y = setup_dlg->key_count - 1;
	if (y_offset) setup_dlg->cursor_x = 0;
	if (x_offset > 0) {
		for (i = 1; setup_dlg->keys[setup_dlg->cursor_y][i]; i++);
		max_x = i - 1;
		if (setup_dlg->cursor_x + x_offset > max_x - 1) {
			x_offset = 0;
			setup_dlg->cursor_x = max_x - 1;
		}
	}
	setup_dlg->cursor_x += x_offset;
	if (setup_dlg->cursor_x < 0) setup_dlg->cursor_x = 0;
	if (setup_dlg->cursor_y > setup_dlg->visible_lines + setup_dlg->offset - 1)
		setup_dlg->offset = setup_dlg->cursor_y - setup_dlg->visible_lines + 1;
	else if (setup_dlg->cursor_y < setup_dlg->offset)
		setup_dlg->offset = setup_dlg->cursor_y;
}

void setup_shutdown(SetupDialog *setup_dlg)
{
	int i, j;
	for (i = 0; i < setup_dlg->key_count; i++)
		for (j = 0; j < MAX_PRESETS_PER_KEY; j++)
			if (setup_dlg->keys[i][j])
				free(setup_dlg->keys[i][j]);
}
