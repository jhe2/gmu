/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: about.h  Created: 061223
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

#ifndef _ABOUT_H
#define _ABOUT_H
int  about_process_action(TextBrowser *tb_about, View *view, View old_view, int user_key_action);
void about_init(TextBrowser *tb_about, Skin *skin, char *decoders);
#endif
