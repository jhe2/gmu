/* 
 * Gmu GP2X Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: about.c  Created: 061223
 *
 * Description: Program info
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "kam.h"
#include "textbrowser.h"
#include "about.h"
#include "core.h"

static const char *text_about_gmu = 
	"Libraries used by this program:\n\n"
	"- SDL, SDL_Image, SDL_gfx\n"
	"- libjpeg+libpng used by SDL_image\n\n"
	"The decoder plugins use additional\n"
	"libraries for decoding.\n\n"
	"Program written by\n"
	"Johannes Heimansberg (**wejp.k.vu**)\n\n"
	"Please take a look at the README.txt\n"
	"file for more details and\n"
	"configuration hints. You also might\n"
	"want to check the in-program help\n"
	"screen.\n\n"
	"Project website:\n"
	"**http://wejp.k.vu/projects/gmu/**";

int about_process_action(TextBrowser *tb_about, View *view, View old_view, int user_key_action)
{
	int update = 0;
	switch (user_key_action) {
		case OKAY:
			*view = old_view;
			update = 1;
			break;
		case MOVE_CURSOR_DOWN:
			text_browser_scroll_down(tb_about);
			update = 1;
			break;
		case MOVE_CURSOR_UP:
			text_browser_scroll_up(tb_about);
			update = 1;
			break;
		case MOVE_CURSOR_LEFT:
			text_browser_scroll_left(tb_about);
			update = 1;
			break;
		case MOVE_CURSOR_RIGHT:
			text_browser_scroll_right(tb_about);
			update = 1;
			break;
	}
	return update;
}

void about_init(TextBrowser *tb_about, Skin *skin, char *decoders)
{
	static char txt[1024];

	snprintf(txt, 1023, "This is the Gmu music player.\n\n"
	                    "Version.........: **"VERSION_NUMBER"**\n"
	                    "Built on........: "__DATE__" "__TIME__"\n"
	                    "Detected device.: %s\n"
	                    "Config directory: %s\n\n"
	                    "Gmu supports various file formats\n"
	                    "through decoder plugins.\n\n"
	                    "%s decoders:\n\n%s\n"
	                    "%s",
	                    gmu_core_get_device_model_name(),
	                    gmu_core_get_config_dir(),
	                    STATIC ? "Static build with built-in" : "Loaded",
	                    decoders,
	                    text_about_gmu);

	text_browser_init(tb_about, skin);
	text_browser_set_text(tb_about, txt, "About Gmu");
}
