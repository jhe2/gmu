/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: coverviewer.c  Created: 061030
 *
 * Description: Cover and track info viewer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <string.h>
#include <stdlib.h>
#include "SDL.h"
#include "util.h"
#include "trackinfo.h"
#include "textrenderer.h"
#include "skin.h"
#include "coverviewer.h"
#include "coverimg.h"
#include "util.h"
#include "debug.h"
#include "audio.h"

void cover_viewer_init(CoverViewer *cv, const Skin *skin, int large, CoverAlign align, int embedded_cover)
{
	cv->skin        = skin;
	cv->large       = large;
	cv->small_cover_align = align;
	cv->y_offset    = 0;
	cv->hide_cover  = 0;
	cv->hide_text   = 0;
	cv->spectrum_analyzer = 0;
	cv->try_to_load_embedded_cover = embedded_cover;
	text_browser_init(&cv->tb, skin);
	text_browser_set_text(&cv->tb, "", "Track info");
	cover_image_init(&cv->ci);
}

void cover_viewer_free(CoverViewer *cv)
{
	cover_image_stop_thread(&cv->ci);
	cover_image_free(&cv->ci);
}

void cover_viewer_load_artwork(CoverViewer *cv, TrackInfo *ti, char *audio_file,
                               char *image_file_pattern, int *ready_flag)
{
	static char last_cover_path[256] = "";

	cv->y_offset = 0;
	if (!cv->large)
		cover_image_set_target_size(&cv->ci, gmu_widget_get_width((GmuWidget *)&cv->skin->lv, 1) / 2,
		                            gmu_widget_get_height((GmuWidget *)&cv->skin->lv, 1));
	else
		cover_image_set_target_size(&cv->ci, gmu_widget_get_width((GmuWidget *)&cv->skin->lv, 1), -1);

	/* Try to load image embedded in tag: */
	if (cv->try_to_load_embedded_cover == EMBEDDED_COVER_FIRST && ti->image.data != NULL) {
		cover_image_load_image_from_memory(&cv->ci, ti->image.data, ti->image.data_size, 
                                         ti->image.mime_type, ready_flag);
		last_cover_path[0] = '\0';
		ti->has_cover_artwork = 1;
	} else if (audio_file != NULL && strlen(audio_file) < 256) {
		char *fn = get_file_matching_given_pattern_alloc(audio_file, image_file_pattern);
		if (fn) {
			ti->has_cover_artwork = 1;
			if (strncmp(fn, last_cover_path, 255) != 0) {
				wdprintf(V_INFO, "coverviewer", "Loading %s\n", fn);
				cover_image_load_image_from_file(&cv->ci, fn, ready_flag);
				wdprintf(V_DEBUG, "coverviewer", "Ok. Loading in progress...\n");
				strncpy(last_cover_path, fn, 255);
				last_cover_path[255] = '\0';
			} else {
				wdprintf(V_INFO, "coverviewer", "Cover file already loaded. No need to reload the file.\n");
			}
			free(fn);
		} else {
			ti->has_cover_artwork = 0;
			if (cover_image_free_image(&cv->ci))
				last_cover_path[0] = '\0';
		}
	} else if (cv->try_to_load_embedded_cover == EMBEDDED_COVER_LAST && ti->image.data != NULL) {
		cover_image_load_image_from_memory(&cv->ci, ti->image.data, ti->image.data_size, 
		                                   ti->image.mime_type, ready_flag);
		last_cover_path[0] = '\0';	
	}
}

void cover_viewer_update_data(CoverViewer *cv, TrackInfo *ti)
{
	char *lyrics = "";

	if (strlen(trackinfo_get_lyrics(ti)) > 0) {
		int len = strlen(trackinfo_get_lyrics(ti)) + 42;
		lyrics = malloc(sizeof(char) * len);
		snprintf(lyrics, len, "\n\n**Lyrics/Additional information:**\n%s", trackinfo_get_lyrics(ti));
	}

	snprintf(cv->track_info_text, SIZE_TRACKINFO_TEXT-1,
	         "**Title:**\n%s\n**Artist:**\n%s\n**Album:**\n%s\n**Track number:**\n%s\n"
	         "**Date:**\n%s\n**Length:**\n%02d:%02d\n**Samplerate:**\n%d Hz\n"
	         "**Channels:**\n%d (%s)\n**Bitrate:**\n%ld kbit/s %s\n**File type:**\n%s\n"
	         "**File:**\n%s%s",
	         trackinfo_get_title(ti), trackinfo_get_artist(ti), trackinfo_get_album(ti),
	         trackinfo_get_tracknr(ti), trackinfo_get_date(ti), trackinfo_get_length_minutes(ti),
	         trackinfo_get_length_seconds(ti), trackinfo_get_samplerate(ti),
	         trackinfo_get_channels(ti), trackinfo_get_channels(ti) >= 2 ? "stereo" : "mono",
	         trackinfo_get_bitrate(ti) / 1000, trackinfo_is_vbr(ti) ? "(average)" : "",
	         trackinfo_get_file_type(ti), trackinfo_get_file_name(ti), lyrics);
	text_browser_set_text(&cv->tb, cv->track_info_text, "Track info");
	if (lyrics[0] != '\0') free(lyrics);
}

int cover_viewer_is_spectrum_analyzer_enabled(CoverViewer *cv)
{
	return cv->spectrum_analyzer;
}

void cover_viewer_enable_spectrum_analyzer(CoverViewer *cv)
{
	audio_spectrum_register_for_access();
	cv->spectrum_analyzer = 1;
}

void cover_viewer_disable_spectrum_analyzer(CoverViewer *cv)
{
	cv->spectrum_analyzer = 0;
	audio_spectrum_unregister();
}

void cover_viewer_show(CoverViewer *cv, SDL_Surface *target, TrackInfo *ti)
{
	SDL_Rect     srect, drect;
	int          text_x_offset = 5;
	int          chars_per_line = (skin_textarea_get_characters_per_line((Skin *)cv->skin) < 256 ? 
	                               skin_textarea_get_characters_per_line((Skin *)cv->skin) : 255);
	SDL_Surface *cover = cover_image_get_image(&cv->ci);
	int          ax = gmu_widget_get_pos_x((GmuWidget *)&cv->skin->lv, 1);
	int          ay = gmu_widget_get_pos_y((GmuWidget *)&cv->skin->lv, 1);
	int          aw = gmu_widget_get_width((GmuWidget *)&cv->skin->lv, 1);
	int          ah = gmu_widget_get_height((GmuWidget *)&cv->skin->lv, 1);

	if (cover != NULL && !cv->hide_cover && trackinfo_has_cover_artwork(ti)) {
		if (!cv->large) {
			if (cv->small_cover_align == ALIGN_LEFT) {
				text_browser_set_pos_x(&cv->tb, ax + aw / 2);
			} else {
				text_browser_set_pos_x(&cv->tb, ax);
			}
			if (!cv->hide_text) {
				chars_per_line = chars_per_line / 2 - 1;
				text_browser_set_chars_per_line(&cv->tb, chars_per_line);
				switch (cv->small_cover_align) {
					case ALIGN_LEFT:
						drect.x = ax;
						drect.y = ay + ah / 2 - cover->h / 2;
						text_x_offset = cover->w + 5;
						break;
					case ALIGN_RIGHT:
						drect.x = ax + aw - cover->w;
						drect.y = ay + ah / 2 - cover->h / 2;
						text_x_offset = 0;
						break;
				}
				text_browser_set_pos_x(&cv->tb, ax + text_x_offset);
				text_browser_draw(&cv->tb, target);
			} else {
				drect.x = ax + aw / 2 - cover->w / 2;
				drect.y = ay + ah / 2 - cover->h / 2;
			}
			if (cover->w <= aw && cover->h <= ah)
				SDL_BlitSurface(cover, NULL, target, &drect);
		} else {
			text_browser_set_pos_x(&cv->tb, ax);
			text_browser_set_chars_per_line(&cv->tb, chars_per_line);
			drect.x = ax;
			drect.y = ay;
			srect.x = 0;
			srect.y = cv->y_offset;
			srect.w = aw;
			srect.h = ah;
			text_x_offset = 5;
			SDL_BlitSurface(cover, &srect, target, &drect);
			if (!cv->hide_text)	text_browser_draw(&cv->tb, target);
		}
	} else {
		text_browser_set_pos_x(&cv->tb, ax);
		text_browser_set_chars_per_line(&cv->tb, chars_per_line);
		if (!cv->hide_text) text_browser_draw(&cv->tb, target);
	}
	
	/* Draw spectrum analyzer */
	if (cv->spectrum_analyzer) {
		Uint32   color = SDL_MapRGB(target->format, 0, 70, 255);
		int      i, barwidth = aw / 20;
		int16_t *amplitudes;
		static int16_t amplitudes_smoothed[8];
		SDL_Rect dstrect;

		dstrect.w = barwidth;
		dstrect.h = 20;
		dstrect.x = cv->hide_text ? ax + aw / 2 - aw / 4 : ax + aw / 2;
		if (audio_spectrum_read_lock()) {
			amplitudes = audio_spectrum_get_current_amplitudes();
			for (i = 0; i < 8; i++) {
				int16_t a = amplitudes[i] / 327 + 2;
				dstrect.x += barwidth+1;
				if (amplitudes_smoothed[i] < a) amplitudes_smoothed[i] = a;
				amplitudes_smoothed[i] = amplitudes_smoothed[i] > 70 ? 70 : amplitudes_smoothed[i];
				dstrect.h = amplitudes_smoothed[i];
				dstrect.y = ay + ah / 2 + 25 - amplitudes_smoothed[i];
				SDL_FillRect(target, &dstrect, color);
				amplitudes_smoothed[i] -= (15-i);
				if (amplitudes_smoothed[i] < 2) amplitudes_smoothed[i] = 2;
			}
			audio_spectrum_read_unlock();
		}
	}

	if (cv->hide_text && !cv->hide_cover &&  !cv->spectrum_analyzer)
		skin_draw_header_text((Skin *)cv->skin, "Track info (Cover only)", target);
	else if (cv->hide_cover && !cv->hide_text && !cv->spectrum_analyzer)
		skin_draw_header_text((Skin *)cv->skin, "Track info (Text only)", target);
	else if (cv->hide_cover && cv->hide_text && cv->spectrum_analyzer)
		skin_draw_header_text((Skin *)cv->skin, "Track info (Spectrum analyzer only)", target);
	else if (cv->hide_text && cv->hide_cover && !cv->spectrum_analyzer)
		skin_draw_header_text((Skin *)cv->skin, "Track info (showing nothing)", target);
	else if (cv->hide_text && !cv->hide_cover && cv->spectrum_analyzer)
		skin_draw_header_text((Skin *)cv->skin, "Track info (Cover + Spectrum analyzer)", target);
	else if (!cv->hide_text && cv->hide_cover && cv->spectrum_analyzer)
		skin_draw_header_text((Skin *)cv->skin, "Track info (Text + Spectrum analyzer)", target);
	else if (!cv->hide_text && !cv->hide_cover && !cv->spectrum_analyzer)
		skin_draw_header_text((Skin *)cv->skin, "Track info (Text + Cover)", target);
}

void cover_viewer_scroll_down(CoverViewer *cv)
{
	SDL_Surface *cover = cover_image_get_image(&cv->ci);
	if (cv->hide_text && cover != NULL) {
		if (cv->y_offset < cover->h - gmu_widget_get_height((GmuWidget *)&cv->skin->lv, 1) - 10)
			cv->y_offset += 10;
	} else if (!cv->hide_text) {
		text_browser_scroll_down(&cv->tb);
	}
}

void cover_viewer_scroll_up(CoverViewer *cv)
{
	if (cv->hide_text) {
		if (cv->y_offset > 0)
			cv->y_offset -= 10;
	} else if (!cv->hide_text) {
		text_browser_scroll_up(&cv->tb);
	}
}

void cover_viewer_scroll_left(CoverViewer *cv)
{
	if (!cv->hide_text)
		text_browser_scroll_left(&cv->tb);
}

void cover_viewer_scroll_right(CoverViewer *cv)
{
	if (!cv->hide_text)
		text_browser_scroll_right(&cv->tb);
}

int cover_viewer_cycle_cover_and_spectrum_visibility(CoverViewer *cv)
{
	if (cv->spectrum_analyzer && cv->hide_cover)
		cv->hide_cover = 0;
	else if (cv->spectrum_analyzer && !cv->hide_cover)
		cover_viewer_disable_spectrum_analyzer(cv);
	else if (!cv->spectrum_analyzer && !cv->hide_cover)
		cv->hide_cover = 1;
	else
		cover_viewer_enable_spectrum_analyzer(cv);
	return 0;
}

int cover_viewer_toggle_text_visible(CoverViewer *cv)
{
	return cv->hide_text = !cv->hide_text;
}
