/* 
 * Gmu Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: oss_mixer.c  Created: 090630
 *
 * Description: OSS audio mixer functions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of
 * the License. See the file COPYING in the Gmu's main directory
 * for details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include "oss_mixer.h"

static int mixer_device = 0;
static int initial_volume_pcm = 0, initial_volume_master = 0;
static int available_mixers[SOUND_MIXER_NRDEVICES];

int oss_mixer_open(void)
{
	int result = 1;

  	if ((mixer_device = open("/dev/mixer", O_RDWR)) < 0) {
  		result = 0;
		printf("oss_mixer: Failed to open mixer.\n");
	} else {
		int mask;

		if (ioctl(mixer_device, SOUND_MIXER_READ_DEVMASK, &mask) == -1) {
			printf("oss_mixer: No mixers available.\n");
		} else {
			int   i, p = 0;
			char *dn[] = SOUND_DEVICE_NAMES;

			printf("oss_mixer: Available controls: ");
			for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
				if (mask & (1 << i)) {
					if (p) printf(", ");
					printf("%s (%d)", dn[i], i);
					p = 1;
					available_mixers[i] = 1;
				} else {
					available_mixers[i] = 0;
				}
			if (!p) printf("None");
			printf(".\n");
		}
		ioctl(mixer_device, SOUND_MIXER_READ_PCM, &initial_volume_pcm);
		ioctl(mixer_device, SOUND_MIXER_READ_VOLUME, &initial_volume_master);
		printf("oss_mixer: Volume settings: master=%d pcm=%d\n",
		       initial_volume_master, initial_volume_pcm);
	}
	return result;
}

void oss_mixer_close(void)
{
	ioctl(mixer_device, SOUND_MIXER_WRITE_PCM, &initial_volume_pcm);
	close(mixer_device);
}

void oss_mixer_set_volume(int mixer, int volume)
{
	int l = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
	int r = volume < 0 ? 0 : (volume > 100 ? 100 : volume);

	l <<= 8; l |= r;
	if (1 || available_mixers[mixer]) { /* Ignore non-existance of mixers (Dingoo workaround) */
		ioctl(mixer_device, MIXER_WRITE(mixer), &l);
		printf("oss_mixer: mixer %d volume=%d\n", mixer, volume);
	} else {
		printf("oss_mixer: No such mixer: %d\n", mixer);
	}
}

int oss_mixer_get_volume(int mixer)
{
	int val = 0;

	if (available_mixers[mixer])
		ioctl(mixer_device, MIXER_READ(mixer), &val);
	return val;
}

int oss_mixer_is_mixer_available(int mixer)
{
	return (mixer < SOUND_MIXER_NRDEVICES ? available_mixers[mixer] : 0);
}
