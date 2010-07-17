/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: filebrowser.c  Created: 061011
 *
 * Description: The Gmu file browser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <unistd.h>
#include <string.h>
#include "dir.h"
#include "textrenderer.h"
#include "filebrowser.h"
#include "skin.h"

void file_browser_init(FileBrowser *fb, const Skin *skin, Charset charset)
{
	fb->skin = skin;
	fb->horiz_offset = 0;
	fb->offset = 0;
	fb->selection = 0;
	fb->charset = charset;
	fb->directories_first = 0;
	fb->longest_line_so_far = 0;
}

void file_browser_set_directories_first(FileBrowser *fb, int value)
{
	fb->directories_first = value;
}

int file_browser_set_selection(FileBrowser *fb, int selection)
{
	fb->selection = selection;
	return 1;
}

int file_browser_get_selection(FileBrowser *fb)
{
	return fb->selection;
}

void file_browser_move_selection_down(FileBrowser *fb)
{
	if (fb->selection < dir_get_number_of_files(&fb->dir) - 1) {
		fb->selection++;
	} else {
		fb->selection = 0;
		fb->offset = 0;
	}
	if (fb->selection > fb->offset + skin_textarea_get_number_of_lines((Skin *)fb->skin) - 1)
		fb->offset++;
}

void file_browser_move_selection_up(FileBrowser *fb)
{
	if (fb->selection > 0) {
		fb->selection--;
	} else {
		int number_of_visible_lines = skin_textarea_get_number_of_lines((Skin *)fb->skin);
		fb->selection = dir_get_number_of_files(&fb->dir) - 1 >= 0 ? 
		                dir_get_number_of_files(&fb->dir) - 1 : 0;
		fb->offset = fb->selection - number_of_visible_lines + 1 > 0 ?
		             fb->selection - number_of_visible_lines + 1 : 0;
	}
	if (fb->selection < fb->offset) fb->offset--;
}

void file_browser_move_selection_n_items_down(FileBrowser *fb, int n)
{
	int i;
	for (i = n; i--; ) file_browser_move_selection_down(fb);
}

void file_browser_move_selection_n_items_up(FileBrowser *fb, int n)
{
	int i;
	for (i = n; i--; ) file_browser_move_selection_up(fb);
}

char *file_browser_get_selected_file(FileBrowser *fb)
{
	return dir_get_filename(&fb->dir, fb->selection);
}

int file_browser_change_dir(FileBrowser *fb, char *new_dir)
{
	char buf[128];
	int  result = 0;

	strncpy(buf, new_dir, 127);
	if (chdir(buf) == 0) {
		dir_free(&fb->dir);
		dir_read(&fb->dir, ".", fb->directories_first);
		fb->selection = 0;
		fb->offset = 0;
		fb->horiz_offset = 0;
		result = 1;
	}
	return result;
}

int file_browser_selection_is_dir(FileBrowser *fb)
{
	return (dir_get_flag(&fb->dir, fb->selection) == DIRECTORY ? 1 : 0);
}

void file_browser_draw(FileBrowser *fb, SDL_Surface *sdl_target)
{
	int       cpl = skin_textarea_get_characters_per_line((Skin *)fb->skin);
	int       i, pl, len = (cpl > 127 ? 127 : cpl);
	const int chars_left = len - 15;
	char      buf[128], *buf2 = alloca(chars_left+1), *path = dir_get_path(&fb->dir);
	char     *bufptr, buf3[128];
	int       number_of_visible_lines = skin_textarea_get_number_of_lines((Skin *)fb->skin);
	int       selected_entry_drawn = 0;

	pl = strlen(dir_get_path(&fb->dir));
	if (pl > 127) pl = 127;

	/* We have chars_left characters for path display available) */
	i = (pl > chars_left ? pl - chars_left : 0);
	memcpy(buf2, path+i, pl-i);
	buf2[pl-i] = '\0';
	if (pl > chars_left) memcpy(buf2, "...", 3);

	snprintf(buf, 63, "File browser (%s)", buf2);
	skin_draw_header_text((Skin *)fb->skin, buf, sdl_target);

	fb->longest_line_so_far = 0;

	for (i = fb->offset; 
	     i < fb->offset + number_of_visible_lines && i < dir_get_number_of_files(&fb->dir); 
	     i++) {
	    int  line_length = 0;
	    LCD *font, *font_inverted;

		if (dir_get_flag(&fb->dir, i) == DIRECTORY) {
			snprintf(buf, len, "[DIR]");
		} else {
			char fsbuf[32];
			dir_get_human_readable_filesize(&fb->dir, i, fsbuf, 32);
			snprintf(buf, len, " %4s", fsbuf);
		}

		if (i == fb->offset + number_of_visible_lines - 1 && !selected_entry_drawn)
			fb->selection = i;

		font          = (LCD *)(i == fb->selection ? &fb->skin->font2 : &fb->skin->font1);
		font_inverted = (LCD *)(i == fb->selection ? &fb->skin->font1 : &fb->skin->font2);
		lcd_draw_string(font, buf, sdl_target, 
						gmu_widget_get_pos_x((GmuWidget *)&fb->skin->lv, 1), 
						gmu_widget_get_pos_y((GmuWidget *)&fb->skin->lv, 1) + 1
						+ (i-fb->offset)*(fb->skin->font2_char_height+1));

		snprintf(buf, 127, "%s", dir_get_filename(&fb->dir, i));
		if (fb->charset == UTF_8) {
			charset_utf8_to_iso8859_1(buf3, buf, 127);
			bufptr = buf3;
		} else {
			bufptr = buf;
		}

		line_length = strlen(bufptr);
		if (line_length > fb->longest_line_so_far)
			fb->longest_line_so_far = line_length;

		lcd_draw_string_with_highlight(font, font_inverted, bufptr, fb->horiz_offset, sdl_target, 
									   gmu_widget_get_pos_x((GmuWidget *)&fb->skin->lv, 1)
									   + fb->skin->font1_char_width * 7,
									   gmu_widget_get_pos_y((GmuWidget *)&fb->skin->lv, 1)+ 1
									   + (i-fb->offset) * (fb->skin->font2_char_height + 1),
									   cpl-6, RENDER_ARROW);
		if (i == fb->selection) selected_entry_drawn = 1;
	}
}

void file_browser_scroll_horiz(FileBrowser *fb, int direction)
{
	int cpl = skin_textarea_get_characters_per_line((Skin *)fb->skin);
	int tmp = (fb->horiz_offset + direction < fb->longest_line_so_far - cpl + 7 ? 
	           fb->horiz_offset + direction : fb->longest_line_so_far - cpl + 7);
	fb->horiz_offset = (tmp > 0 ? tmp : 0);
}

Charset file_browser_get_filenames_charset(FileBrowser *fb)
{
	return fb->charset;
}
