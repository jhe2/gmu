/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: filebrowser.h  Created: 061011
 *
 * Description: The Gmu file browser
 */
#include "dir.h"
#include "charset.h"
#include "skin.h"

#ifndef _FILEBROWSER_H
#define _FILEBROWSER_H
typedef struct FileBrowser
{
	int         offset;
	int         horiz_offset;
	int         selection;
	Dir         dir;
	const Skin *skin;
	Charset     charset;
	int         directories_first;
	int         longest_line_so_far;
	int         select_next_after_add;
} FileBrowser;

void    file_browser_init(FileBrowser *fb, const Skin *skin, Charset charset, char *base_dir);
void    file_browser_free(FileBrowser *fb);
int     file_browser_set_selection(FileBrowser *fb, int selection);
int     file_browser_get_selection(FileBrowser *fb);
void    file_browser_move_selection_down(FileBrowser *fb);
void    file_browser_move_selection_up(FileBrowser *fb);
void    file_browser_move_selection_n_items_down(FileBrowser *fb, int n);
void    file_browser_move_selection_n_items_up(FileBrowser *fb, int n);
char   *file_browser_get_selected_file(FileBrowser *fb);
char   *file_browser_get_selected_file_full_path_alloc(FileBrowser *fb);
int     file_browser_change_dir(FileBrowser *fb, char *new_dir);
int     file_browser_selection_is_dir(FileBrowser *fb);
void    file_browser_draw(FileBrowser *fb, SDL_Surface *sdl_target);
void    file_browser_scroll_horiz(FileBrowser *fb, int direction);
Charset file_browser_get_filenames_charset(FileBrowser *fb);
void    file_browser_set_directories_first(FileBrowser *fb, int value);
void    file_browser_select_next_after_add(FileBrowser *fb, int select);
int     file_browser_is_select_next_after_add(FileBrowser *fb);
#endif
