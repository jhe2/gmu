/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: coverimg.c  Created: 070104
 *
 * Description: Cover image loader
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
#include "SDL_thread.h"
#include "SDL_image.h"
#ifndef SDLFE_WITHOUT_SDL_GFX
#include "SDL_rotozoom.h"
#endif
#include "coverimg.h"
#include "../../png.h"
#include "../../jpeg.h"
#include "../../bmp.h"
#include "debug.h"
#include FILE_HW_H /* MAX_COVER_IMAGE_PIXELS */

static int cover_image_thread(void *udata)
{
#ifndef SDLFE_WITHOUT_SDL_GFX
	CoverImage  *ci = (CoverImage *)udata;
	ImageSize    is;
	int          width = 0, height = 0;

	wdprintf(V_DEBUG, "coverimg", "Loader thread.\n");
	ci->thread_running = 1;
	while (ci->thread_running) {
		if (ci->loading) {
			SDL_Surface *cover_fullsize = NULL;

			while (SDL_mutexP(ci->mutex1) == -1) SDL_Delay(50);
			if (ci->filename[0] != '\0')
				wdprintf(V_INFO, "coverimg", "Loading \"%s\"\n", ci->filename);
			else
				wdprintf(V_INFO, "coverimg", "Loading image from memory...\n");
			while (SDL_mutexP(ci->mutex2) == -1) SDL_Delay(50);
			if (ci->filename[0] != '\0') {
				char fn[256];

				strncpy(fn, ci->filename, 255);
				ci->filename[0] = '\0';
				SDL_mutexV(ci->mutex2);
				/* check if cover image dimensions are small enough: */
				if (!png_get_dimensions_from_file(&is, fn, &width, &height))
					if (!jpeg_get_dimensions_from_file(&is, fn, &width, &height))
						bmp_get_dimensions_from_file(&is, fn, &width, &height);
				wdprintf(V_DEBUG, "coverimg", "image size = %d x %d\n", width, height);
				if (width * height < MAX_COVER_IMAGE_PIXELS && width > 0 && height > 0) {
					cover_fullsize = IMG_Load(fn);
					wdprintf(V_DEBUG, "coverimg", "Cover image \"%s\" loaded. Now resizing...\n", fn);
				} else {
					wdprintf(V_WARNING, "coverimg", "Cover image too large or bad image data.\n");
				}
			} else if (ci->image_data != NULL) {
				/* Try to load image from memory... */
				switch (ci->mime_type) {
					case COVER_MIME_JPEG:
						jpeg_get_dimensions_from_memory(&is, ci->image_data, ci->image_data_size, &width, &height);
						break;
					case COVER_MIME_PNG:
						png_get_dimensions_from_memory(&is, ci->image_data, ci->image_data_size, &width, &height);
						break;
					case COVER_MIME_BMP:
						bmp_get_dimensions_from_memory(&is, ci->image_data, ci->image_data_size, &width, &height);
						break;
					default:
						break;
				}
				wdprintf(V_DEBUG, "coverimg", "image dimensions: %d x %d\n", width, height);
				if (width * height < MAX_COVER_IMAGE_PIXELS && width > 0 && height > 0)
					cover_fullsize = IMG_Load_RW(SDL_RWFromConstMem(ci->image_data, ci->image_data_size), 1);
				else
					wdprintf(V_WARNING, "coverimg", "Cover image too large or bad image data.\n");
				if (!cover_fullsize)
					wdprintf(V_WARNING, "coverimg", "Failed. Probably bad image data in tag.\n");
				SDL_mutexV(ci->mutex2);
			} else {
				SDL_mutexV(ci->mutex2);
				wdprintf(V_INFO, "coverimg", "No image available to load.\n");
			}

			if (cover_fullsize) {
				SDL_Surface *tmp;
				float        ratio_image  = (float)cover_fullsize->w / cover_fullsize->h;
				float        ratio_screen = (float)ci->target_width / ci->target_height;
				float        rx = 1.0, ry = 1.0;

				if (ci->target_height <= 0 || ratio_image > ratio_screen)
					ry = rx = (float)ci->target_width / cover_fullsize->w;
				else if (ci->target_height > 0)
					ry = rx = (float)ci->target_height / cover_fullsize->h;
				if (ci->image) {
					SDL_FreeSurface(ci->image);
					ci->image = NULL;
				}
				tmp = zoomSurface(cover_fullsize, rx, ry, 1);
				/*printf("coverimg: orig. size = %dx%d\t new size = %dx%d\n",
						cover_fullsize->w, cover_fullsize->h, tmp->w, tmp->h);*/
				SDL_FreeSurface(cover_fullsize);
				if (tmp) {
					ci->image = SDL_DisplayFormat(tmp); 
					SDL_FreeSurface(tmp);
					wdprintf(V_INFO, "coverimg", "Loaded and resized cover image successfully.\n");
				}
			} else {
				if (ci->image) {
					SDL_FreeSurface(ci->image);
					ci->image = NULL;
				}
			}
			if (ci->ready_flag) *ci->ready_flag = 1;
			ci->loading = 0;
			SDL_mutexV(ci->mutex1);
		}
		SDL_Delay(333);
	}
#else
	wdprintf(V_INFO, "coverimg", "Cover image support has been disabled at compile time.\n");
#endif
	return 1;
}

void cover_image_init(CoverImage *ci)
{
	ci->mutex1 = SDL_CreateMutex();
	ci->mutex2 = SDL_CreateMutex();
	ci->visible_area_offset_x = 0;
	ci->visible_area_offset_y = 0;
	ci->visible_area_width = -1;
	ci->visible_area_height = -1;
	ci->image = NULL;
	ci->image_data = NULL;
	ci->image_data_size = 0;
	ci->loading = 0;
	ci->thread = NULL;
	ci->thread_running = 0;
}

void cover_image_stop_thread(CoverImage *ci)
{
	ci->thread_running = 0;
	if (ci->thread)
		SDL_WaitThread(ci->thread, NULL);
	wdprintf(V_DEBUG, "coverimg", "Thread stopped.\n");
	ci->thread = NULL;
}

void cover_image_free(CoverImage *ci)
{
	SDL_DestroyMutex(ci->mutex1);
	SDL_DestroyMutex(ci->mutex2);
	if (ci->image) SDL_FreeSurface(ci->image);
	if (ci->image_data) free(ci->image_data);
}

int cover_image_free_image(CoverImage *ci)
{
	int result = 0;
	if (ci->image) {
		SDL_FreeSurface(ci->image);
		ci->image = NULL;
		result = 1;
	}
	return result;
}

void cover_image_load_image_from_file(CoverImage *ci, char *filename, int *ready_flag)
{
	while (SDL_mutexP(ci->mutex2) == -1);
	strncpy(ci->filename, filename, 255);
	ci->image_data = NULL;
	SDL_mutexV(ci->mutex2);
	ci->loading = 1;
	ci->ready_flag = ready_flag;
	if (!ci->thread)
		ci->thread = SDL_CreateThread(cover_image_thread, ci);
}

void cover_image_load_image_from_memory(CoverImage *ci, char *image_data, int image_data_size, 
                                        char *image_mime_type, int *ready_flag)
{
	if (strncmp(image_mime_type, "image/jpg",  9)  == 0 ||
	    strncmp(image_mime_type, "image/jpeg", 10) == 0)
		ci->mime_type = COVER_MIME_JPEG;
	else if (strncmp(image_mime_type, "image/png", 9) == 0)
		ci->mime_type = COVER_MIME_PNG;
	else if (strncmp(image_mime_type, "image/bmp", 9) == 0)
		ci->mime_type = COVER_MIME_BMP;
	else
		ci->mime_type = COVER_MIME_UNKNOWN;

	if (ci->mime_type == COVER_MIME_JPEG || 
	    ci->mime_type == COVER_MIME_PNG || ci->mime_type == COVER_MIME_BMP) {
		wdprintf(V_INFO, "coverimg", "Loading cover image of type %s from song meta data (tag)...\n",
		         image_mime_type);
		while (SDL_mutexP(ci->mutex2) == -1);
		if (ci->image_data) free(ci->image_data);
		ci->image_data = malloc(image_data_size);
		if (ci->image_data) { 
			memcpy(ci->image_data, image_data, image_data_size);
			ci->image_data_size = image_data_size;
			ci->filename[0] = '\0';
		}
		SDL_mutexV(ci->mutex2);
		if (ci->image_data) {
			ci->loading = 1;
			ci->ready_flag = ready_flag;
			if (!ci->thread)
				ci->thread = SDL_CreateThread(cover_image_thread, ci);
		}
	}
}

SDL_Surface *cover_image_get_image(CoverImage *ci)
{
	SDL_Surface *result = NULL;
	if (!ci->loading) {
		result = ci->image;
	}
	return result;
}

void cover_image_set_target_size(CoverImage *ci, int width, int height)
{
	ci->target_width  = width;
	ci->target_height = height;
}
