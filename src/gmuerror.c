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

#include <stdlib.h>
#include "gmuerror.h"

/*
 * The number of error message strings must match the number of items
 * in GmuError enum in gmuerror.h minus one
 */
static const char *gmu_error_message[] = {
	"Unknown error",
	"ERROR: Cannot open audio device",
	"ERROR: Cannot open file"
};

const char *gmu_error_get_message(GmuError error)
{
	return error < GMU_ERROR_END_MARKER ? gmu_error_message[error] : NULL;
}
