/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: skin.c  Created: 061107
 *
 * Description: Gmu theme engine
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "skin.h"
#include "wejpconfig.h"
#include FILE_HW_H
#include "core.h"
#include "gmuwidget.h"
#include "debug.h"

static int skin_init_widget(char *skin_name, ConfigFile *skinconf, char *prefix, GmuWidget *w)
{
	int   tmp_x1 = 0, tmp_y1 = 0, tmp_x2 = 0, tmp_y2 = 0;
	char *tmp_img_prefix = NULL;
	char *val, tmp[256];
	int   result = 0;

	snprintf(tmp, 255, "%s.PosX1", prefix);
	val = cfg_get_key_value(*skinconf, tmp);
	if (val) tmp_x1 = atoi(val);
	snprintf(tmp, 255, "%s.PosY1", prefix);
	val = cfg_get_key_value(*skinconf, tmp);
	if (val) tmp_y1 = atoi(val);
	snprintf(tmp, 255, "%s.PosX2", prefix);
	val = cfg_get_key_value(*skinconf, tmp);
	if (val) tmp_x2 = atoi(val);
	snprintf(tmp, 255, "%s.PosY2", prefix);
	val = cfg_get_key_value(*skinconf, tmp);
	if (val) tmp_y2 = atoi(val);
	snprintf(tmp, 255, "%s.ImagePrefix", prefix);
	val = cfg_get_key_value(*skinconf, tmp);
	if (val) tmp_img_prefix = val;
	if (tmp_img_prefix) {
		snprintf(tmp, 255, "%s/themes/%s/%s", gmu_core_get_base_dir(), skin_name, tmp_img_prefix);
		gmu_widget_new(w, tmp, tmp_x1, tmp_y1, tmp_x2, tmp_y2);
		result = 1;
	}
	return result;
}

static int skin_config_load(Skin *skin, char *skin_name)
{
	int        result = 1;
	ConfigFile skinconf;
	char       skin_file[256];

	memset(skin, 0, sizeof(Skin));
	skin->version = 1;

	skin->buffer = NULL;

	skin->title_scroller_offset_x1 = -1;
	skin->title_scroller_offset_x2 = -1;
	skin->title_scroller_offset_y = -1;

	skin->symbols_width = 0;
	skin->symbols_height = 0;

	skin->symbol_play_offset_x = -1;
	skin->symbol_play_offset_y = -1;

	skin->symbol_pause_offset_x = -1;
	skin->symbol_pause_offset_y = -1;

	skin->symbol_stereo_offset_x = -1;
	skin->symbol_stereo_offset_y = -1;

	skin->volume_offset_x = -1;
	skin->volume_offset_y = -1;

	skin->bitrate_offset_x = -1;
	skin->bitrate_offset_y = -1;

	skin->frequency_offset_x = -1;
	skin->frequency_offset_y = -1;

	skin->time_offset_x = -1;
	skin->time_offset_y = -1;

	skin->display_symbols = NULL;
	skin->arrow_up = NULL;
	skin->arrow_down = NULL;

	snprintf(skin_file, 255, "%s/themes/%s/theme.conf", gmu_core_get_base_dir(), skin_name);
	cfg_init_config_file_struct(&skinconf);
	if (cfg_read_config_file(&skinconf, skin_file) != 0) {
		wdprintf(V_ERROR, "skin", "Could not read skin config \"%s\".\n", skin_file);
		result = 0;
	} else {
		char *val;

		val = cfg_get_key_value(skinconf, "FormatVersion");
		if (val) skin->version = atoi(val);

		strncpy(skin->name, skin_name, 127);

		switch (skin->version) {
			case 2: /* New theme format with support for a resizable window */
				wdprintf(V_INFO, "skin", "Modern theme file format found.\n");

				skin_init_widget(skin_name, &skinconf, "Display",  &(skin->display));
				skin_init_widget(skin_name, &skinconf, "ListView", &(skin->lv));
				skin_init_widget(skin_name, &skinconf, "Header",   &(skin->header));
				skin_init_widget(skin_name, &skinconf, "Footer",   &(skin->footer));

				val = cfg_get_key_value(skinconf, "Display.TitleScrollerOffsetX1");
				if (val) skin->title_scroller_offset_x1 = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.TitleScrollerOffsetX2");
				if (val) skin->title_scroller_offset_x2 = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.TitleScrollerOffsetY");
				if (val) skin->title_scroller_offset_y = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.BitrateOffsetX");
				if (val) skin->bitrate_offset_x = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.BitrateOffsetY");
				if (val) skin->bitrate_offset_y = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.TimeOffsetX");
				if (val) skin->time_offset_x = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.TimeOffsetY");
				if (val) skin->time_offset_y = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.FrequencyOffsetX");
				if (val) skin->frequency_offset_x = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.FrequencyOffsetY");
				if (val) skin->frequency_offset_y = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.SymbolsWidth");
				if (val) skin->symbols_width = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.SymbolsHeight");
				if (val) skin->symbols_height = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.Symbol.Play.OffsetX");
				if (val) skin->symbol_play_offset_x = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.Symbol.Play.OffsetY");
				if (val) skin->symbol_play_offset_y = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.Symbol.Pause.OffsetX");
				if (val) skin->symbol_pause_offset_x = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.Symbol.Pause.OffsetY");
				if (val) skin->symbol_pause_offset_y = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.Symbol.Stereo.OffsetX");
				if (val) skin->symbol_stereo_offset_x = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.Symbol.Stereo.OffsetY");
				if (val) skin->symbol_stereo_offset_y = atoi(val);
				/* fonts */
				val = cfg_get_key_value(skinconf, "Display.Font");
				if (val) strncpy(skin->font_display_name, val, 127);
				val = cfg_get_key_value(skinconf, "Display.FontCharWidth");
				if (val) skin->font_display_char_width = atoi(val);
				val = cfg_get_key_value(skinconf, "Display.FontCharHeight");
				if (val) skin->font_display_char_height = atoi(val);
				val = cfg_get_key_value(skinconf, "Font1");
				if (val) strncpy(skin->font1_name, val, 127);
				val = cfg_get_key_value(skinconf, "Font1CharWidth");
				if (val) skin->font1_char_width = atoi(val);
				val = cfg_get_key_value(skinconf, "Font1CharHeight");
				if (val) skin->font1_char_height = atoi(val);
				val = cfg_get_key_value(skinconf, "Font2");
				if (val) strncpy(skin->font2_name, val, 127);
				val = cfg_get_key_value(skinconf, "Font2CharWidth");
				if (val) skin->font2_char_width = atoi(val);
				val = cfg_get_key_value(skinconf, "Font2CharHeight");
				if (val) skin->font2_char_height = atoi(val);
				result = 1;

				/* load images (symbols, arrows) */
				{
					char         tmp[256];
					SDL_Surface *tmp_sf;
				
					val = cfg_get_key_value(skinconf, "Display.Symbols");
					if (val) {
						snprintf(tmp, 255, "%s/themes/%s/%s", gmu_core_get_base_dir(), skin->name, val);
						if ((tmp_sf = IMG_Load(tmp))) {
							skin->display_symbols = SDL_DisplayFormatAlpha(tmp_sf);
							SDL_FreeSurface(tmp_sf);
						}
					}
					val = cfg_get_key_value(skinconf, "Icon.ArrowUp");
					if (val) {
						snprintf(tmp, 255, "%s/themes/%s/%s", gmu_core_get_base_dir(), skin->name, val);
						if ((tmp_sf = IMG_Load(tmp))) {
							skin->arrow_up = SDL_DisplayFormatAlpha(tmp_sf);
							SDL_FreeSurface(tmp_sf);
						}
					}
					val = cfg_get_key_value(skinconf, "Icon.ArrowDown");
					if (val) {
						snprintf(tmp, 255, "%s/themes/%s/%s", gmu_core_get_base_dir(), skin->name, val);
						if ((tmp_sf = IMG_Load(tmp))) {
							skin->arrow_down = SDL_DisplayFormatAlpha(tmp_sf);
							SDL_FreeSurface(tmp_sf);
						}
					}
				}

				/* fonts */
				{
					int  a, b, c;
					char tmp[256];
					
					wdprintf(V_DEBUG, "skin", "Loading fonts...\n");
					snprintf(tmp, 255, "%s/themes/%s/%s", gmu_core_get_base_dir(), skin->name, skin->font_display_name);
					wdprintf(V_DEBUG, "skin", "Loading %s\n", tmp);
					a = textrenderer_init(&skin->font_display, tmp, 
					                      skin->font_display_char_width, skin->font_display_char_height);
					snprintf(tmp, 255, "%s/themes/%s/%s", gmu_core_get_base_dir(), skin->name, skin->font1_name);
					wdprintf(V_DEBUG, "skin", "Loading %s\n", tmp);
					b = textrenderer_init(&skin->font1, tmp, 
								          skin->font1_char_width, skin->font1_char_height);
					snprintf(tmp, 255, "%s/themes/%s/%s", gmu_core_get_base_dir(), skin->name, skin->font2_name);
					wdprintf(V_DEBUG, "skin", "Loading %s\n", tmp);
					c = textrenderer_init(&skin->font2, tmp,
					                      skin->font2_char_width, skin->font2_char_height);
					if (a && b && c)
						wdprintf(V_INFO, "skin", "skin: Skin data loaded successfully.\n");
					else
						result = 0;
				}
				break;
			default:
				wdprintf(V_ERROR, "skin", "Invalid file format version: %d.\n", skin->version);
				skin->version = 0;
				break;
		}
	}
	cfg_free_config_file_struct(&skinconf);
	return result;
}

int skin_init(Skin *skin, char *skin_name)
{
	int res = skin_config_load(skin, skin_name);
	if (!res && strncmp(skin_name, "default", 7) != 0) {
		wdprintf(V_INFO, "skin", "Trying to load default skin...\n");
		if (res) skin_free(skin);
		res = skin_init(skin, "default");
	}
	return res;
}

void skin_free(Skin *skin)
{
	if (skin->display_symbols) SDL_FreeSurface(skin->display_symbols);
	if (skin->arrow_up) SDL_FreeSurface(skin->arrow_up);
	if (skin->arrow_down) SDL_FreeSurface(skin->arrow_down);
	textrenderer_free(&skin->font1);
	textrenderer_free(&skin->font2);
	textrenderer_free(&skin->font_display);
	gmu_widget_free(&(skin->display));
	gmu_widget_free(&(skin->lv));
	gmu_widget_free(&(skin->header));
	gmu_widget_free(&(skin->footer));
	SDL_FreeSurface(skin->buffer);
}

/* target is used to determine the necessary size and color depth for the offscreen */
static int skin_init_offscreen(Skin *skin, SDL_Surface *target)
{
	int initialized = 0;

	if (!skin->buffer) { /* new surface */
		SDL_Surface *tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, target->w, target->h, HW_COLOR_DEPTH, 0, 0, 0, 0);
		if (tmp) skin->buffer = SDL_DisplayFormat(tmp); else skin->buffer = NULL;
		SDL_FreeSurface(tmp);
		initialized = 1;
	} else if (skin->buffer->w != target->w || skin->buffer->h != target->h) { /* reinit surface */
		SDL_Surface *tmp = SDL_CreateRGBSurface(SDL_SWSURFACE, target->w, target->h, HW_COLOR_DEPTH, 0, 0, 0, 0);
		SDL_FreeSurface(skin->buffer);
		if (tmp) skin->buffer = SDL_DisplayFormat(tmp); else skin->buffer = NULL;
		SDL_FreeSurface(tmp);
		initialized = 1;
	}
	return initialized;
}

static void skin_draw_widget(Skin *skin, GmuWidget *gw, SDL_Surface *buffer)
{
	SDL_Rect srect, drect;

	/* if necessary, draw the widget to the offscreen */
	if (skin_init_offscreen(skin, buffer)) {
		gmu_widget_draw(&skin->display, skin->buffer);
		gmu_widget_draw(&skin->header, skin->buffer);
		gmu_widget_draw(&skin->lv, skin->buffer);
		gmu_widget_draw(&skin->footer, skin->buffer);
	}
	srect.x = gmu_widget_get_pos_x(gw, 0);
	srect.y = gmu_widget_get_pos_y(gw, 0);
	srect.w = gmu_widget_get_width(gw, 0);
	srect.h = gmu_widget_get_height(gw, 0);
	drect.x = srect.x;
	drect.y = srect.y;
	SDL_BlitSurface(skin->buffer, &srect, buffer, &drect);
}

static void skin_update_widget(Skin *skin, GmuWidget *gw, SDL_Surface *display, SDL_Surface *buffer)
{
	SDL_Rect srect, drect;

	srect.x = gmu_widget_get_pos_x(gw, 0);
	srect.y = gmu_widget_get_pos_y(gw, 0);
	srect.w = gmu_widget_get_width(gw, 0);
	srect.h = gmu_widget_get_height(gw, 0);
	drect.x = srect.x;
	drect.y = srect.y;
	SDL_BlitSurface(buffer, &srect, display, &drect);
	SDL_UpdateRects(display, 1, &drect);
}

void skin_update_display(Skin *skin, SDL_Surface *display, SDL_Surface *buffer)
{
	skin_update_widget(skin, &skin->display, display, buffer);
}

void skin_draw_display_bg(Skin *skin, SDL_Surface *buffer)
{
	skin_draw_widget(skin, &skin->display, buffer);
}

void skin_update_header(Skin *skin, SDL_Surface *display, SDL_Surface *buffer)
{
	skin_update_widget(skin, &skin->header, display, buffer);
}

void skin_draw_header_bg(Skin *skin, SDL_Surface *buffer)
{
	skin_draw_widget(skin, &skin->header, buffer);
}

void skin_update_textarea(Skin *skin, SDL_Surface *display, SDL_Surface *buffer)
{
	skin_update_widget(skin, &skin->lv, display, buffer);
}

void skin_draw_textarea_bg(Skin *skin, SDL_Surface *buffer)
{
	skin_draw_widget(skin, &skin->lv, buffer);
}

void skin_update_footer(Skin *skin, SDL_Surface *display, SDL_Surface *buffer)
{
	skin_update_widget(skin, &skin->footer, display, buffer);
}

void skin_draw_footer_bg(Skin *skin, SDL_Surface *buffer)
{
	skin_draw_widget(skin, &skin->footer, buffer);
}

void skin_update_bg(Skin *skin, SDL_Surface *display, SDL_Surface *buffer)
{
	SDL_BlitSurface(buffer, NULL, display, NULL);
	SDL_UpdateRect(display, 0, 0, 0, 0);
}

int skin_textarea_get_number_of_lines(Skin *skin)
{
	return gmu_widget_get_height((GmuWidget *)&skin->lv, 1) / (skin->font2_char_height+1);
}

int skin_textarea_get_characters_per_line(Skin *skin)
{
	return gmu_widget_get_width((GmuWidget *)&skin->lv, 1) / (skin->font2_char_width+1);
}

void skin_draw_header_text(Skin *skin, char *text, SDL_Surface *target)
{
	textrenderer_draw_string(&skin->font1, text, target, 
	                         gmu_widget_get_pos_x(&skin->header, 1),
			                 gmu_widget_get_pos_y(&skin->header, 0) +
			                 (gmu_widget_get_height(&skin->header, 0) -
			                 skin->font1_char_height) / 2);
}

void skin_draw_footer_text(Skin *skin, char *text, SDL_Surface *target)
{
	int len = skin_textarea_get_characters_per_line(skin);
	textrenderer_draw_string_with_highlight(&skin->font1, &skin->font2, text, 0, target,
			                                gmu_widget_get_pos_x(&skin->footer, 1),
			                                gmu_widget_get_pos_y(&skin->footer, 0) +
			                                (gmu_widget_get_height(&skin->footer, 0) -
			                                skin->font1_char_height) / 2,
			                                len, RENDER_CROP);
}

void skin_draw_scroll_arrow_up(Skin *skin, SDL_Surface *target)
{
	SDL_Rect srect, drect;
	int ox = gmu_widget_get_pos_x(&skin->lv, 1);
	int oy = gmu_widget_get_pos_y(&skin->lv, 1);

	srect.w = skin->arrow_up->w;
	srect.h = skin->arrow_up->h;
	srect.x = 0;
	srect.y = 0;
	drect.x = ox + gmu_widget_get_width(&skin->lv, 1) - skin->arrow_up->w;
	drect.y = oy;
	drect.w = 1;
	drect.h = 1;
	if (skin->arrow_up)
		SDL_BlitSurface(skin->arrow_up, &srect, target, &drect);
}

void skin_draw_scroll_arrow_down(Skin *skin, SDL_Surface *target)
{
	SDL_Rect srect, drect;
	int ox = gmu_widget_get_pos_x(&skin->lv, 1);
	int oy = gmu_widget_get_pos_y(&skin->lv, 1);

	srect.w = skin->arrow_down->w;
	srect.h = skin->arrow_down->h;
	srect.x = 0;
	srect.y = 0;
	drect.x = ox + gmu_widget_get_width(&skin->lv, 1) - skin->arrow_down->w;
	drect.y = oy + gmu_widget_get_height(&skin->lv, 1) - skin->arrow_down->h;
	drect.w = 1;
	drect.h = 1;
	if (skin->arrow_down)
		SDL_BlitSurface(skin->arrow_down, &srect, target, &drect);
}

void skin_draw_display_symbol(Skin *skin, SDL_Surface *target, SkinDisplaySymbol symbol)
{
	SDL_Rect srect, drect;
	int ox = gmu_widget_get_pos_x(&skin->display, 0);
	int oy = gmu_widget_get_pos_y(&skin->display, 0);

	srect.w = skin->symbols_width;
	srect.h = skin->symbols_height;
	srect.y = 0;
	switch (symbol) {
		default:
		case SYMBOL_PLAY:
			srect.x = 0;
			drect.x = ox + skin->symbol_play_offset_x;
			drect.y = oy + skin->symbol_play_offset_y;
			drect.w = 1;
			drect.h = 1;
			break;
		case SYMBOL_PAUSE:
			srect.x = 1 * skin->symbols_width;
			drect.x = ox + skin->symbol_pause_offset_x;
			drect.y = oy + skin->symbol_pause_offset_y;
			drect.w = 1;
			drect.h = 1;
			break;
		case SYMBOL_STEREO:
			srect.x = 2 * skin->symbols_width;
			drect.x = ox + skin->symbol_stereo_offset_x;
			drect.y = oy + skin->symbol_stereo_offset_y;
			drect.w = 1;
			drect.h = 1;
			break;
	}
	if (skin->display_symbols && drect.x - ox >= 0 && drect.y - oy >= 0)
		SDL_BlitSurface(skin->display_symbols, &srect, target, &drect);
}
