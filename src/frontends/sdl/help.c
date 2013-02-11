/* 
 * Gmu GP2X Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: help.c  Created: 100404
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
#include "help.h"
#include "core.h"

static const char *text_help = 
"Welcome to the Gmu Music Player!\n\n"
"This is a short introduction to the\n"
"most important functions of Gmu.\n\n"
"Gmu is controlled by the device's buttons.\n"
"Each button can have two functions. The\n"
"primary function will be activated by\n"
"just pressing that button, while the secondary\n"
"function will be activated by pressing the\n"
"modifier key and the button at the same time.\n"
"The modifier button is the **%s **button.\n"
"When referring to it, it will be called \"Mod\"\n"
"in the following text.\n\n"
"To scroll up and down use the **%s **and\n"
"**%s **buttons.\n\n"
"Gmu has several screens displaying various\n"
"things. Its main screens are the file browser,\n"
"the playlist browser and the track info screen.\n"
"You can switch between those by pressing **%s**.\n\n"
"There are global functions which work all the\n"
"time no matter which screen you have selected\n"
"and there are screen dependent functions.\n\n"
"**Important global functions**\n\n"
"Play/Skip forward.......: **%s **\n"
"Skip backward...........: **%s **\n"
"Seek forward............: **%s **\n"
"Seek backward...........: **%s **\n"
"Pause...................: **%s **\n"
"Stop....................: **%s **\n"
"Volume up...............: **%s **\n"
"Volume down.............: **%s **\n"
"Exit Gmu................: **%s **\n"
"Program information.....: **%s **\n"
"Lock buttons+Screen off.: **%s **\n"
"Unlock buttons+Screen on: **%s **\n"
"\n"
"**File browser functions**\n\n"
"Add file/Change dir.....: **%s **\n"
"Add directory...........: **%s **\n"
"Play single file........: **%s **\n"
"New playlist from dir...: **%s **\n"
"\n"
"**Playlist browser functions**\n\n"
"Play selected item......: **%s **\n"
"Enqueue selected item...: **%s **\n"
"Remove selected item....: **%s **\n"
"Clear playlist..........: **%s **\n"
"Change play mode........: **%s **\n"
"Save/load playlist......: **%s **\n"
"\n"
"These are the most important functions. There\n"
"are some more which are typically used less\n"
"frequently. Those functions are explained in\n"
"the README.txt file which comes with Gmu.\n"
"If you wish to open this help screen at a later\n"
"time you can do so by pressing** %s**.\n\n"
"**Getting started**\n\n"
"The first thing you probably want to do is to\n"
"populate the playlist with some tracks.\n"
"This is very simple. First use **%s **to\n"
"navigate to the file browser screen. Once you\n"
"are there, use the buttons described above to\n"
"navigate within the file system tree and to add\n"
"single files or whole directories to the playlist.\n"
"Once there is at least one file in the playlist\n"
"you can start playback by pressing** %s**.\n\n"
"Have fun with the Gmu Music Player.\n"
"/wej"
;

void help_init(TextBrowser *tb_help, Skin *skin, KeyActionMapping *kam)
{
	static char txt[3072];

	snprintf(txt, 3071, text_help,
	                    key_action_mapping_get_button_name(kam, MODIFIER),
	                    key_action_mapping_get_full_button_name(kam, MOVE_CURSOR_UP),
	                    key_action_mapping_get_full_button_name(kam, MOVE_CURSOR_DOWN),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_TOGGLE_VIEW),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_NEXT),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_PREV),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_SEEK_FWD),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_SEEK_BWD),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_PAUSE),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_STOP),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_INC_VOLUME),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_DEC_VOLUME),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_EXIT),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_PROGRAM_INFO),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_LOCK),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_UNLOCK),
	                    key_action_mapping_get_full_button_name(kam, FB_ADD_FILE_TO_PL_OR_CHDIR),
	                    key_action_mapping_get_full_button_name(kam, FB_ADD_DIR_TO_PL),
	                    key_action_mapping_get_full_button_name(kam, FB_PLAY_FILE),
	                    key_action_mapping_get_full_button_name(kam, FB_NEW_PL_FROM_DIR),
	                    key_action_mapping_get_full_button_name(kam, PL_PLAY_ITEM),
	                    key_action_mapping_get_full_button_name(kam, PL_ENQUEUE),
	                    key_action_mapping_get_full_button_name(kam, PL_REMOVE_ITEM),
	                    key_action_mapping_get_full_button_name(kam, PL_CLEAR_PLAYLIST),
	                    key_action_mapping_get_full_button_name(kam, PL_TOGGLE_RANDOM),
	                    key_action_mapping_get_full_button_name(kam, PL_SAVE_PLAYLIST),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_HELP),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_TOGGLE_VIEW),
	                    key_action_mapping_get_full_button_name(kam, GLOBAL_NEXT));

	text_browser_init(tb_help, skin);
	text_browser_set_text(tb_help, txt, "Gmu Help");
}

int help_process_action(TextBrowser *tb_help, View *view, View old_view, int user_key_action)
{
	int update = 0;
	switch (user_key_action) {
		case OKAY:
			*view = old_view;
			update = 1;
			break;
		case MOVE_CURSOR_DOWN:
			text_browser_scroll_down(tb_help);
			update = 1;
			break;
		case MOVE_CURSOR_UP:
			text_browser_scroll_up(tb_help);
			update = 1;
			break;
		case MOVE_CURSOR_LEFT:
			text_browser_scroll_left(tb_help);
			update = 1;
			break;
		case MOVE_CURSOR_RIGHT:
			text_browser_scroll_right(tb_help);
			update = 1;
			break;
	}
	return update;
}
