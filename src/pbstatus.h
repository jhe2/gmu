/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: pbstatus.h  Created: ?
 *
 * Description: playback status enumeration
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _PBSTATUS_H
#define _PBSTATUS_H
enum _PB_Status { STOPPED, PLAYING, PAUSED, FINISHED };

typedef enum _PB_Status PB_Status;
#endif
