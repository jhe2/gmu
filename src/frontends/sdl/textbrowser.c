/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: textbrowser.c  Created: 061104
 *
 * Description: A simple text browser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <string.h>
#include "textbrowser.h"
#include "SDL.h"
#include "skin.h"
#include "textrenderer.h"
#define MAX_LINE_LENGTH 256

int text_browser_set_text(TextBrowser *tb, const char *text, const char *title)
{
	tb->text = text;
	tb->text_length = strlen(tb->text);
	tb->title = title;
	tb->offset_x = 0;
	tb->char_offset = 0;
	tb->end_reached = 0;
	tb->longest_line_so_far = 0;
	return 0;
}

void text_browser_init(TextBrowser *tb, const Skin *skin)
{
	tb->skin = skin;
	tb->offset_x = 0;
	tb->char_offset = 0;
	tb->end_reached = 0;
	tb->pos_x = gmu_widget_get_pos_x((GmuWidget *)&tb->skin->lv, 1);
	tb->pos_y = gmu_widget_get_pos_y((GmuWidget *)&tb->skin->lv, 1);
	text_browser_set_chars_per_line(tb, skin_textarea_get_characters_per_line((Skin *)tb->skin));
	tb->longest_line_so_far = 0;
}

void text_browser_set_pos_x(TextBrowser *tb, int x)
{
	if (x >= 0) tb->pos_x = x;
}

void text_browser_set_chars_per_line(TextBrowser *tb, int chars)
{
	tb->chars_per_line = chars - 1 < MAX_LINE_LENGTH-1 ? chars - 1: MAX_LINE_LENGTH-1;
}

void text_browser_scroll_left(TextBrowser *tb)
{
	if (tb->offset_x > 0) tb->offset_x--;
}

void text_browser_scroll_right(TextBrowser *tb)
{
	if (tb->offset_x < tb->longest_line_so_far - tb->chars_per_line + 1) tb->offset_x++;
}

void text_browser_scroll_down(TextBrowser *tb)
{
	if (!tb->end_reached) {
		for (; tb->text[tb->char_offset]!='\n' && tb->text[tb->char_offset]!='\0'; tb->char_offset++);
		if (tb->text[tb->char_offset] == '\0') {
			for (; tb->text[tb->char_offset-1] != '\n' && tb->char_offset > 1; tb->char_offset--);
			tb->end_reached = 1;
		} else {
			tb->char_offset++;
		}
	}
}

void text_browser_scroll_up(TextBrowser *tb)
{
	if (tb->char_offset > 0) tb->char_offset--;
	for (; tb->text[tb->char_offset-1] != '\n' && tb->char_offset > 1; tb->char_offset--);
	if (tb->char_offset == 1) tb->char_offset--;
}

void text_browser_draw(TextBrowser *tb, SDL_Surface *sdl_target)
{
	int i, char_offset = tb->char_offset, yo = 1;
	int tlen = strlen(tb->text), indent = tb->skin->font1_char_width / 2;
	int nol = skin_textarea_get_number_of_lines((Skin *)tb->skin);

	text_browser_set_chars_per_line(tb, skin_textarea_get_characters_per_line((Skin *)tb->skin));
	tb->pos_x = gmu_widget_get_pos_x((GmuWidget *)&tb->skin->lv, 1);
	tb->pos_y = gmu_widget_get_pos_y((GmuWidget *)&tb->skin->lv, 1);

	skin_draw_header_text((Skin *)tb->skin, (char *)tb->title, sdl_target);
	for (i = 0; i < nol && char_offset < tlen; i++) {
		char  line[MAX_LINE_LENGTH], line_break_char = '\0';
		int   line_length = 0, red = 0;

		memset(line, '\0', MAX_LINE_LENGTH);
		while (tb->text[char_offset+line_length] != '\n' &&
		       tb->text[char_offset+line_length] != '\0') {
			line_length++;
			if (tb->text[char_offset+line_length] == '*' && tb->text[char_offset+line_length+1] == '*')
				red++;
		}
		/* If we do not have reached the end of the text yet, *
		 * check for an additional \r at each line ending:    */
		if (tb->text[char_offset+line_length] != '\0' && tb->text[char_offset+line_length+1] == '\r')
			line_length++;

		if (tb->text[char_offset+line_length] != '\0' && line_break_char == '\0')
			line_break_char = tb->text[char_offset+line_length];
		if (tb->text[char_offset+line_length] != line_break_char) line_length--;
		/*printf("lbc=%d\n", line_break_char);*/

		if (tb->text[char_offset+line_length] == '\0' || 
		    tb->text[char_offset+line_length+1] == '\0') /* if there is an additional \r after the \n */
			tb->end_reached = 1;
		else
			tb->end_reached = 0;
		line_length = (line_length > MAX_LINE_LENGTH-1 ? MAX_LINE_LENGTH-1 : line_length);
		tb->longest_line_so_far = (line_length - red > tb->longest_line_so_far ? 
		                           line_length - red : tb->longest_line_so_far);

		strncpy(line, tb->text+char_offset, line_length);
		line[line_length] = '\0';

		textrenderer_draw_string_with_highlight(&tb->skin->font1, &tb->skin->font2,
		                                        line, tb->offset_x, sdl_target,
		                                        tb->pos_x + indent, tb->pos_y + yo,
		                                        tb->chars_per_line-1, RENDER_ARROW);
		yo += tb->skin->font1_char_height+1;
		char_offset += line_length + 1;
	}
	if (tb->char_offset > 0)
		skin_draw_scroll_arrow_up((Skin *)tb->skin, sdl_target);
	if (!tb->end_reached)
		skin_draw_scroll_arrow_down((Skin *)tb->skin, sdl_target);
}
