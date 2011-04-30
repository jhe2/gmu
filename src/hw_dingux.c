/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: hw_dingoo.c  Created: 090628
 *
 * Description: Dingoo specific stuff (button mapping etc.)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include "hw_dingux.h"
#include "oss_mixer.h"
#include "debug.h"

static int display_on_value = 100;
static int selected_mixer = -1;

int hw_open_mixer(int mixer_channel)
{
	int res = oss_mixer_open();
	selected_mixer = mixer_channel;
	wdprintf(V_INFO, "hw_dingux", "Selected mixer: %d\n", selected_mixer);
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
		wdprintf(V_INFO, "hw_dingux", "No suitable mixer available.\n");
	}
}

void hw_display_off(void)
{
	char  tmp[5];
	FILE *f;
	int   display_on_value_tmp = 0;

	wdprintf(V_DEBUG, "hw_dingux", "Display off requested.\n");
	if ((f = fopen("/proc/jz/lcd_backlight", "r"))) { /* Old kernel */
		if (fgets(tmp, 4, f)) display_on_value_tmp = atoi(tmp);
		fclose(f);
		if (display_on_value_tmp > 0) display_on_value = display_on_value_tmp;
		if ((f = fopen("/proc/jz/lcd_backlight", "w"))) {
			fprintf(f, "0\n");
			fclose(f);
		}
	} else { /* new OpenDingux kernel */
		if ((f = fopen("/sys/devices/platform/jz4740-slcd-fb/graphics/fb0/blank", "w"))) {
			fprintf(f, "1\n");
			fclose(f);
		} else if ((f = fopen("/sys/class/graphics/fb0/blank", "w"))) { /* newer OpenDingux kernel */
			fprintf(f, "1\n");
			fclose(f);
		}
		display_on_value = -1;
	}
}

void hw_display_on(void)
{
	FILE *f;

	wdprintf(V_DEBUG, "hw_dingux", "Display on requested.\n");
	if (display_on_value > 0) { /* Old kernel */
		if ((f = fopen("/proc/jz/lcd_backlight", "w"))) {
			fprintf(f, "%d\n", display_on_value);
			fclose(f);
		}
	} else { /* new OpenDingux kernel */
		if ((f = fopen("/sys/devices/platform/jz4740-slcd-fb/graphics/fb0/blank", "w"))) {
			fprintf(f, "0\n");
			fclose(f);
		} else if ((f = fopen("/sys/class/graphics/fb0/blank", "w"))) { /* newer OpenDingux kernel */
			fprintf(f, "0\n");
			fclose(f);
		}
	}
}

void hw_detect_device_model(void)
{
}

const char *hw_get_device_model_name(void)
{
	return "Dingoo A320";
}
