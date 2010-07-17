/* 
 * Gmu GP2X Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: help.h  Created: 100404
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
#include "core.h"

#ifndef _HELP_H
#define _HELP_H
void help_init(TextBrowser *tb_help, Skin *skin, KeyActionMapping *kam);
int  help_process_action(TextBrowser *tb_help, View *view, View old_view, int user_key_action);
#endif
