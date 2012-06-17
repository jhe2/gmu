# 
# Gmu Music Player
#
# Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
#
# File: dingux.mk  Created: 060904
#
# Description: Makefile configuration (Dingoo A320/A330 running Dingux)
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
SDL_LIB=-lSDL -lpthread -lpng -ljpeg
SDL_CFLAGS=-I/usr/include/SDL -I/opt/opendingux-toolchain/usr/include/ -D_GNU_SOURCE=1 -D_REENTRANT
CXX=mipsel-linux-g++
CC=mipsel-linux-gcc
STRIP=mipsel-linux-strip
COPTS?=-O3 -ffast-math
CFLAGS=-fomit-frame-pointer $(SDL_CFLAGS) -I/opt/opendingux-toolchain/usr/include/ -DFILE_HW_H="\"hw_$(TARGET).h\""
LFLAGS=-s -L/opt/opendingux-toolchain/usr/lib/ -lpthread -lm -ldl -lz -lgcc -Wl,-export-dynamic
DISTFILES=$(BINARY) frontends decoders themes gmu.png README.txt libs.dingoo gmu.dge COPYING gmuinput.dingux.conf gmu.bmp
