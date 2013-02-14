/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: inputconfig.c  Created: 100306
 *
 * Description: Input devices configuration
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../wejpconfig.h"
#include "inputconfig.h"
#include "../../debug.h"

static char          *hw_button_name[MAX_BUTTONS];
static int            hw_button_val[MAX_BUTTONS];
static ActivateMethod hw_button_method[MAX_BUTTONS];
static InputType      hw_button_type[MAX_BUTTONS];
static int            number_of_buttons;
static int            has_joystick;

int input_config_init(char *inputconf_file)
{
	int        result = 0;
	ConfigFile inputconf;
	int        w;

	has_joystick = 0;
	number_of_buttons = 0;
	for (w = 0; w < MAX_BUTTONS; w++) {
		hw_button_name[w] = NULL;
		hw_button_val[w]  = 0;
	}

	wdprintf(V_INFO, "inputconfig", "Initializing. Loading %s\n", inputconf_file);
	cfg_init_config_file_struct(&inputconf);
	if (cfg_read_config_file(&inputconf, inputconf_file) == 0) {
		char key[128];
		int  e, j, ja;
		/* Load keyboard configuration ... */
		sprintf(key, "Button-0");
		for (e = 0; cfg_is_key_available(inputconf, key) && e < MAX_BUTTONS; e++) {
			char *val, *name;
			int   val_int, namelen;

			snprintf(key, 127, "Button-%d", e);
			val = cfg_get_key_value(inputconf, key);
			/* split val into keycode and name */
			if (val) {
				val_int = atoi(val);
				name = val;
				number_of_buttons++;
				while (*name != '\0' && *name++ != ',');
				namelen = strlen(name);

				/* Check for ",R" (fire on button RELEASE instead of button press) */
				if (namelen >= 2 && name[namelen-2] == ',' && name[namelen-1] == 'R') {
					hw_button_method[e] = ACTIVATE_RELEASE;
					name[namelen-2] = '\0';
					namelen = strlen(name);
				} else {
					hw_button_method[e] = ACTIVATE_PRESS;
				}

				hw_button_name[e]   = malloc(namelen+1);
				hw_button_type[e]   = INPUT_KEYBOARD;
				if (hw_button_name[e]) {
					strncpy(hw_button_name[e], name, namelen);
					hw_button_name[e][namelen] = '\0';
					hw_button_val[e] = val_int;
					/*printf("%03d: '%s' = %d (a-method = %d)\n", e, hw_button_name[e], hw_button_val[e], hw_button_method[e]);*/
				} else { /* out of memory */
					wdprintf(V_ERROR, "inputconfig", "ERROR: Out of memory!\n");
					break;
				}
			}
		}
		if (e > 0) e--;

		/* Load joystick configuration... */
		sprintf(key, "JoyButton-0");
		for (j = e; cfg_is_key_available(inputconf, key) && j < MAX_BUTTONS; j++) {
			char *val, *name;
			int   val_int, namelen;

			snprintf(key, 127, "JoyButton-%d", j-e+1);
			val = cfg_get_key_value(inputconf, key);
			if (val) {
				/* split val into keycode and name */
				val_int = atoi(val);
				name = val;

				number_of_buttons++;
				while (*name != '\0' && *name++ != ',');
				namelen = strlen(name);
				hw_button_name[j] = malloc(namelen+1);
				hw_button_type[j] = INPUT_JOYSTICK;
				if (hw_button_name[j]) {
					has_joystick = 1;
					strncpy(hw_button_name[j], name, namelen);
					hw_button_name[j][namelen] = '\0';
					hw_button_val[j] = val_int;
					/*printf("%03d: '%s' = %d\n", j, hw_button_name[j], hw_button_val[j]);*/
				} else { /* out of memory */
					wdprintf(V_ERROR, "inputconfig", "ERROR: Out of memory!\n");
					break;
				}
			}
		}
		if (j > 0) j--;

		/* Load joystick axis configuration... */
		sprintf(key, "JoyAxis-0");
		for (ja = j; cfg_is_key_available(inputconf, key) && j < MAX_BUTTONS; ja++) {
			char *val, *name;
			int   val_int, namelen;

			snprintf(key, 127, "JoyAxis-%d", ja-j);
			val = cfg_get_key_value(inputconf, key);
			if (val) {
				/* split val into keycode and name */
				val_int = atoi(val);
				name = val;

				number_of_buttons++;
				while (*name != '\0' && *name++ != ',');
				namelen = strlen(name);
				hw_button_name[ja] = malloc(namelen+1);
				hw_button_type[ja] = INPUT_JOYSTICK;
				if (hw_button_name[ja]) {
					has_joystick = 1;
					strncpy(hw_button_name[ja], name, namelen);
					hw_button_name[ja][namelen] = '\0';
					hw_button_val[ja] = val_int;
					hw_button_method[ja] = ACTIVATE_JOYAXIS_MOVE;
					/*printf("%03d: '%s' = %d\n", ja, hw_button_name[ja], hw_button_val[ja]);*/
				} else { /* out of memory */
					wdprintf(V_ERROR, "inputconfig", "ERROR: Out of memory!\n");
					break;
				}
			}
		}
		wdprintf(V_INFO, "inputconfig", "Init done.\n");
		result = 1;
	} else {
		wdprintf(V_ERROR, "inputconfig", "Failed loading input configuration file: %s.\n", inputconf_file);
		result = 0;
	}
	cfg_free_config_file_struct(&inputconf);
	return result;
}

int input_config_get_val(char *button_name, ActivateMethod *am)
{
	int w, result = 0;
	int bnl = strlen(button_name);

	for (w = 0; w < number_of_buttons; w++) {
		if (strlen(hw_button_name[w]) == bnl) {
			if (strncmp(hw_button_name[w], button_name, strlen(hw_button_name[w])) == 0) {
				/*wdprintf(V_DEBUG, "inputconfig", "Found match: %s = %s (%d)\n", hw_button_name[w], button_name, hw_button_val[w]);*/
				result = hw_button_val[w];
				if (am) *am = hw_button_method[w];
				break;
			}
		}
	}
	return result;
}

char *input_config_get_button_name(int val, InputType type)
{
	int   e;
	char *result = NULL;

	for (e = 0; hw_button_val[e] != val && e < number_of_buttons; e++);
	if (hw_button_type[e] == type && hw_button_val[e] == val) result = hw_button_name[e];
	return result;
}

int input_config_has_joystick(void)
{
	return has_joystick;
}

void input_config_free(void)
{
	int j;
	for (j = 0; j < number_of_buttons; j++)
		if (hw_button_name[j]) free(hw_button_name[j]);
	number_of_buttons = 0;
}
