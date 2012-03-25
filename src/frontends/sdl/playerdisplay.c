/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
#define BLINK_DELAY 4

extern const Skin skin;

static char  notice_message[MAX_LENGTH+1];
static int   notice_time_left;
static int   scrolling = SCROLL_AUTO;
static int   playback_symbol_blinking = 0;

void player_display_set_scrolling(int s)
{
	scrolling = s;
}

void player_display_set_playback_symbol_blinking(int blink)
{
	playback_symbol_blinking = blink;
}

static void player_display_show_volume(TextRenderer *tr, SDL_Surface *target, int volume)
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
			textrenderer_draw_char(&skin.data.font_display, vol_ch1, target, 
			                       skin.config.volume_offset_x,
			                       skin.config.volume_offset_y);		
		if (vol_ch2)
			textrenderer_draw_char(&skin.data.font_display, vol_ch2, target, 
			                       skin.config.volume_offset_x + tr->chwidth,
			                       skin.config.volume_offset_y);
	}*/
}

void player_display_draw(TextRenderer *tr, TrackInfo *ti, PB_Status player_status,
                         int ptime_msec, int ptime_remaining, int volume,
                         int busy, int shutdown_time, SDL_Surface *buffer)
{
	int  min = 0, sec = 0;
	char buf[MAX_LENGTH+1];
	int  title_scroller_chars = ((skin.title_scroller_offset_x2 > 0 ? skin.title_scroller_offset_x2 :
	                             (gmu_widget_get_width((GmuWidget *)&skin.display, 0) + skin.title_scroller_offset_x2))
	                             - skin.title_scroller_offset_x1) / (skin.font_display_char_width+1);
	static int blink_state = BLINK_DELAY;

	if (player_status != STOPPED) {
		SkinDisplaySymbol symbol = (player_status == PLAYING ? SYMBOL_PLAY  : 
		                            player_status == PAUSED  ? SYMBOL_PAUSE : SYMBOL_NONE);
		if (symbol != SYMBOL_NONE) {
			if ((playback_symbol_blinking && blink_state < BLINK_DELAY / 2) || !playback_symbol_blinking)
				skin_draw_display_symbol((Skin *)&skin, buffer, symbol);
		}

		if (trackinfo_get_channels(ti) > 1)
			skin_draw_display_symbol((Skin *)&skin, buffer, SYMBOL_STEREO);

		blink_state--;
		if (blink_state < 0) blink_state = BLINK_DELAY;

		if (ptime_remaining) {
			if (ptime_msec >= 0) {
				min = (ti->length - ptime_msec / 1000) / 60;
				sec = (ti->length - ptime_msec / 1000) - min * 60;
			} else {
				min = -1;
			}
		} else {
			min = (ptime_msec / 1000) / 60;
			sec = (ptime_msec / 1000) - min * 60;
		}
	}

	/* Volume */
	player_display_show_volume(tr, buffer, volume);

	if (skin.title_scroller_offset_x1 >= 0 && skin.title_scroller_offset_y >= 0) {
		char        lcd_text[MAX_LENGTH+1];
		char        text[MAX_LENGTH+1];
		int         len = (title_scroller_chars > MAX_LENGTH ? 
		                   MAX_LENGTH : title_scroller_chars);
		int         str_max_visible_len = (busy && len > 2) ? 
		                                  len-2 : (shutdown_time != 0 && len > 4 ? len-4 : len);
		static int  display_message_pos = 0;   
		int         str_len = 0;
		UCodePoint *lcd_text_cp = NULL;
		int         lcd_text_cp_size = 0;

		trackinfo_get_full_title(ti, text, MAX_LENGTH);

		/* Fix the string in case it was cropped due to length limit 
		 * and left an incomplete uft8 char at the end: */
		charset_fix_broken_utf8_string(text);
		str_len = charset_utf8_len(text)+1;

		if (notice_time_left == 0) {
			if (-display_message_pos < (str_len + str_max_visible_len)) {
				display_message_pos--;
			} else {
				display_message_pos = 0;
			}
			lcd_text_cp_size = str_len+(2*str_max_visible_len)+1;
			lcd_text_cp = malloc(sizeof(UCodePoint) * lcd_text_cp_size);
			if (lcd_text_cp) {
				int i;
				strtoupper(lcd_text, text, strlen(text)+1);
				for (i = 0; i < lcd_text_cp_size; i++) lcd_text_cp[i] = ' ';
				charset_utf8_to_codepoints(lcd_text_cp+str_max_visible_len, lcd_text, str_len);
			}
		} else {
			int i;
			strncpy(lcd_text, notice_message, len);
			str_len = charset_utf8_len(lcd_text)+1;
			lcd_text[len] = '\0';
			lcd_text_cp_size = str_len+str_max_visible_len+1;
			lcd_text_cp = malloc(sizeof(UCodePoint) * lcd_text_cp_size);
			for (i = 0; i < lcd_text_cp_size; i++) lcd_text_cp[i] = ' ';
			charset_utf8_to_codepoints(lcd_text_cp, lcd_text, str_len);
		}

		if (lcd_text_cp) {
			UCodePoint *to_draw = lcd_text_cp;
			if (display_message_pos < 0 && !notice_time_left)
				to_draw -= display_message_pos;
			textrenderer_draw_string_codepoints(&skin.font_display, to_draw,
												str_max_visible_len,
												buffer, 
												skin.title_scroller_offset_x1,
												skin.title_scroller_offset_y);
			free(lcd_text_cp);
		}
		if (notice_time_left) notice_time_left--;
		
		if (busy && len > 2) { /* show busy indicator when busy */
			char       busy_ch[] = { '-', '\\', 'I', '/' };
			static int ch_sel = 0;

			textrenderer_draw_char(&skin.font_display, busy_ch[ch_sel], buffer,
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
			textrenderer_draw_string(&skin.font_display, tmp, buffer,
		                             skin.title_scroller_offset_x1 +
		                             (title_scroller_chars-4) *
			                         (skin.font_display_char_width+1),
		                             skin.title_scroller_offset_y);
		}
	}
	if (skin.bitrate_offset_x >= 0 && skin.bitrate_offset_y >= 0) {
		snprintf(buf, 27, "%4d KBPS", (int)(ti->recent_bitrate / 1000));
		textrenderer_draw_string(&skin.font_display, buf, buffer,
		                         skin.bitrate_offset_x, skin.bitrate_offset_y);
	}
	if (skin.frequency_offset_x >= 0 && skin.frequency_offset_y >= 0) {
		char tmp6[6];
		snprintf(tmp6, 6, "%05d", ti->samplerate);
		snprintf(buf, 27, "%c%c.%c KHZ", tmp6[0] != '0' ? tmp6[0] : ' ', tmp6[1], tmp6[2]);
		textrenderer_draw_string(&skin.font_display, buf, buffer,
		                         skin.frequency_offset_x, skin.frequency_offset_y);
	}
	if (skin.time_offset_x >= 0 && skin.time_offset_y >= 0) {
		if (min >= 0 && min <= 99 && sec >= 0)
			snprintf(buf, 27, " %02d:%02d", min, sec);
		else if (min > 99 && sec >= 0)
			snprintf(buf, 27, "%03d:%02d", min, sec);
		else
			snprintf(buf, 27, " --:--");
		textrenderer_draw_string(&skin.font_display, buf, buffer,
		                         skin.time_offset_x, skin.time_offset_y);
	}
}

void player_display_set_notice_message(char *message, int timeout)
{
	strncpy(notice_message, message, MAX_LENGTH);
	notice_message[MAX_LENGTH] = '\0';
	notice_time_left = timeout;
}
