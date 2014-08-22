/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2014 Johannes Heimansberg (wejp.k.vu)
 *
 * File: gmuerror.h  Created: 140822
 *
 * Description: Gmu errors header file
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#ifndef _GMUERROR_H
#define _GMUERROR_H
typedef enum GmuError {
	GMU_ERROR_UNKNOWN = 0,
	GMU_ERROR_CANNOT_OPEN_AUDIO_DEVICE, GMU_ERROR_CANNOT_OPEN_FILE,
	GMU_ERROR_END_MARKER
} GmuError;

const char *gmu_error_get_message(GmuError error);
#endif
