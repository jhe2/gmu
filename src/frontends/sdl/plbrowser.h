/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: plbrowser.h  Created: 061025
 *
 * Description: The Gmu playlist browser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "charset.h"
#include "skin.h"

typedef struct PlaylistBrowser
{
	int         offset;
	int         horiz_offset;
	int         selection;
	Entry      *first_visible_entry;
	const Skin *skin;
	Charset     filenames_charset;
	int         longest_line_so_far;
} PlaylistBrowser;

void    pl_browser_init(PlaylistBrowser *pb, 
                        const Skin *skin, Charset filenames_charset);
void    pl_browser_draw(PlaylistBrowser *pb, SDL_Surface *sdl_target);
void    pl_browser_playlist_clear(PlaylistBrowser *pb);
int     pl_browser_are_selection_and_current_entry_equal(PlaylistBrowser *pb);
int     pl_browser_playlist_remove_selection(PlaylistBrowser *pb);
int     pl_browser_get_selection(PlaylistBrowser *pb);
int     pl_browser_set_selection(PlaylistBrowser *pb, int pos);
void    pl_brower_move_selection_down(PlaylistBrowser *pb);
void    pl_brower_move_selection_up(PlaylistBrowser *pb);
void    pl_brower_move_selection_n_items_down(PlaylistBrowser *pb, int n);
void    pl_brower_move_selection_n_items_up(PlaylistBrowser *pb, int n);
void    pl_browser_scroll_horiz(PlaylistBrowser *pb, int direction);
Charset pl_browser_get_filenames_charset(PlaylistBrowser *pb);
Entry  *pl_browser_get_selected_entry(PlaylistBrowser *pb);
