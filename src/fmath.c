/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: fmath.c  Created: 110511
 *
 * Description: Integer math stuff
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdio.h>
#include "fmath.h"
#include "fmath_tables.h"

int fsin(int x)
{
	return _sin[x % 6283];
}

int fcos(int x)
{
	return _cos[x % 6283];
}
