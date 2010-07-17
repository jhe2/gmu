/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: coverimg.h  Created: 070104
 *
 * Description: Cover image loader 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "SDL.h"
#include "SDL_thread.h"

#ifndef _COVERIMG_H
#define _COVERIMG_H

typedef enum MimeType { COVER_MIME_UNKNOWN, COVER_MIME_JPEG, COVER_MIME_PNG, COVER_MIME_BMP } MimeType;

typedef struct CoverImage {
	SDL_Surface *image;
	int          target_width, target_height;
	int          visible_area_offset_x, visible_area_offset_y;
	int          visible_area_width, visible_area_height;
	SDL_Thread  *thread;
	SDL_mutex   *mutex1, *mutex2;
	char         filename[256];
	char        *image_data;
	int          image_data_size;
	int         *ready_flag;
	int          loading;
	MimeType     mime_type;
	int          thread_running;
} CoverImage;

void         cover_image_init(CoverImage *ci);
void         cover_image_free(CoverImage *ci);
void         cover_image_stop_thread(CoverImage *ci);
void         cover_image_load_image_from_file(CoverImage *ci, char *filename, int *ready_flag);
void         cover_image_load_image_from_memory(CoverImage *ci, char *image_data, int image_data_size, 
                                                char *image_mime_type, int *ready_flag);
SDL_Surface *cover_image_get_image(CoverImage *ci);
int          cover_image_free_image(CoverImage *ci);
void         cover_image_set_target_size(CoverImage *ci, int width, int height);
#endif
