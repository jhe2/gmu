/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: hw_unknown.c  Created: 090629
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
#include "oss_mixer.h"
#include "debug.h"
#include "PDL.h"
#include "hw_pre.h"

static int selected_mixer = -1;

void hw_display_off(void)
{
	wdprintf(V_DEBUG, "hw_pre", "Display off requested.\n");
}

void hw_display_on(void)
{
	wdprintf(V_DEBUG, "hw_pre", "Display on requested.\n");
}

int hw_open_mixer(int mixer_channel)
{
	int res = oss_mixer_open();
	selected_mixer = mixer_channel;
	wdprintf(V_INFO, "hw_pre", "Selected mixer: %d\n", selected_mixer);
	return res;
}

void hw_close_mixer(void)
{
	oss_mixer_close();
}

void hw_set_volume(int volume)
{
	if (selected_mixer >= 0) {
		if (volume >= 0) oss_mixer_set_volume(selected_mixer, volume);
	} else {
		wdprintf(V_INFO, "hw_pre", "No suitable mixer available.\n");
	}
}

void hw_detect_device_model(void)
{
}

const char *hw_get_device_model_name(void)
{
	return "Palm Pre";
}

void hw_init_device(void)
{
	PDL_Init(0);
	PDL_NotifyMusicPlaying(PDL_TRUE);
}

void hw_sdl_post_init(void)
{
	PDL_ScreenTimeoutEnable(PDL_TRUE);
}
