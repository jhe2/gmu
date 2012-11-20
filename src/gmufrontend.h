/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmufrontend.h  Created: 081228
 *
 * Description: Header file for Gmu frontend plugins
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#ifndef _GMUFRONTEND_H
#define _GMUFRONTEND_H
#include "gmuevent.h"

typedef struct _GmuFrontend {
	/* Short identifier such as "sdl_frontend" */
	const char   *identifier;
	/* Should return a human-readable name such as "SDL Frontend v1.0" */
	const char * (*get_name)(void);
	/* Init function. Can be NULL if not neccessary. Will be called ONCE when 
	 * the decoder is loaded. */
	int          (*frontend_init)(void);
	/* Function to be called on unload, you should free/close everything 
	 * that is still allocated/opened at this point. Can be NULL. */
	void         (*frontend_shutdown)(void);
	/* Will be called on a regular basis (max. a few times per second) in 
	 * gmu-core's main loop */
	void         (*mainloop_iteration)(void);
	/* Will be called whenever Gmu's state has changed (e.g. playlist modified,
	 * current track changed, ...) */
	int          (*event_callback)(GmuEvent event_type, int param);
	/* internal handle, do not use */
	void         *handle;
} GmuFrontend;

/* This function must be implemented by the frontend. It must return a valid
 * GmuFrontend object */
GmuFrontend *GMU_REGISTER_FRONTEND(void);
#endif
