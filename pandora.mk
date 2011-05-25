# 
# Gmu Music Player
#
# Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
#
# File: pandora.mk  Created: 100601
#
# Description: Makefile configuration (Pandora)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License. See the file COPYING in the Gmu's main directory
# for details.
#

#DECODERS_TO_BUILD=decoders/vorbis.so decoders/musepack.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so
#DECODERS_TO_BUILD=decoders/vorbis.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so decoders/musepack.so
DECODERS_TO_BUILD=decoders/wavpack.so decoders/mpg123.so decoders/vorbis.so
FRONTENDS_TO_BUILD=frontends/sdl.so frontends/log.so
SDL_LIB=-L/usr/local/pandora/arm-2009q3/lib -lSDL -lpthread
SDL_CFLAGS=-I/usr/local/pandora/arm-2009q3/include/SDL -I/usr/local/pandora/arm-2009q3/include -D_GNU_SOURCE=1 -D_REENTRANT
CXX=/usr/local/pandora/arm-2009q3/bin/arm-none-linux-gnueabi-g++
CC=/usr/local/pandora/arm-2009q3/bin/arm-none-linux-gnueabi-gcc
STRIP=/usr/local/pandora/arm-2009q3/bin/arm-none-linux-gnueabi-strip
COPTS?=-O3 -mtune=cortex-a8 -ffast-math -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS=-fno-strict-aliasing -fomit-frame-pointer $(SDL_CFLAGS) -I/usr/local/pandora/arm-2009q3/include -DFILE_HW_H="\"hw_$(TARGET).h\""
LFLAGS=-L/usr/local/pandora/arm-2009q3/lib -lts -lpng -ljpeg -lpthread -lm -ldl -lz -lgcc -Wl,-export-dynamic
DISTFILES=$(BINARY) frontends decoders themes gmu.png README.txt libs.pandora gmu.pandora.conf gmu-pandora.sh COPYING gmuinput.pandora.conf gmu.bmp
