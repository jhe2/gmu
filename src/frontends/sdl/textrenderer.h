/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: textrenderer.h  Created: 060929
 *
 * Description: Bitmap font renderer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "SDL.h"
#include "SDL_image.h"

#ifndef _TEXTRENDERER_H
#define _TEXTRENDERER_H
struct _LCD
{
	SDL_Surface *chars;
	int chwidth, chheight;
};

typedef struct _LCD LCD;

typedef enum { RENDER_DEFAULT, RENDER_CROP, RENDER_ARROW } Render_Mode;

int  lcd_init(LCD *lcd, char *chars_file, int chwidth, int chheight);
void lcd_free(LCD *lcd);
void lcd_draw_char(const LCD *lcd, char ch, SDL_Surface *target, int target_x, int target_y);
void lcd_draw_string(const LCD *lcd, const char *str, SDL_Surface *target, int target_x, int target_y);
void lcd_draw_string_with_highlight(const LCD *lcd1, const LCD *lcd2,
                                    const char *str, int str_offset,
                                    SDL_Surface *target, int target_x, int target_y,
                                    int max_length, Render_Mode rm);
int  lcd_get_string_length(const char *str);
#endif
