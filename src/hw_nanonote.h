/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: hw_nanonote.h  Created: 100111
 *
 * Description: Ben NanoNote specific stuff (button mapping etc.)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _HW_NANONOTE_H
#define _HW_NANONOTE_H

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <unistd.h>

#define HW_COLOR_DEPTH 32
#define HW_SCREEN_WIDTH 320
#define HW_SCREEN_HEIGHT 240

#define SAMPLE_BUFFER_SIZE 4096

#define MAX_COVER_IMAGE_PIXELS 400000

int         hw_open_mixer(int mixer_channel);
void        hw_close_mixer(void);
void        hw_set_volume(int volume);
void        hw_display_off(void);
void        hw_display_on(void);
void        hw_detect_device_model(void);
const char *hw_get_device_model_name(void);
#endif
