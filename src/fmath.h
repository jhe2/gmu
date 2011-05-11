/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2011 Johannes Heimansberg (wejp.k.vu)
 *
 * File: fmath.h  Created: 110511
 *
 * Description: Integer math stuff
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#ifndef _FMATH_H
#define _FMATH_H

#define F_PI2 6283
#define F_PI  3142

/* Functions return results as value multiplied by 10000, 
 * Arguments have to be multiplied by 1000 */
int fsin(int x);
int fcos(int x);
#endif
