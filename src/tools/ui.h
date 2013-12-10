/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
 *
 * File: ui.h  Created: 121209
 *
 * Description: Gmu command line tool
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#ifndef WEJ_UI_H
#define WEJ_UI_H

#include <wchar.h>
#include "window.h"
#include "listwidget.h"

typedef enum { FUNC_NONE, FUNC_HELP, FUNC_NEXT_WINDOW, FUNC_PLAY, FUNC_PAUSE, 
               FUNC_PLAY_PAUSE, FUNC_STOP, FUNC_NEXT, FUNC_PREVIOUS,
               FUNC_PLAYMODE, FUNC_PL_CLEAR, FUNC_PL_DEL_ITEM, FUNC_SEARCH,
               FUNC_FB_ADD, FUNC_TEXT_INPUT, FUNC_VOLUME_UP, FUNC_VOLUME_DOWN,
               FUNC_QUIT, FUNC_MLIB_REFRESH
} Function;

typedef struct FooterButtons {
	char    *button_name, *button_desc;
	Function func;
	wint_t   key;
	int      keycode;
} FooterButtons;

typedef struct UI {
	Window        *win_cmd, *win_ti, *win_lib, *win_header, *win_footer;
	ListWidget    *lw_pl, *lw_fb, *lw_mlib_search;
	FooterButtons *fb_pl, *fb_fb, *fb_ti, *fb_lib, *fb_cmd;
	FooterButtons *fb_visible;
	int            rows, cols;
	WindowType     active_win;
	WINDOW        *rootwin;
	int            text_input_enabled;
	int            color;
	/* Track information & playback status */
	char           ti_title[128], ti_artist[128], ti_album[128], ti_date[64];
	char           status[32];
	int            pb_time, playmode, volume;
} UI;

void ui_init(UI *ui, int color);
void ui_draw_header(UI *ui);
void ui_refresh_active_window(UI *ui);
void ui_draw_footer(UI *ui);
void ui_draw(UI *ui);
void ui_update_trackinfo(UI *ui, char *title, char *artist, char *album, char *date);
void ui_update_playmode(UI *ui, int playmode);
void ui_update_volume(UI *ui, int volume);
void ui_update_playback_time(UI *ui, int time);
void ui_update_playback_status(UI *ui, char *status);
void ui_draw_trackinfo(UI *ui);
void ui_cursor_text_input(UI *ui, char *str);
void ui_enable_text_input(UI *ui, int enable);
void ui_resize(UI *ui);
void ui_set_footer_buttons(UI *ui);
void ui_active_win_next(UI *ui);
void ui_active_win_set(UI *ui, WindowType aw);
void ui_free(UI *ui);
int  ui_has_color(UI *ui);
#endif
