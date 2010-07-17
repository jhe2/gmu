/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: textrenderer.c  Created: 060929
 *
 * Description: Bitmap font renderer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <string.h>
#include "textrenderer.h"
#include "SDL.h"
#include "SDL_image.h"

int lcd_init(LCD *lcd, char *chars_file, int chwidth, int chheight)
{
	int          result = 0;
	SDL_Surface *tmp = IMG_Load(chars_file);

	lcd->chars = NULL;
	if (tmp) {
		if ((lcd->chars = SDL_DisplayFormatAlpha(tmp))) {
			lcd->chwidth  = chwidth;
			lcd->chheight = chheight;
			result = 1;
		}
		SDL_FreeSurface(tmp);
	}
	return result;
}

void lcd_free(LCD *lcd)
{
	if (lcd->chars != NULL) {
		SDL_FreeSurface(lcd->chars);
		lcd->chars = NULL;
	}
}

void lcd_draw_char(const LCD *lcd, char ch, SDL_Surface *target, int target_x, int target_y)
{
	int      n = ((unsigned char)ch - '!') * lcd->chwidth;
	SDL_Rect srect, drect;

	if (n >= 0) {
		srect.x = 1 + n;
		srect.y = 1;
		srect.w = lcd->chwidth;
		srect.h = lcd->chheight;

		drect.x = target_x;
		drect.y = target_y;
		drect.w = 1;
		drect.h = 1;

		SDL_BlitSurface(lcd->chars, &srect, target, &drect);
	}
}

void lcd_draw_string(const LCD *lcd, const char *str, SDL_Surface *target, int target_x, int target_y)
{
	int i;
	int l = (int)strlen(str);

	for (i = 0; i < l; i++)
		lcd_draw_char(lcd, str[i], target, target_x + i * (lcd->chwidth + 1), target_y);
}

int lcd_get_string_length(const char *str)
{
	int i, len = (int)strlen(str);
	int len_const = len;

	for (i = 0; i < len_const-1; i++)
		if (str[i] == '*' && str[i+1] == '*') len--;
	return len;
}

void lcd_draw_string_with_highlight(const LCD *lcd1, const LCD *lcd2,
                                    const char *str, int str_offset,
                                    SDL_Surface *target, int target_x, int target_y,
                                    int max_length, Render_Mode rm)
{
	int highlight = 0;
	int i, j;
	int l = (int)strlen(str);

	if (rm == RENDER_ARROW) {
		if (str_offset > 0)
			lcd_draw_char(lcd2, '<', target, target_x, target_y);
		if (lcd_get_string_length(str) - str_offset > max_length) {
			lcd_draw_char(lcd2, '>', target, target_x + (max_length - 1) * (lcd2->chwidth + 1), target_y);
			max_length--;
		}
	}

	if (rm == RENDER_CROP) {
		int len = (int)strlen(str);

		if (len > max_length) {
			int current_max = 0;

			for (i = 0, j = 0; j < max_length; i++, j++) {
				if (str[i] == '*' && i+1 < l && str[i+1] == '*') j-=2;
				if (str[i] == ' ') current_max = j;
			}
			max_length = current_max;
		}
	}

	for (i = 0, j = 0; i < l && j - str_offset < max_length; i++, j++) {
		if (str[i] == '*' && i+1 < l && str[i+1] == '*') {
			highlight = !highlight;
			i+=2;
		}
		if (j >= str_offset && (j != str_offset || str_offset == 0)) {
			if (!highlight)
				lcd_draw_char(lcd1, str[i], target, 
				              target_x + (j-str_offset) * (lcd1->chwidth + 1), target_y);
			else
				lcd_draw_char(lcd2, str[i], target, 
				              target_x + (j-str_offset) * (lcd2->chwidth + 1), target_y);
		}
	}
}
