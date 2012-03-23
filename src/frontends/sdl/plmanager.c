/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: plmanager.c  Created: 061223
 *
 * Description: Playlist save dialog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "plmanager.h"
#include "skin.h"

int plmanager_get_flag(PlaylistManager *ps)
{
	return ps->flag;
}

void plmanager_reset_flag(PlaylistManager *ps)
{
	ps->flag = 0;
}

int plmanager_process_action(PlaylistManager *ps, View *view, int user_key_action)
{
	int update = 0;
	switch (user_key_action) {
		case PLMANAGER_SELECT:
			ps->flag = PLMANAGER_SAVE_LIST;
			*view = PLAYLIST;
			update = 1;
			break;
		case PLMANAGER_CANCEL:
			*view = PLAYLIST;
			update = 1;
			break;
		case PLMANAGER_LOAD:
			ps->flag = PLMANAGER_LOAD_LIST;
			*view = PLAYLIST;
			update = 1;
			break;
		case PLMANAGER_LOAD_APPEND:
			ps->flag = PLMANAGER_APPEND_LIST;
			*view = PLAYLIST;
			update = 1;
			break;
		case MOVE_CURSOR_DOWN:
			plmanager_move_selection_down(ps);
			update = 1;
			break;
		case MOVE_CURSOR_UP:
			plmanager_move_selection_up(ps);
			update = 1;
			break;
	}
	return update;
}

void plmanager_draw(PlaylistManager *ps, SDL_Surface *sdl_target)
{
	int i;
	int posx = gmu_widget_get_pos_x((GmuWidget *)&ps->skin->lv, 1);
	int posy = gmu_widget_get_pos_y((GmuWidget *)&ps->skin->lv, 1);

	skin_draw_header_text((Skin *)ps->skin, "Save playlist as... / Load playlist", sdl_target);

	for (i = 0; i < PLMANAGER_MAX_ITEMS && ps->filenames[i][0] != '\0'; i++) {
		if (i == ps->selection)
			textrenderer_draw_string(&ps->skin->font2, ps->filenames[i], sdl_target, 
			                         posx+1, 
			                         posy+1+i*(ps->skin->font2_char_height+1));
		else
			textrenderer_draw_string(&ps->skin->font1, ps->filenames[i], sdl_target, 
			                         posx+1, 
			                         posy+1+i*(ps->skin->font2_char_height+1));
	}
}

void plmanager_init(PlaylistManager *ps, char *filenames_list, Skin *skin)
{
	int i, j, k;
	for (i = 0, j = 0, k = 0; filenames_list[i] != '\0' && j < PLMANAGER_MAX_ITEMS-1; i++) {
		if (filenames_list[i] != ';') {
			ps->filenames[j][k] = filenames_list[i];
			if (k < PLMANAGER_MAX_ITEM_SIZE-1) k++;
		} else {
			ps->filenames[j][k] = '\0';
			j++;
			k = 0;
		}
	}
	ps->filenames[j][k] = '\0';
	if (j < PLMANAGER_MAX_ITEMS) {
		ps->filenames[j+1][0] = '\0';
	} else {
		j = PLMANAGER_MAX_ITEMS - 1;
	}
	ps->items = j;
	ps->selection = 0;
	ps->skin = skin;
	ps->flag = PLMANAGER_NOTHING;
}

char *plmanager_get_selection(PlaylistManager *ps)
{
	return ps->filenames[ps->selection];
}

void plmanager_move_selection_down(PlaylistManager *ps)
{
	if (ps->selection < ps->items)
		ps->selection++;
	else
		ps->selection = 0;
}

void plmanager_move_selection_up(PlaylistManager *ps)
{
	if (ps->selection > 0)
		ps->selection--;
	else
		ps->selection = ps->items;
}
