/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: kam.h  Created: 061102
 *
 * Description: Key mapping stuff
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _KAM_H
#define _KAM_H
#include "inputconfig.h"

typedef enum _View
{
	ANY = 0, FILE_BROWSER = 1, PLAYLIST = 2, ABOUT = 4, HELP = 8, 
	TRACK_INFO = 16, QUESTION = 32, SETUP = 64, 
	SETUP_FILE_BROWSER = 128, PLAYLIST_SAVE = 256, EGG = 512
} View;

typedef enum _UserAction
{
	FB_ADD_FILE_TO_PL_OR_CHDIR,
	FB_ADD_DIR_TO_PL,
	FB_INSERT_FILE_INTO_PL,
	FB_PLAY_FILE,
	FB_DELETE_FILE,
	FB_NEW_PL_FROM_DIR,
	PL_PLAY_ITEM,
	PL_REMOVE_ITEM,
	PL_CLEAR_PLAYLIST,
	PL_TOGGLE_RANDOM,
	PL_DELETE_FILE,
	PL_SAVE_PLAYLIST,
	PL_ENQUEUE,
	PLMANAGER_SELECT,
	PLMANAGER_LOAD,
	PLMANAGER_LOAD_APPEND,
	PLMANAGER_CANCEL,
	OKAY,
	SETUP_SELECT,
	SETUP_SAVE_AND_EXIT,
	SETUP_SAVE_AND_RUN_GMU,
	SETUP_FB_SELECT,
	SETUP_FB_CHDIR,
	SETUP_FB_CANCEL,
	TRACKINFO_TOGGLE_COVER,
	TRACKINFO_TOGGLE_TEXT,
	TRACKINFO_DELETE_FILE,
	QUESTION_YES,
	QUESTION_NO,
	GLOBAL_PROGRAM_INFO,
	GLOBAL_HELP,
	GLOBAL_EXIT,
	GLOBAL_FULLSCREEN,
	GLOBAL_LOCK,
	GLOBAL_UNLOCK,
	GLOBAL_TOGGLE_VIEW,
	GLOBAL_PAUSE,
	GLOBAL_STOP,
	GLOBAL_NEXT,
	GLOBAL_PREV,
	GLOBAL_SEEK_FWD,
	GLOBAL_SEEK_BWD,
	GLOBAL_INC_VOLUME,
	GLOBAL_DEC_VOLUME,
	GLOBAL_TOGGLE_TIME,
	GLOBAL_SET_SHUTDOWN_TIMER,
	FB_DIR_UP,
	FB_CHDIR,
	MOVE_CURSOR_UP,
	MOVE_CURSOR_DOWN,
	MOVE_CURSOR_LEFT,
	MOVE_CURSOR_RIGHT,
	PAGE_UP,
	PAGE_DOWN,
	MODIFIER,
	LAST_ACTION
} UserAction;

#define BUTTON_NAME_MAX_LENGTH 32

typedef struct KeyActionMapping
{
	int            button;
	ActivateMethod method;
	int            modifier;
	View           scope;
	char           button_name[BUTTON_NAME_MAX_LENGTH];
	char          *description;
} KeyActionMapping;

void  key_action_mapping_init(KeyActionMapping *kam);
void  key_action_mapping_set_defaults(KeyActionMapping *kam);
int   key_action_mapping_load_config(KeyActionMapping *kam, char *keymap_file);
int   key_action_mapping_get_action(KeyActionMapping *kam, int button, int modifier, View view, ActivateMethod amethod);
char *key_action_mapping_get_button_name(KeyActionMapping *kam, UserAction action);
char *key_action_mapping_get_full_button_name(KeyActionMapping *kam, UserAction action);
int   key_action_mapping_get_modifier(KeyActionMapping *kam, UserAction action);
void  key_action_mapping_generate_help_string(KeyActionMapping *kam, char *target, int target_size, int modifier, View view);
#endif
