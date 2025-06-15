/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2025 Johannes Heimansberg (wej.k.vu)
 *
 * File: textrenderer.c  Created: 060929
 *
 * Description: Bitmap/TrueType font renderer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <string.h>
#include "textrenderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#ifndef SDLFE_WITHOUT_SDL_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include "charset.h"
#include "debug.h"

int textrenderer_init(TextRenderer *tr, const char *chars_file, int chwidth, int chheight)
{
	int          result = 0;
	SDL_Surface *bitmap_font = IMG_Load(chars_file);

	tr->chars = NULL;
#ifndef SDLFE_WITHOUT_SDL_TTF
	tr->ttf_font = NULL;
#endif
	tr->renderer_type = RENDERER_BITMAP;
	if (bitmap_font) {
		tr->chars = bitmap_font;
		tr->chwidth  = chwidth;
		tr->chheight = chheight;
		tr->line_height = chheight+1;
		result = 1;
	} else {
		wdprintf(V_ERROR, "textrenderer", "Error initializing bitmap font renderer: %s size=%dx%d\n", chars_file, chwidth, chheight);
	}
	return result;
}

int textrenderer_init_ttf(TextRenderer *tr, const char *ttf_file, int fontsize_points, SDL_Color ttf_color)
{
	int result = 0;

#ifndef SDLFE_WITHOUT_SDL_TTF
	if (!TTF_WasInit() && TTF_Init() == -1) {
		wdprintf(V_ERROR, "textrenderer", "TTF_Init: %s\n", TTF_GetError());
		return result;
	}

	tr->renderer_type = RENDERER_TRUETYPE;
	tr->chars = NULL;
	tr->ttf_font = TTF_OpenFont(ttf_file, fontsize_points);
	if (tr->ttf_font) {
		tr->line_height = TTF_FontLineSkip(tr->ttf_font);
		tr->ttf_color = ttf_color;
		result = 1;
	} else {
		tr->line_height = -1;
		wdprintf(V_ERROR, "textrenderer", "Error opening TTF file: %s\n", ttf_file);
	}
#else
	wdprintf(V_ERROR, "textrenderer", "ERROR: TTF support has been disabled at compile-time.\n");
#endif
	return result;
}

void textrenderer_free(TextRenderer *tr)
{
	if (tr->chars != NULL) {
		SDL_FreeSurface(tr->chars);
		tr->chars = NULL;
	}
#ifndef SDLFE_WITHOUT_SDL_TTF
	if (tr->ttf_font != NULL) {
		TTF_CloseFont(tr->ttf_font);
	}
#endif
}

void textrenderer_draw_char(const TextRenderer *tr, UCodePoint ch, SDL_Surface *target, int target_x, int target_y)
{
	const int n = (ch - '!') * tr->chwidth;
	SDL_Rect  srect, drect;

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

void textrenderer_draw_string_codepoints(const TextRenderer *tr, const UCodePoint *str, int str_len, SDL_Surface *target, int target_x, int target_y)
{
	int i;
	for (i = 0; i < str_len && str[i]; i++)
		textrenderer_draw_char(tr, str[i], target, target_x + i * (tr->chwidth + 1), target_y);
}

/*
 * textrenderer_draw_string() renders a string of text (single line)
 * onto the supplied SDL_Surface. Depending on the renderer type
 * the text is either rendered with a bitmap font or a truetype font.
 */
void textrenderer_draw_string(const TextRenderer *tr, const char *str, SDL_Surface *target, int target_x, int target_y)
{
	int utf8_chars = charset_utf8_len(str)+1;
	switch (tr->renderer_type) {
		case RENDERER_BITMAP: {
			UCodePoint *ustr = utf8_chars > 0 ? malloc(sizeof(UCodePoint) * (utf8_chars+1)) : NULL;

			if (ustr && charset_utf8_to_codepoints(ustr, str, utf8_chars)) {
				textrenderer_draw_string_codepoints(tr, ustr, utf8_chars, target, target_x, target_y);
			}
			if (ustr) free(ustr);
			break;
		}
		case RENDERER_TRUETYPE: {
			SDL_Rect     drect;
			SDL_Surface *surface = TTF_RenderUTF8_Solid(tr->ttf_font, str, tr->ttf_color);
			drect.x = target_x;
			drect.y = target_y;
			drect.w = 1;
			drect.h = 1;
			SDL_BlitSurface(surface, NULL, target, &drect);
			SDL_FreeSurface(surface);
			break;
		}
	}
}

int textrenderer_get_string_length(const char *str)
{
	int i, len = (int)strlen(str);
	int len_const = len;
	int utf8_chars = charset_utf8_len(str);

	for (i = 0; i < len_const-1; i++)
		if (str[i] == '*' && str[i+1] == '*') utf8_chars--;
	return utf8_chars;
}

/*
 * textrenderer_draw_string_with_highlight() renders a string similar
 * to textrenderer_draw_string(). In addition this function supports
 * highlighting pieces of the text in a different color. Parts to be
 * highlighted need to be enclosed in two '*' characters, e.g.
 * "This is **important**.".
 */
void textrenderer_draw_string_with_highlight(
	const TextRenderer *tr1, const TextRenderer *tr2,
	const char *str, int str_offset,
	SDL_Surface *target, int target_x, int target_y,
	int max_length, Render_Mode rm
)
{
	int highlight = 0;
	int i, j;
	int l = (int)strlen(str);
	int utf8_chars = charset_utf8_len(str)+1;
	UCodePoint *ustr = utf8_chars > 0 ? malloc(sizeof(UCodePoint) * (utf8_chars+1)) : NULL;

	if (rm == RENDER_ARROW) {
		if (str_offset > 0)
			textrenderer_draw_char(tr2, '<', target, target_x, target_y);
		if (textrenderer_get_string_length(str) - str_offset > max_length) {
			textrenderer_draw_char(tr2, '>', target, target_x + (max_length - 1) * (tr2->chwidth + 1), target_y);
			max_length--;
		}
	}

	if (rm == RENDER_CROP) {
		if (utf8_chars > max_length) {
			int current_max = 0;

			for (i = 0, j = 0; j < max_length && str[i] != '\0'; i++, j++) {
				if (str[i] == '*' && i+1 < l && str[i+1] == '*') j-=2;
				if (str[i] == ' ') current_max = j;
			}
			max_length = current_max;
		}
	}

	switch (tr1->renderer_type) {
		case RENDERER_BITMAP: {
			if (ustr && charset_utf8_to_codepoints(ustr, str, utf8_chars)) {
				for (i = 0, j = 0; i < utf8_chars && j - str_offset < max_length; i++, j++) {
					if (str[i] == '*' && i+1 < utf8_chars && str[i+1] == '*') {
						highlight = !highlight;
						i+=2;
					}
					if (j >= str_offset && (j != str_offset || str_offset == 0)) {
						if (!highlight)
							textrenderer_draw_char(
								tr1, ustr[i], target,
								target_x + (j-str_offset) * (tr1->chwidth + 1), target_y
							);
						else
							textrenderer_draw_char(
								tr2, ustr[i], target,
								target_x + (j-str_offset) * (tr2->chwidth + 1), target_y
							);
					}
				}
			}
			break;
		}
		case RENDERER_TRUETYPE: {
			printf("stub: [ttf] textrenderer_draw_string_with_highlight(): %s\n", str);
			/* TODO: Implement highlight rendering */
			textrenderer_draw_string(tr1, str, target, target_x, target_y);
			break;
		}
	}
	if (ustr) free(ustr);
}

int textrenderer_get_line_height(const TextRenderer *tr)
{
	return tr->line_height;
}
