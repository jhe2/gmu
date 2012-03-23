/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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

int textrenderer_init(TextRenderer *tr, char *chars_file, int chwidth, int chheight)
{
	int          result = 0;
	SDL_Surface *tmp = IMG_Load(chars_file);

	tr->chars = NULL;
	if (tmp) {
		if ((tr->chars = SDL_DisplayFormatAlpha(tmp))) {
			tr->chwidth  = chwidth;
			tr->chheight = chheight;
			result = 1;
		}
		SDL_FreeSurface(tmp);
	}
	return result;
}

void textrenderer_free(TextRenderer *tr)
{
	if (tr->chars != NULL) {
		SDL_FreeSurface(tr->chars);
		tr->chars = NULL;
	}
}

void textrenderer_draw_char(const TextRenderer *tr, char ch, SDL_Surface *target, int target_x, int target_y)
{
	int      n = ((unsigned char)ch - '!') * tr->chwidth;
	SDL_Rect srect, drect;

	if (n >= 0) {
		srect.x = 1 + n;
		srect.y = 1;
		srect.w = tr->chwidth;
		srect.h = tr->chheight;

		drect.x = target_x;
		drect.y = target_y;
		drect.w = 1;
		drect.h = 1;

		SDL_BlitSurface(tr->chars, &srect, target, &drect);
	}
}

void textrenderer_draw_string(const TextRenderer *tr, const char *str, SDL_Surface *target, int target_x, int target_y)
{
	int i;
	int l = (int)strlen(str);

	for (i = 0; i < l; i++)
		textrenderer_draw_char(tr, str[i], target, target_x + i * (tr->chwidth + 1), target_y);
}

int textrenderer_get_string_length(const char *str)
{
	int i, len = (int)strlen(str);
	int len_const = len;

	for (i = 0; i < len_const-1; i++)
		if (str[i] == '*' && str[i+1] == '*') len--;
	return len;
}

void textrenderer_draw_string_with_highlight(const TextRenderer *tr1, const TextRenderer *tr2,
                                             const char *str, int str_offset,
                                             SDL_Surface *target, int target_x, int target_y,
                                             int max_length, Render_Mode rm)
{
	int highlight = 0;
	int i, j;
	int l = (int)strlen(str);

	if (rm == RENDER_ARROW) {
		if (str_offset > 0)
			textrenderer_draw_char(tr2, '<', target, target_x, target_y);
		if (textrenderer_get_string_length(str) - str_offset > max_length) {
			textrenderer_draw_char(tr2, '>', target, target_x + (max_length - 1) * (tr2->chwidth + 1), target_y);
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
				textrenderer_draw_char(tr1, str[i], target, 
				                       target_x + (j-str_offset) * (tr1->chwidth + 1), target_y);
			else
				textrenderer_draw_char(tr2, str[i], target, 
				                       target_x + (j-str_offset) * (tr2->chwidth + 1), target_y);
		}
	}
}
