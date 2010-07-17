/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmuwidget.h  Created: 100120
 *
 * Description: Gmu widget
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "SDL/SDL.h"
#ifndef _GMUWIDGET_H
#define _GMUWIDGET_H

typedef enum GmuWidgetImagePosition {
	LEFT,        MIDDLE,        RIGHT,
	TOP_LEFT,    TOP_MIDDLE,    TOP_RIGHT,
	BOTTOM_LEFT, BOTTOM_MIDDLE, BOTTOM_RIGHT
} GmuWidgetImagePosition;

typedef struct _GmuWidgetImage GmuWidgetImage;

struct _GmuWidgetImage {
	SDL_Surface           *image;
	GmuWidgetImagePosition position;
	GmuWidgetImage        *next;
};

typedef struct {
	GmuWidgetImage *first;
	int             pos_x1, pos_y1;
	int             pos_x2, pos_y2;
	int             border_left, border_right, border_top, border_bottom;
	int             actual_y1, actual_x2, actual_y2;
} GmuWidget;

/* Function to initialize a new widget. This one differs from the _init function
 * in that it automatically adds the necessary images, when the follow a predefined
 * naming scheme. Missing images are ignored silently */
void gmu_widget_new(GmuWidget *gw, char *images_prefix, int x1, int y1, int x2, int y2);
/* if x2 and/or y2 are negative (or 0) the position is measured from the 
 * right/bottom border of the window (instead of left/top when positive) */
void gmu_widget_init(GmuWidget *gw, int x1, int y1, int x2, int y2);
void gmu_widget_free(GmuWidget *gw);
void gmu_widget_draw(GmuWidget *gw, SDL_Surface *target);
int  gmu_widget_get_width(GmuWidget *gw, int get_usable_size);
int  gmu_widget_get_height(GmuWidget *gw, int get_usable_size);
int  gmu_widget_get_pos_x(GmuWidget *gw, int get_usable_pos);
int  gmu_widget_get_pos_y(GmuWidget *gw, int get_usable_pos);
#endif
