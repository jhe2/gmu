/* 
 * Gmu GP2X Music Player
 *
 * Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
 *
 * File: hw_gp2x.c  Created: ?
 *
 * Description: GP2X specific stuff (button mapping etc.)
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
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include "hw_gp2xwiz.h"
#include "oss_mixer.h"
#include "debug.h"
#define SYS_CLK_FREQ 7372800

static int memfd;

static volatile unsigned long  *memregs32;
static volatile unsigned short *memregs16;
static GP2XModel                gp2x_model = MODEL_UNKNOWN;
static int                      selected_mixer = -1;

static void set_cpu_clock(unsigned int MHZ)
{
	unsigned int v, l;
	unsigned int mdiv, pdiv = 3, scale = 0;
	MHZ *= 1000000;
	mdiv = (MHZ * pdiv) / SYS_CLK_FREQ;

	mdiv = ((mdiv - 8) << 8) & 0xff00;
	pdiv = ((pdiv - 2) << 2) & 0xfc;
	scale &= 3;
	v = mdiv | pdiv | scale;

	l = memregs32[0x808>>2];            /* Get interupt flags                */
	memregs32[0x808>>2] = 0xFF8FFFE7;   /* Turn off interrupts               */
	memregs16[0x910>>1] = v;            /* Set frequency                     */
	while (memregs16[0x0902 >> 1] & 1); /* Wait for the frequency to be used */
	memregs32[0x808>>2] = l;            /* Turn on interrupts                */
}

unsigned char gp2x_init_phys(void)
{
	memfd = open("/dev/mem", O_RDWR);
	if (memfd == -1) {
		wdprintf(V_WARNING, "hw_gp2xwiz", "Open failed for /dev/mem\n");
		return 0;
	}

	memregs32 = (unsigned long*) mmap(0, 0x10000, PROT_READ|PROT_WRITE, 
	                                  MAP_SHARED, memfd, 0xc0000000);

	if (memregs32 == (unsigned long *)0xFFFFFFFF) return 0;
	memregs16 = (unsigned short *)memregs32;
	wdprintf(V_DEBUG, "hw_gp2xwiz", "init phys.\n");
	return 1;
}

void gp2x_close_phys(void)
{
	close(memfd);
	wdprintf(V_DEBUG, "hw_gp2xwiz", "close phys.\n");
}

void gp2x_set_cpu_clock(unsigned int MHz)
{
	wdprintf(V_INFO, "hw_gp2xwiz", "CPU Clock = %d MHz\n", MHz);
	if (!gp2x_init_phys()) return;
	set_cpu_clock(MHz);
	gp2x_close_phys();
}

void hw_display_off(void)
{
	if (!gp2x_init_phys()) return;

	switch (gp2x_get_model()) {
		case MODEL_F100:
			/* Turn off backlight */
			memregs16[0x106E >> 1] &= ~0x0004;
			/* Turn off LCD controller */
			/*memregs16[0x2800>>1] &=~0x0001;*/
			/* Set LCD to sleep mode */
			/*memregs16[0x1062>>1] |= 0x0004;*/
			break;
		case MODEL_F200:
			/* Turn off backlight */
			memregs16[0x1076 >> 1] &= ~0x0800;
			break;
		default:
			break;
	}
	gp2x_close_phys();
	wdprintf(V_DEBUG, "hw_gp2xwiz", "Display off.\n");
}

void hw_display_on(void)
{
	if (!gp2x_init_phys()) return;

	switch (gp2x_get_model()) {
		case MODEL_F100:
			/* Set LCD from sleep mode */
			/*memregs16[0x1062>>1] &=~0x0004;*/
			/* Turn on LCD controller */
			/*memregs16[0x2800>>1] |= 0x0001;*/
			/* Turn on backlight */
			memregs16[0x106E >> 1] |= 0x0004;
			break;
		case MODEL_F200:
			/* Turn on backlight */
			memregs16[0x1076 >> 1] |= 0x0800;
			break;
		default:
			break;
	}
	gp2x_close_phys();
	wdprintf(V_DEBUG, "hw_gp2xwiz", "Display on.\n");
}

int hw_open_mixer(int mixer_channel)
{
	int res = oss_mixer_open();
	selected_mixer = mixer_channel;
	wdprintf(V_INFO, "hw_gp2xwiz", "Selected mixer: %d\n", selected_mixer);
	return res;
}

void hw_close_mixer(void)
{
	oss_mixer_close();
}

void hw_set_volume(int volume)
{
	if (selected_mixer >= 0) {
		if (volume >= 0) oss_mixer_set_volume(selected_mixer, volume);
	} else {
		wdprintf(V_INFO, "hw_gp2xwiz", "No suitable mixer available.\n");
	}
}

void hw_detect_device_model(void)
{
	GP2XModel model = MODEL_UNKNOWN;
	int       fd;

	/* Check for GP2X-F200 */
	if ((fd = open("/dev/touchscreen/wm97xx", O_RDONLY)) == -1) {
		/* Check for GP2X Wiz */
		if ((fd = open("/dev/pollux_serial0", O_RDONLY)) == -1) {
			model = MODEL_F100;
		} else {
			model = MODEL_WIZ;
			close(fd);
		}
	} else {
		model = MODEL_F200;
		close(fd);
	}
	gp2x_model = model;
}

GP2XModel gp2x_get_model(void)
{
	return gp2x_model;
}

const char *hw_get_device_model_name(void)
{
	char *result = NULL;
	switch (gp2x_model) {
		case MODEL_UNKNOWN:
			result = "Unknown device";
			break;
		case MODEL_F100:
			result = "GP2X-F100";
			break;
		case MODEL_F200:
			result = "GP2X-F200";
			break;
		case MODEL_WIZ:
			result = "GP2X Wiz";
			break;
	}
	return result;
}
