/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: textbrowser.h  Created: 061104
 *
 * Description: A simple text browser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "skin.h"

#ifndef _TEXTBROWSER_H
#define _TEXTBROWSER_H
typedef struct _TextBrowser {
	const char *text, *title;
	int         text_length;
	int         offset_x;
	int         pos_x, pos_y, chars_per_line;
	int         char_offset;
	int         end_reached;
	int         longest_line_so_far;
	const Skin *skin;
} TextBrowser;

void text_browser_init(TextBrowser *tb, const Skin *skin);
int  text_browser_set_text(TextBrowser *tb, const char *text, const char *title);
void text_browser_set_pos_x(TextBrowser *tb, int x);
void text_browser_set_chars_per_line(TextBrowser *tb, int chars);
void text_browser_draw(TextBrowser *tb, SDL_Surface *sdl_target);
void text_browser_scroll_left(TextBrowser *tb);
void text_browser_scroll_right(TextBrowser *tb);
void text_browser_scroll_down(TextBrowser *tb);
void text_browser_scroll_up(TextBrowser *tb);
#endif
