/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2025 Johannes Heimansberg (wej.k.vu)
 *
 * File: textrenderer.h  Created: 060929
 *
 * Description: Bitmap/TrueType font renderer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#ifndef SDLFE_WITHOUT_SDL_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include "charset.h"

#ifndef GMU_TEXTRENDERER_H
#define GMU_TEXTRENDERER_H
typedef enum { RENDERER_BITMAP, RENDERER_TRUETYPE } Renderer_Type;

struct _TextRenderer
{
	Renderer_Type renderer_type;
	SDL_Surface  *chars;
	int           chwidth, chheight;
#ifndef SDLFE_WITHOUT_SDL_TTF
	TTF_Font     *ttf_font;
	SDL_Color     ttf_color;
	int           ttf_size;
	int           line_height;
#endif
};

typedef struct _TextRenderer TextRenderer;

typedef enum { RENDER_DEFAULT, RENDER_CROP, RENDER_ARROW } Render_Mode;

int textrenderer_init(
	TextRenderer *tr, const char *chars_file,
	int chwidth, int chheight
);
int textrenderer_init_ttf(
	TextRenderer *tr, const char *ttf_file,
	int fontsize_points, SDL_Color ttf_color
);
void textrenderer_free(TextRenderer *tr);
void textrenderer_draw_char(
	const TextRenderer *tr, UCodePoint ch, SDL_Surface *target,
	int target_x, int target_y
);
void textrenderer_draw_string_codepoints(
	const TextRenderer *tr, const UCodePoint *str,
	int str_len, SDL_Surface *target,
	int target_x, int target_y
);
void textrenderer_draw_string(
	const TextRenderer *tr, const char *str, SDL_Surface *target,
	int target_x, int target_y
);
void textrenderer_draw_string_with_highlight(
	const TextRenderer *tr1, const TextRenderer *tr2,
	const char *str, int str_offset,
	SDL_Surface *target, int target_x, int target_y,
	int max_length, Render_Mode rm
);
int textrenderer_get_string_length(const char *str);
int textrenderer_get_line_height(const TextRenderer *tr);
#endif
