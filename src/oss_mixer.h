/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: oss_mixer.h  Created: 090630
 *
 * Description: OSS audio mixer functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#ifndef _OSS_MIXER_H
#define _OSS_MIXER_H
#include <sys/soundcard.h>

int  oss_mixer_open(void);
void oss_mixer_close(void);
void oss_mixer_set_volume(int mixer, int volume);
int  oss_mixer_get_volume(int mixer);
int  oss_mixer_is_mixer_available(int mixer);
#endif
