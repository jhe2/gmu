/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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

void file_browser_init(FileBrowser *fb, const Skin *skin, Charset charset, char *base_dir)
{
	fb->skin = skin;
	fb->horiz_offset = 0;
	fb->offset = 0;
	fb->selection = 0;
	fb->charset = charset;
	fb->directories_first = 0;
	fb->longest_line_so_far = 0;
	fb->select_next_after_add = 0;
	dir_init(&(fb->dir));
	dir_set_base_dir(&(fb->dir), base_dir);
}

void file_browser_free(FileBrowser *fb)
{
	dir_free(&(fb->dir));
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

char *file_browser_get_selected_file_full_path_alloc(FileBrowser *fb)
{
	return dir_get_filename_with_full_path_alloc(&fb->dir, fb->selection);
}

static int internal_change_dir(FileBrowser *fb, char *new_dir)
{
	int   result = 0;
	char *ndir = NULL;

	if (new_dir) {
		int len = strlen(new_dir);
		if (len > 0) {
			ndir = malloc(len+1);
			if (ndir) {
				memcpy(ndir, new_dir, len+1);
				dir_free(&fb->dir);
				result = dir_read(&fb->dir, ndir, fb->directories_first);
				fb->selection = 0;
				fb->offset = 0;
				fb->horiz_offset = 0;
				free(ndir);
			}
		}
	}
	return result;
}

int file_browser_change_dir(FileBrowser *fb, char *new_dir)
{
	int result = internal_change_dir(fb, new_dir);
	if (!result) result = internal_change_dir(fb, "..");
	if (!result) result = internal_change_dir(fb, dir_get_base_dir(&(fb->dir)));
	return result;
}

int file_browser_selection_is_dir(FileBrowser *fb)
{
	return (dir_get_flag(&fb->dir, fb->selection) == DIRECTORY ? 1 : 0);
}

#define FB_MAXIMUM_STR_LENGTH 255

void file_browser_draw(FileBrowser *fb, SDL_Surface *sdl_target)
{
	int       cpl = skin_textarea_get_characters_per_line((Skin *)fb->skin);
	int       i, pl, len = (cpl > FB_MAXIMUM_STR_LENGTH ? FB_MAXIMUM_STR_LENGTH : cpl);
	const int chars_left = len - 15;
	char      buf[FB_MAXIMUM_STR_LENGTH+1], *buf2 = alloca(chars_left+1), *path = dir_get_path(&fb->dir);
	char     *bufptr, buf3[FB_MAXIMUM_STR_LENGTH];
	int       number_of_visible_lines = skin_textarea_get_number_of_lines((Skin *)fb->skin);
	int       selected_entry_drawn = 0;

	pl = path ? strlen(path) : 0;
	if (pl > FB_MAXIMUM_STR_LENGTH) pl = FB_MAXIMUM_STR_LENGTH;

	buf2[0] = '\0';
	
	if (pl > 0) {
		/* We have chars_left characters for path display available) */
		i = (pl > chars_left ? pl - chars_left : 0);
		memcpy(buf2, path+i, pl-i);
		buf2[pl-i] = '\0';
		if (pl > chars_left) memcpy(buf2, "...", 3);
	}

	snprintf(buf, FB_MAXIMUM_STR_LENGTH, "File browser (%s)", buf2);
	skin_draw_header_text((Skin *)fb->skin, buf, sdl_target);

	fb->longest_line_so_far = 0;

	for (i = fb->offset; 
	     i < fb->offset + number_of_visible_lines && i < dir_get_number_of_files(&fb->dir); 
	     i++) {
	    int           line_length = 0;
	    TextRenderer *font, *font_inverted;

		if (dir_get_flag(&fb->dir, i) == DIRECTORY) {
			snprintf(buf, len, "[DIR]");
		} else {
			char fsbuf[32];
			dir_get_human_readable_filesize(&fb->dir, i, fsbuf, 32);
			snprintf(buf, len, " %4s", fsbuf);
		}

		if (i == fb->offset + number_of_visible_lines - 1 && !selected_entry_drawn)
			fb->selection = i;

		font          = (TextRenderer *)(i == fb->selection ? &fb->skin->font2 : &fb->skin->font1);
		font_inverted = (TextRenderer *)(i == fb->selection ? &fb->skin->font1 : &fb->skin->font2);
		textrenderer_draw_string(font, buf, sdl_target, 
		                         gmu_widget_get_pos_x((GmuWidget *)&fb->skin->lv, 1), 
		                         gmu_widget_get_pos_y((GmuWidget *)&fb->skin->lv, 1) + 1
		                         + (i-fb->offset)*(fb->skin->font2_char_height+1));

		snprintf(buf, FB_MAXIMUM_STR_LENGTH, "%s", dir_get_filename(&fb->dir, i));
		if (fb->charset != UTF_8) {
			charset_iso8859_1_to_utf8(buf3, buf, FB_MAXIMUM_STR_LENGTH);
			bufptr = buf3;
		} else {
			bufptr = buf;
		}

		line_length = strlen(bufptr);
		if (line_length > fb->longest_line_so_far)
			fb->longest_line_so_far = line_length;

		textrenderer_draw_string_with_highlight(font, font_inverted, bufptr, fb->horiz_offset, sdl_target, 
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

int file_browser_is_select_next_after_add(FileBrowser *fb)
{
	return fb->select_next_after_add;
}

void file_browser_select_next_after_add(FileBrowser *fb, int select)
{
	fb->select_next_after_add = select;
}
