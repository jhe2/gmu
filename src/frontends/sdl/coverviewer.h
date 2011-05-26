/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: coverviewer.h  Created: 061030
 *
 * Description: Cover and track info viewer 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "SDL.h"
#include "textrenderer.h"
#include "skin.h"
#include "trackinfo.h"
#include "textbrowser.h"
#include "coverimg.h"

#ifndef _COVERVIEWER_H
#define _COVERVIEWER_H

#define SIZE_TRACKINFO_TEXT 16384

enum { EMBEDDED_COVER_NO, EMBEDDED_COVER_FIRST, EMBEDDED_COVER_LAST }; 

typedef enum _CoverAlign { ALIGN_LEFT, ALIGN_RIGHT } CoverAlign;

typedef struct CoverViewer {
	const Skin  *skin;
	TextBrowser  tb;
	int          large;
	CoverAlign   small_cover_align;
	int          try_to_load_embedded_cover;
	int          y_offset;
	int          hide_cover, hide_text;
	char         track_info_text[SIZE_TRACKINFO_TEXT];
	CoverImage   ci;
	int          spectrum_analyzer;
} CoverViewer;

void cover_viewer_init(CoverViewer *cv, const Skin *skin, int large, 
                       CoverAlign align, int embedded_cover);
void cover_viewer_free(CoverViewer *cv);
void cover_viewer_load_artwork(CoverViewer *cv, TrackInfo *ti, char *audio_file, 
                               char *image_file_pattern, int *ready_flag);
void cover_viewer_update_data(CoverViewer *cv, TrackInfo *ti);
void cover_viewer_show(CoverViewer *cv, SDL_Surface *target, TrackInfo *ti);
void cover_viewer_scroll_down(CoverViewer *cv);
void cover_viewer_scroll_up(CoverViewer *cv);
void cover_viewer_scroll_left(CoverViewer *cv);
void cover_viewer_scroll_right(CoverViewer *cv);
int  cover_viewer_cycle_cover_and_spectrum_visibility(CoverViewer *cv);
int  cover_viewer_toggle_text_visible(CoverViewer *cv);
int  cover_viewer_is_spectrum_analyzer_enabled(CoverViewer *cv);
void cover_viewer_enable_spectrum_analyzer(CoverViewer *cv);
void cover_viewer_disable_spectrum_analyzer(CoverViewer *cv);
#endif
