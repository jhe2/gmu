# 
# Gmu Music Player
#
# Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
#
# File: caanoo.mk  Created: 130214
#
# Description: Makefile configuration
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License. See the file COPYING in the Gmu's main directory
# for details.
#

#DECODERS_TO_BUILD=decoders/vorbis.so decoders/musepack.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so
DECODERS_TO_BUILD=decoders/wavpack.so decoders/mpg123.so
FRONTENDS_TO_BUILD=frontends/sdl.so
TOOLS_TO_BUILD=
DEVICE=Caanoo

SDL_LIB=-L$(CAANOO_SDK)/arm-gph-linux-gnueabi/sys-root/usr/lib -lSDL -lpthread
SDL_CFLAGS=-I$(CAANOO_SDK)/arm-gph-linux-gnueabi/sys-root/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT
CXX=arm-gph-linux-gnueabi-g++
CC=arm-gph-linux-gnueabi-gcc
STRIP=arm-gph-linux-gnueabi-strip
COPT=-O3 -Wall -ffast-math -fomit-frame-pointer -I$(CAANOO_SDK)/arm-gph-linux-gnueabi/sys-root/usr/include -D_$(DEVICE)
CFLAGS=-Os $(SDL_CFLAGS)
LFLAGS=-L$(CAANOO_SDK)/arm-gph-linux-gnueabi/sys-root/usr/lib $(SDL_LIB) -lpng -ljpeg -lpthread -lm -ldl -lz -Wl,-export-dynamic
DISTFILES=$(BINARY) frontends decoders themes gmu.png README.txt libs.caanoo gmu-caanoo.gpu COPYING gmuinput.caanoo.conf


distbin_caanoo: default_distbin
	$(Q)echo '[info]'>gmu.ini
	$(Q)echo 'name="Gmu Music player"'>>gmu.ini
	$(Q)echo 'path="/$(projname)-$(TARGET)/gmu-caanoo.gpu"'>>gmu.ini
	$(Q)echo 'group="APPS"'>>gmu.ini
	$(Q)zip "$(projname)-$(TARGET).zip" gmu.ini
	$(Q)rm gmu.ini
