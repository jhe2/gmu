/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2023 Johannes Heimansberg (wej.k.vu)
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
	"- SDL, SDL_Image, SDL_gfx (optional)\n\n"
	"The decoder plugins use additional\n"
	"libraries for decoding.\n\n"
	"Program written by\n"
	"Johannes Heimansberg (**wej.k.vu**)\n\n"
	"Please take a look at the README.md\n"
	"file for more details and\n"
	"configuration hints. You also might\n"
	"want to check the in-program help\n"
	"screen.\n\n"
	"Project website:\n"
	"**http://wej.k.vu/projects/gmu/**\n\n"
	"Gmu is free software: You can\n"
	"redistribute it and/or modify it under\n"
	"the terms of the GNU General Public\n"
	"License version 2.\n";

int about_process_action(TextBrowser *tb_about, View *view, View old_view, int user_key_action)
{
	int update = 0;
	switch (user_key_action) {
		case OKAY:
			*view = old_view;
			update = 1;
			break;
		case RUN_SETUP:
			*view = SETUP;
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

void about_init(TextBrowser *tb_about, Skin *skin, const char *decoders)
{
	static char txt[1024];
	SDL_version compiled;
	const SDL_version *linked;

	SDL_VERSION(&compiled);
	linked = SDL_Linked_Version();

	snprintf(
		txt, 1023, "This is the Gmu music player.\n\n"
		"Version.........: **"VERSION_NUMBER"**\n"
		"Built on........: "__DATE__" "__TIME__"\n"
		"SDL version.....: %u.%u.%u (runtime: %u.%u.%u)\n"
		"Detected device.: %s\n"
		"Config directory: %s\n"
		"Config file.....: %s\n"
		"Command line....: %s\n\n"
		"Gmu supports various file formats\n"
		"through decoder plugins.\n\n"
		"%s decoders:\n\n%s\n"
		"%s",
		compiled.major, compiled.minor, compiled.patch,
		linked->major, linked->minor, linked->patch,
		gmu_core_get_device_model_name(),
		gmu_core_get_config_dir(),
		gmu_core_get_config_file_path(),
		gmu_core_get_command_line(),
		STATIC ? "Static build with built-in" : "Loaded",
		decoders,
		text_about_gmu
	);

	text_browser_init(tb_about, skin);
	text_browser_set_text(tb_about, txt, "About Gmu");
}
