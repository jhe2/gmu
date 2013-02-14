/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: inputconfig.h  Created: 100306
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

#ifndef _INPUTCONFIG_H
#define _INPUTCONFIG_H

#define MAX_BUTTONS 256

typedef enum InputType { INPUT_KEYBOARD, INPUT_JOYSTICK } InputType;
typedef enum ActivateMethod { ACTIVATE_PRESS, ACTIVATE_RELEASE, ACTIVATE_JOYAXIS_MOVE } ActivateMethod;

int   input_config_init(char *inputconf_file);
char *input_config_get_button_name(int val, InputType type);
int   input_config_get_val(char *button_name, ActivateMethod *am);
int   input_config_has_joystick(void);
void  input_config_free(void);
#endif
