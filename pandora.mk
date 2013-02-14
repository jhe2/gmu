# 
# Gmu Music Player
#
# Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
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
DECODERS_TO_BUILD=decoders/wavpack.so decoders/mpg123.so decoders/vorbis.so decoders/speex.so decoders/mikmod.so
FRONTENDS_TO_BUILD=frontends/sdl.so frontends/log.so frontends/gmuhttp.so
SDL_LIB=-L$(PNDSDK)/usr/lib -lSDL -lpthread
SDL_CFLAGS=-I$(PNDSDK)/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT
CXX=$(PNDSDK)/bin/arm-none-linux-gnueabi-g++
CC=$(PNDSDK)/bin/arm-none-linux-gnueabi-gcc
STRIP=$(PNDSDK)/bin/arm-none-linux-gnueabi-strip
COPTS?=-O3 -mtune=cortex-a8 -ffast-math -march=armv7-a -mfpu=neon -mfloat-abi=softfp
CFLAGS=-fno-strict-aliasing -fomit-frame-pointer $(SDL_CFLAGS) -I$(PNDSDK)/usr/include/ncursesw -I$(PNDSDK)/usr/include
LFLAGS=-s -L$(PNDSDK)/usr/lib -lts -lpng -ljpeg -lpthread -lm -ldl -lz -lgcc -Wl,-export-dynamic
DISTFILES=$(COMMON_DISTBIN_FILES) gmuc gmu.pandora.conf gmuc.pandora.conf gmu-pandora.sh gmuc-pandora.sh gmuc-terminal-pandora.sh gmuinput.pandora.conf

DESTDIR=tmp/
PREFIX=pndgmu

DISBIN_DEPS=pnd

pnd: all distbin
	mkdir -p tmp
	-rm -rf tmp/$(projname)-$(TARGET)
	-rm -rf tmp/gmu
	unzip $(projname)-$(TARGET).zip -d tmp
	mv tmp/$(projname)-$(TARGET) tmp/gmu
	cp PXML.xml tmp/gmu
	$(PNDSDK)/../sdk_utils/pnd_make.sh -p $(projname).pnd -i gmu.png -d `pwd`/tmp/gmu
