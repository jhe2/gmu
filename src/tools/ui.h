/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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

#include "window.h"
#include "listwidget.h"

typedef enum { FUNC_NONE, FUNC_HELP, FUNC_NEXT_WINDOW, FUNC_PLAY, FUNC_PAUSE, 
               FUNC_PLAY_PAUSE, FUNC_STOP, FUNC_NEXT, FUNC_PREVIOUS,
               FUNC_PLAYMODE, FUNC_PL_CLEAR, FUNC_PL_DEL_ITEM, FUNC_SEARCH,
               FUNC_FB_ADD, FUNC_TEXT_INPUT, FUNC_VOLUME_UP, FUNC_VOLUME_DOWN,
               FUNC_QUIT
} Function;

typedef struct FooterButtons {
	char    *button_name, *button_desc;
	Function func;
	wint_t   key;
	int      keycode;
} FooterButtons;

typedef struct UI {
	Window        *win_cmd, *win_ti, *win_lib, *win_header, *win_footer;
	ListWidget    *lw_pl, *lw_fb;
	FooterButtons *fb_pl, *fb_fb, *fb_ti, *fb_cmd;
	FooterButtons *fb_visible;
	int            rows, cols;
	WindowType     active_win;
	WINDOW        *rootwin;
	int            text_input_enabled;
	int            color;
} UI;

void ui_init(UI *ui, int color);
void ui_draw_header(UI *ui, char *cur_artist, char *cur_title, char *cur_status, int cur_time, int playmode, int volume);
void ui_refresh_active_window(UI *ui);
void ui_draw_footer(UI *ui);
void ui_draw(UI *ui);
void ui_update_trackinfo(UI *ui, char *title, char *artist, char *album, char *date);
void ui_cursor_text_input(UI *ui, char *str);
void ui_enable_text_input(UI *ui, int enable);
void ui_resize(UI *ui);
void ui_set_footer_buttons(UI *ui);
void ui_active_win_next(UI *ui);
void ui_free(UI *ui);
int  ui_has_color(UI *ui);
#endif
