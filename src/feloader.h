/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
 *
 * File: feloader.h  Created: 081228
 *
 * Description: Shared object decoader loader for Gmu frontend plugins
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _FELOADER_H
#define _FELOADER_H
#include "gmufrontend.h"

typedef struct _FrontendChainElement FrontendChainElement;

struct _FrontendChainElement {
	FrontendChainElement *next;
	GmuFrontend          *gf;
};

GmuFrontend *feloader_load_frontend(char *so_file);
/* Loads a frontend and adds it to the frontend chain: */
int          feloader_load_single_frontend(char *so_file);
int          feloader_load_all(char *directory);
GmuFrontend *feloader_frontend_list_get_next_frontend(int getfirst);
void         feloader_free(void);
int          feloader_unload_frontend(GmuFrontend *gf);
int          feloader_load_builtin_frontends(void);
#endif
