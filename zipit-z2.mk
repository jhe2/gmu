# 
# Gmu Music Player
#
# Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
#
# File: zipit-z2.mk  Created: 100829
#
# Description: Makefile configuration Zipit Z2
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License. See the file COPYING in the Gmu's main directory
# for details.
#

#DECODERS_TO_BUILD=decoders/vorbis.so decoders/musepack.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so
DECODERS_TO_BUILD=decoders/wavpack.so decoders/vorbis.so decoders/mpg123.so decoders/flac.so decoders/speex.so
FRONTENDS_TO_BUILD=frontends/sdl.so frontends/log.so
SDL_LIB=-L/opt/crossdev/z2/lib -lSDL -lpthread
SDL_CFLAGS=-I/home/wejp/crossdev/z2/buildroot/output/staging/usr/include/SDL -I/home/wejp/crossdev/z2/buildroot/output/staging/usr/include -D_GNU_SOURCE=1 -D_REENTRANT
#CXX=arm-unknown-linux-gnu-g++
#CC=arm-unknown-linux-gnu-gcc
CXX=/home/wejp/crossdev/z2/buildroot/output/staging/usr/bin/arm-unknown-linux-uclibcgnueabi-g++
CC=/home/wejp/crossdev/z2/buildroot/output/staging/usr/bin/arm-unknown-linux-uclibcgnueabi-gcc
STRIP=/home/wejp/crossdev/z2/buildroot/output/staging/usr/bin/arm-unknown-linux-uclibcgnueabi-strip
COPTS?=-O3 -mcpu=xscale -ffast-math
CFLAGS=-fno-strict-aliasing -fomit-frame-pointer $(SDL_CFLAGS) -I/usr/local/pandora/arm-2009q3/include -DFILE_HW_H="\"hw_$(TARGET).h\""
LFLAGS=-L/home/wejp/crossdev/z2/buildroot/output/staging/usr/lib -lpthread -lm -ldl -lz -lgcc -Wl,-export-dynamic
DISTFILES=$(BINARY) frontends decoders themes gmu.png README.txt libs.z2 gmu-z2.sh COPYING gmuinput.z2.conf gmu.bmp
