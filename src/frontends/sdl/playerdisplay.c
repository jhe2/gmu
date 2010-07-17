/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: playerdisplay.c  Created: 061109
 *
 * Description: Gmu's display
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <string.h>
#include "SDL.h"
#include "playerdisplay.h"
#include "textrenderer.h"
#include "trackinfo.h"
#include "skin.h"
#include "pbstatus.h"
#include "util.h"
#define MAX_LENGTH 512

extern const Skin skin;

static char  notice_message[MAX_LENGTH+1];
static int   notice_time_left;
static int   scrolling = SCROLL_AUTO;

void player_display_set_scrolling(int s)
{
	scrolling = s;
}

static void player_display_show_volume(LCD *lcd, SDL_Surface *target, int volume)
{
/*	if (skin.config.volume_offset_x >= 0 && skin.config.volume_offset_y >= 0) {
		char vol_ch1 = 0, vol_ch2 = 0;

		if (volume >= 5)       vol_ch1 = 'k';
		else if (volume == 1)  vol_ch1 = 'c';
		else if (volume == 2)  vol_ch1 = 'e';
		else if (volume == 3)  vol_ch1 = 'g';
		else if (volume == 4)  vol_ch1 = 'i';

		if (volume == 6)       vol_ch2 = 'd';
		else if (volume == 7)  vol_ch2 = 'f';
		else if (volume == 8)  vol_ch2 = 'h';
		else if (volume == 9)  vol_ch2 = 'j';
		else if (volume == 10) vol_ch2 = 'l';

		if (vol_ch1)
			lcd_draw_char(&skin.data.font_display, vol_ch1, target, 
			              skin.config.volume_offset_x,
			              skin.config.volume_offset_y);		
		if (vol_ch2)
			lcd_draw_char(&skin.data.font_display, vol_ch2, target, 
			              skin.config.volume_offset_x + lcd->chwidth,
			              skin.config.volume_offset_y);
	}*/
}

void player_display_draw(LCD *lcd, TrackInfo *ti, PB_Status player_status,
                         int ptime_msec, int ptime_remaining, int volume,
                         int busy, int shutdown_time, SDL_Surface *buffer)
{
	int  min = 0, sec = 0;
	char buf[MAX_LENGTH+1];
	int  title_scroller_chars = ((skin.title_scroller_offset_x2 > 0 ? skin.title_scroller_offset_x2 :
	                             (gmu_widget_get_width((GmuWidget *)&skin.display, 1) + skin.title_scroller_offset_x2))
	                             - skin.title_scroller_offset_x1) / (skin.font_display_char_width+1);

	if (player_status != STOPPED) {
		SkinDisplaySymbol symbol = (player_status == PLAYING ? SYMBOL_PLAY  : 
		                            player_status == PAUSED  ? SYMBOL_PAUSE : SYMBOL_NONE);
		if (symbol != SYMBOL_NONE)
			skin_draw_display_symbol((Skin *)&skin, buffer, symbol);

		if (trackinfo_get_channels(ti) > 1)
			skin_draw_display_symbol((Skin *)&skin, buffer, SYMBOL_STEREO);

		if (ptime_remaining) {
			min = (ti->length - ptime_msec / 1000) / 60;
			sec = (ti->length - ptime_msec / 1000) - min * 60;
		} else {
			min = (ptime_msec / 1000) / 60;
			sec = (ptime_msec / 1000) - min * 60;
		}
	}

	/* Volume */
	player_display_show_volume(lcd, buffer, volume);

	if (skin.title_scroller_offset_x1 >= 0 && skin.title_scroller_offset_y >= 0) {
		char       lcd_text[MAX_LENGTH+1], temp[MAX_LENGTH+1];
		char       text[MAX_LENGTH+1];
		int        len = (title_scroller_chars + 1 > MAX_LENGTH ? 
		                  MAX_LENGTH : title_scroller_chars + 1);
		static int display_message_pos = 0;

		trackinfo_get_full_title(ti, temp, MAX_LENGTH);
		memset(text, ' ', len);
		snprintf(text + len, MAX_LENGTH-len, "%s ", temp);
		if (notice_time_left == 0) {
			if (display_message_pos < (int)strlen(text)) {
				if (((int)(strlen(temp)) > title_scroller_chars &&
				    scrolling == SCROLL_AUTO) || scrolling == SCROLL_ALWAYS)
					display_message_pos++;
				else
					display_message_pos = title_scroller_chars + 1;
			} else {
				display_message_pos = 0;
			}
			strtoupper(lcd_text, text + display_message_pos,
			           (busy && len > 2) ? len-2 : (shutdown_time != 0 && len > 3 ? len-4 : len));
		} else {
			strncpy(lcd_text, notice_message, len);
			lcd_text[len] = '\0';
			notice_time_left--;
		}
		lcd_draw_string(&skin.font_display, lcd_text, buffer,
		                skin.title_scroller_offset_x1,
		                skin.title_scroller_offset_y);
		if (busy && len > 2) { /* show busy indicator when busy */
			char       busy_ch[] = { '-', '\\', 'I', '/' };
			static int ch_sel = 0;

			lcd_draw_char(&skin.font_display, busy_ch[ch_sel], buffer,
			              skin.title_scroller_offset_x1 +
			              (title_scroller_chars-1) *
			              (skin.font_display_char_width+1),
			              skin.title_scroller_offset_y);
			ch_sel++;
			if (ch_sel > 3) ch_sel = 0;
		} else if ((shutdown_time > 0 || shutdown_time == -1) && len > 3) {
			char tmp[5];

			if (shutdown_time > 0)
				snprintf(tmp, 5, " %3d", shutdown_time);
			else
				snprintf(tmp, 5, " [S]");
			lcd_draw_string(&skin.font_display, tmp, buffer,
		                skin.title_scroller_offset_x1 +
		                (title_scroller_chars-4) *
			            (skin.font_display_char_width+1),
		                skin.title_scroller_offset_y);
		}
	}
	if (skin.bitrate_offset_x >= 0 && skin.bitrate_offset_y >= 0) {
		snprintf(buf, 27, "%4d KBPS", (int)(ti->recent_bitrate / 1000));
		lcd_draw_string(&skin.font_display, buf, buffer,
		                skin.bitrate_offset_x, skin.bitrate_offset_y);
	}
	if (skin.frequency_offset_x >= 0 && skin.frequency_offset_y >= 0) {
		char tmp6[6];
		snprintf(tmp6, 6, "%05d", ti->samplerate);
		snprintf(buf, 27, "%c%c.%c KHZ", tmp6[0] != '0' ? tmp6[0] : ' ', tmp6[1], tmp6[2]);
		lcd_draw_string(&skin.font_display, buf, buffer,
		                skin.frequency_offset_x, skin.frequency_offset_y);
	}
	if (skin.time_offset_x >= 0 && skin.time_offset_y >= 0) {
		if (min >= 0 && sec >= 0)
			snprintf(buf, 27, "%02d:%02d", min, sec);
		else
			snprintf(buf, 27, "--:--");
		lcd_draw_string(&skin.font_display, buf, buffer,
		                skin.time_offset_x, skin.time_offset_y);
	}
}

void player_display_set_notice_message(char *message, int timeout)
{
	strncpy(notice_message, message, MAX_LENGTH);
	notice_message[MAX_LENGTH] = '\0';
	notice_time_left = timeout;
}
