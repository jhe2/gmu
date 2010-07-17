/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: question.h  Created: 061130
 *
 * Description: Question dialog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "kam.h"
#include "textrenderer.h"
#include "skin.h"
#ifndef _QUESTION_H
#define _QUESTION_H
typedef struct Question {
	char *question;
	View  return_view;
	View *view;
	Skin *skin;
	void (*positive_result_func)(void *);
	void *arg;
} Question;

void question_init(Question *dlg, Skin *skin);
int  question_process_action(Question *dlg, int user_key_action);
void question_draw(Question *dlg, SDL_Surface *sdl_target);
void question_set(Question *dlg, View return_view, View *view, char *question,
                  void (*positive_result_func)(void *), void *arg);
#endif
