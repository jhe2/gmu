# 
# Gmu Music Player
#
# Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
#
# File: gp2xwiz.mk  Created: 060904
#
# Description: Makefile configuration (GP2X [Open2X] and Wiz)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License. See the file COPYING in the Gmu's main directory
# for details.
#

#DECODERS_TO_BUILD=decoders/vorbis.so decoders/musepack.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so
DECODERS_TO_BUILD=decoders/vorbis.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so decoders/musepack.so
FRONTENDS_TO_BUILD=frontends/sdl.so frontends/log.so
DEVICE=GP2X
CONFIG=-D_$(DEVICE) -D_OPEN2X
SDL_LIB=-L/opt/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/lib -lSDL -lpthread
SDL_CFLAGS=-I/usr/include/SDL -I/opt/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/arm-open2x-linux/include -I/opt/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/lib/include -D_GNU_SOURCE=1 -D_REENTRANT
#CXX=arm-unknown-linux-gnu-g++
#CC=arm-unknown-linux-gnu-gcc
CXX=/opt/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/bin/arm-open2x-linux-g++
CC=/opt/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/bin/arm-open2x-linux-gcc
STRIP=/opt/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/bin/arm-open2x-linux-strip
COPTS?=-O3 -mtune=arm920t -ffast-math 
CFLAGS=-fno-strict-aliasing -fomit-frame-pointer $(SDL_CFLAGS) -I/opt/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/include $(CONFIG)
LFLAGS=-L/opt/open2x/gcc-4.1.1-glibc-2.3.6/arm-open2x-linux/lib -lSDL_image -lSDL_gfx $(SDL_LIB) -lpng -ljpeg -lpthread -lm -ldl -lz -lgcc -Wl,-export-dynamic
DISTFILES=$(BINARY) frontends decoders themes gmu.png README.txt libs.wiz wiz.keymap gp2x.keymap gmu.wiz.conf gmu.gp2x.conf gmu-wiz.gpu gmu-gp2x.gpu COPYING gmuinput.gp2x.conf gmuinput.wiz.conf
