/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmuevent.h  Created: 110427
 *
 * Description: Gmu events header file
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#ifndef _GMUEVENT_H
#define _GMUEVENT_H
typedef enum GmuEvent {
	GMU_NO_EVENT,
	GMU_PLAYLIST_CHANGE, GMU_QUEUE_CHANGE, GMU_PLAYLIST_CLEAR,
	GMU_TRACK_CHANGE, GMU_PLAYBACK_STATE_CHANGE,
	GMU_PLAYMODE_CHANGE, GMU_QUIT, GMU_VOLUME_CHANGE,
	GMU_TRACKINFO_CHANGE, GMU_BUFFER_EMPTY,
	GMU_FILE_ERROR, GMU_NETWORK_ERROR,
	GMU_BUFFERING, GMU_BUFFERING_FAILED, GMU_BUFFERING_DONE,
	GMU_PLAYBACK_TIME_CHANGE
} GmuEvent;
#endif
