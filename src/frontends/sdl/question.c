/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: question.c  Created: 061130
 *
 * Description: Question dialog
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include "question.h"
#include "skin.h"
#include "debug.h"

void question_init(Question *dlg, Skin *skin)
{
	dlg->skin = skin;
}

int question_process_action(Question *dlg, int user_key_action)
{
	int result = 0;
	switch (user_key_action) {
		case QUESTION_YES:
			wdprintf(V_DEBUG, "sdl_question", "Answer: YES\n");
			dlg->positive_result_func(dlg->arg);
			result = 1;
			*(dlg->view) = dlg->return_view;
			break;
		case QUESTION_NO:
			wdprintf(V_DEBUG, "sdl_question", "Answer: NO\n");
			*(dlg->view) = dlg->return_view;
			break;
	}
	return result;
}

void question_draw(Question *dlg, SDL_Surface *sdl_target)
{
	skin_draw_header_text(dlg->skin, "Question", sdl_target);
	textrenderer_draw_string(&dlg->skin->font1, dlg->question, sdl_target,
	                         gmu_widget_get_pos_x((GmuWidget *)&dlg->skin->lv, 1),
	                         gmu_widget_get_pos_y((GmuWidget *)&dlg->skin->lv, 1) + 
	                         gmu_widget_get_height((GmuWidget *)&dlg->skin->lv, 1) / 2 - dlg->skin->font1_char_height);
}

void question_set(Question *dlg, View return_view, View *view, char *question,
                  void (*positive_result_func)(void *), void *arg)
{
	dlg->view = view;
	dlg->return_view = return_view;
	dlg->question = question;
	dlg->positive_result_func = positive_result_func;
	dlg->arg = arg;
}
