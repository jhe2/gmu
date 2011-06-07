/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: hw_unknown.h  Created: 090629
 *
 * Description: Hardware specific header file for unknown devices (such as PCs)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#ifndef _HW_PRE_H
#define _HW_PRE_H

#define HW_COLOR_DEPTH 32
#define HW_SCREEN_WIDTH 480
#define HW_SCREEN_HEIGHT 360

#define SAMPLE_BUFFER_SIZE 4096

#define MAX_COVER_IMAGE_PIXELS 400000

#define SHOW_MOUSE_CURSOR 1

int           hw_open_mixer(int mixer_channel);
void          hw_close_mixer(void);
void          hw_set_volume(int volume);
void          hw_display_off(void);
void          hw_display_on(void);
void          hw_detect_device_model(void);
const char   *hw_get_device_model_name(void);
#define       HW_INIT_DEVICE 1
void          hw_init_device(void);
#define       HW_SDL_POST_INIT 1
void          hw_sdl_post_init(void);
#endif
