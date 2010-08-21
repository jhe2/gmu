/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2009 Johannes Heimansberg (wejp.k.vu)
 *
 * File: hw.c  Created: 090629
 *
 * Description: Hardware abstraction header file
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */

#ifdef _GP2X
#include "hw_gp2xwiz.h"
#endif
#ifdef _DINGOO
#include "hw_dingux.h"
#endif
#ifdef _NANONOTE
#include "hw_nanonote.h"
#endif
#ifdef _PANDORA
#include "hw_pandora.h"
#endif
#ifdef _ZIPIT_Z2
#include "hw_zipit-z2.h"
#endif
#ifdef _UNKNOWN_SYSTEM
#include "hw_unknown.h"
#endif
