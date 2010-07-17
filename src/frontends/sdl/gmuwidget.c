/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmuwidget.c  Created: 100120
 *
 * Description: Gmu widget
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdlib.h>
#include "../../util.h"
#include "gmuwidget.h"
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"

void gmu_widget_init(GmuWidget *gw, int x1, int y1, int x2, int y2)
{
	gw->first = NULL;
	gw->pos_x1 = x1;
	gw->pos_y1 = y1;
	gw->pos_x2 = x2;
	gw->pos_y2 = y2;
	gw->border_left   = 0;
	gw->border_right  = 0;
	gw->border_top    = 0;
	gw->border_bottom = 0;
	gw->actual_y1     = 0;
	gw->actual_x2     = 0;
	gw->actual_y2     = 0;
}

static int gmu_widget_image_add(GmuWidget *gw, char *image_file, GmuWidgetImagePosition pos)
{
	GmuWidgetImage *img = gw->first, *newimg = NULL;
	SDL_Surface    *tmp, *tmp2 = NULL;
	int             res = 0;

	if (file_exists(image_file) && (tmp = IMG_Load(image_file))) {
		tmp2 = SDL_DisplayFormat(tmp);
		SDL_FreeSurface(tmp);
	}
	if (tmp2) {
		newimg = malloc(sizeof(GmuWidgetImage));
		newimg->position = pos;
		newimg->image = tmp2;
		newimg->next = NULL;
		gw->first = newimg;
		if (img) newimg->next = img; else newimg->next = NULL;
		res = 1;
	}
	/*if (res)
		printf("gmu_widget: Image %s added.\n", image_file);
	else
		printf("gmu_widget: Adding image %s failed.\n", image_file);*/
	return res;
}

void gmu_widget_new(GmuWidget *gw, char *images_prefix, int x1, int y1, int x2, int y2)
{
	char filename[256];

	gmu_widget_init(gw, x1, y1, x2, y2);
	/* Add images... */
	snprintf(filename, 255, "%s-tl.png", images_prefix);
	gmu_widget_image_add(gw, filename, TOP_LEFT);
	snprintf(filename, 255, "%s-tr.png", images_prefix);
	gmu_widget_image_add(gw, filename, TOP_RIGHT);
	snprintf(filename, 255, "%s-bl.png", images_prefix);
	gmu_widget_image_add(gw, filename, BOTTOM_LEFT);
	snprintf(filename, 255, "%s-br.png", images_prefix);
	gmu_widget_image_add(gw, filename, BOTTOM_RIGHT);
	snprintf(filename, 255, "%s-tm.png", images_prefix);
	gmu_widget_image_add(gw, filename, TOP_MIDDLE);
	snprintf(filename, 255, "%s-bm.png", images_prefix);
	gmu_widget_image_add(gw, filename, BOTTOM_MIDDLE);
	snprintf(filename, 255, "%s-l.png", images_prefix);
	gmu_widget_image_add(gw, filename, LEFT);
	snprintf(filename, 255, "%s-r.png", images_prefix);
	gmu_widget_image_add(gw, filename, RIGHT);
	snprintf(filename, 255, "%s-m.png", images_prefix);
	gmu_widget_image_add(gw, filename, MIDDLE);
}

void gmu_widget_free(GmuWidget *gw)
{
	GmuWidgetImage *img = gw->first;
	while (img) {
		GmuWidgetImage *tmp;
		if (img->image) SDL_FreeSurface(img->image);
		tmp = img->next;
		free(img);
		img = tmp;
	}
}

void gmu_widget_draw(GmuWidget *gw, SDL_Surface *target)
{
	GmuWidgetImage *img = gw->first;

	/* Fetch widths and heights of surrounding elements to be able to
	 * calculate the size of the middle area */
	while (img) {
		switch (img->position) {
			case TOP_LEFT:
				gw->border_left = img->image->w;
				gw->border_top = img->image->h;
				break;
			case LEFT:
				gw->border_left = img->image->w;
				break;
			case TOP_RIGHT:
				gw->border_right = img->image->w;
				gw->border_top = img->image->h;
				break;
			case RIGHT:
				gw->border_right = img->image->w;
				break;
			case TOP_MIDDLE:
				gw->border_top = img->image->h;
				break;
			case BOTTOM_MIDDLE:
				gw->border_bottom = img->image->h;
				break;
			default:
				break;
		}
		img = img->next;
	}

	/* actual_x2/actual_y2: right/bottom positions of the widget. When positive
	 * the widget has a fixed size. When negative the widget's size depends
	 * on the size of the drawing target. (target width/height - actual_x2/actual_y2) */
	gw->actual_x2 = gw->pos_x2 > 0  ? gw->pos_x2 : target->w + gw->pos_x2;
	gw->actual_y2 = gw->pos_y2 > 0  ? gw->pos_y2 : target->h + gw->pos_y2;
	gw->actual_y1 = gw->pos_y1 >= 0 ? gw->pos_y1 : target->h + gw->pos_y1;

	/* Blit the images to the surface */
	img = gw->first;
	while (img) {
		SDL_Rect r, s;
		int      x, y; /* loop counters */

		switch (img->position) {
			case TOP_LEFT:
				r.x = gw->pos_x1;
				r.y = gw->actual_y1;
				r.w = img->image->w;
				r.h = img->image->h;
				if (img->image)	SDL_BlitSurface(img->image, NULL, target, &r);
				break;
			case LEFT:
				r.x = gw->pos_x1;
				r.w = img->image->w;
				r.h = img->image->h;
				for (y = gw->actual_y1 + gw->border_top; y <= gw->actual_y2 - gw->border_bottom - img->image->h; y += img->image->h) {
					r.y = y;
					if (img->image)	SDL_BlitSurface(img->image, NULL, target, &r);
				}
				if ((s.h = gw->actual_y2 - y - gw->border_bottom) > 0) {
					s.x = 0;
					s.y = 0;
					s.w = img->image->w;
					r.w = img->image->w;
					r.y = y;
					if (img->image)	SDL_BlitSurface(img->image, &s, target, &r);
				}
				break;
			case MIDDLE:
				r.w = img->image->w;
				r.h = img->image->h;
				s.x = 0;
				s.y = 0;
				for (x = gw->pos_x1 + gw->border_left; x < gw->actual_x2 - gw->border_right; x+= img->image->w)
					for (y = gw->actual_y1 + gw->border_top; y < gw->actual_y2 - gw->border_bottom; y += img->image->h) {
						int tmpw = 0, tmph = 0;

						r.x = x;
						r.y = y;
						tmph = gw->actual_y2 - gw->border_bottom - y - img->image->h;
						tmpw = gw->actual_x2 - gw->border_right - x - img->image->w;
						if (tmph > 0) tmph = 0;
						if (tmpw > 0) tmpw = 0;
						s.h = img->image->h + tmph;
						s.w = img->image->w + tmpw;
						if (img->image)	SDL_BlitSurface(img->image, &s, target, &r);
					}
				break;
			case RIGHT:
				r.x = gw->actual_x2 - img->image->w;
				r.w = img->image->w;
				r.h = img->image->h;
				for (y = gw->actual_y1 + gw->border_top; y <= gw->actual_y2 - gw->border_bottom - img->image->h; y += img->image->h) {
					r.y = y;
					if (img->image)	SDL_BlitSurface(img->image, NULL, target, &r);
				}
				if ((s.h = gw->actual_y2 - y - gw->border_bottom) > 0) {
					s.x = 0;
					s.y = 0;
					s.w = img->image->w;
					r.w = img->image->w;
					r.y = y;
					if (img->image)	SDL_BlitSurface(img->image, &s, target, &r);
				}
				break;
			case TOP_MIDDLE:
				r.y = gw->actual_y1;
				r.w = img->image->w;
				r.h = img->image->h;
				for (x = gw->pos_x1; x <= target->w - img->image->w; x+= img->image->w) {
					r.x = x;
					if (img->image)	SDL_BlitSurface(img->image, NULL, target, &r);
				}
				if ((s.w = gw->actual_x2 - x) > 0) {
					s.x = 0;
					s.y = 0;
					s.h = img->image->h;
					r.h = img->image->h;
					r.x = x;
					if (img->image)	SDL_BlitSurface(img->image, &s, target, &r);
				}
				break;
			case TOP_RIGHT:
				r.x = gw->actual_x2 - img->image->w;
				r.y = gw->actual_y1;
				r.w = img->image->w;
				r.h = img->image->h;
				if (img->image)	SDL_BlitSurface(img->image, NULL, target, &r);
				break;
			case BOTTOM_LEFT:
				r.x = 0;
				r.y = gw->actual_y2 - gw->border_bottom;
				r.w = img->image->w;
				r.h = img->image->h;
				if (img->image)	SDL_BlitSurface(img->image, NULL, target, &r);
				break;
			case BOTTOM_MIDDLE:
				r.y = gw->actual_y2 - gw->border_bottom;
				r.w = img->image->w;
				r.h = img->image->h;
				for (x = gw->pos_x1; x <= target->w - img->image->w; x+= img->image->w) {
					r.x = x;
					if (img->image)	SDL_BlitSurface(img->image, NULL, target, &r);
				}
				if ((s.w = gw->actual_x2 - x) > 0) {
					s.x = 0;
					s.y = 0;
					s.h = img->image->h;
					r.h = img->image->h;
					r.x = x;
					if (img->image)	SDL_BlitSurface(img->image, &s, target, &r);
				}
				break;
			case BOTTOM_RIGHT:
				r.x = target->w - img->image->w;
				r.y = gw->actual_y2 - gw->border_bottom;
				r.w = img->image->w;
				r.h = img->image->h;
				if (img->image)	SDL_BlitSurface(img->image, NULL, target, &r);
				break;
		}
		/* jump to next image */
		img = img->next;
	}
}

int gmu_widget_get_width(GmuWidget *gw, int get_usable_size)
{
	return gw->actual_x2 - gw->pos_x1 -
	       (get_usable_size ? (gw->border_left + gw->border_right) : 0);
}

int gmu_widget_get_height(GmuWidget *gw, int get_usable_size)
{
	return gw->actual_y2 - gw->actual_y1 -
	       (get_usable_size ? (gw->border_top + gw->border_bottom) : 0);
}

int gmu_widget_get_pos_x(GmuWidget *gw, int get_usable_pos)
{
	return gw->pos_x1 + (get_usable_pos ? gw->border_left : 0);
}

int gmu_widget_get_pos_y(GmuWidget *gw, int get_usable_pos)
{
	return gw->actual_y1 + (get_usable_pos ? gw->border_top : 0);
}
