# 
# Gmu Music Player
#
# Copyright (c) 2006-2013 Johannes Heimansberg (wejp.k.vu)
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
DECODERS_TO_BUILD=decoders/wavpack.so decoders/mpg123.so
FRONTENDS_TO_BUILD=frontends/sdl.so frontends/gmuhttp.so
SDL_LIB=-L$(ZIPIT_OPENWRT)/target-arm_v5te_uClibc-0.9.33.2_eabi/usr/lib -lSDL -lpthread
SDL_CFLAGS=-I$(ZIPIT_OPENWRT)/target-arm_v5te_uClibc-0.9.33.2_eabi/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT
CXX=arm-openwrt-linux-g++
CC=arm-openwrt-linux-gcc
STRIP=arm-openwrt-linux-strip
COPTS?=-O3 -mcpu=xscale -ffast-math
CFLAGS=-fno-strict-aliasing -fomit-frame-pointer $(SDL_CFLAGS) -I$(ZIPIT_OPENWRT)/target-arm_v5te_uClibc-0.9.33.2_eabi/usr/include -DFILE_HW_H="\"hw_$(TARGET).h\""
LFLAGS=-L$(ZIPIT_OPENWRT)/target-arm_v5te_uClibc-0.9.33.2_eabi/usr/lib -L$(ZIPIT_OPENWRT)/toolchain-arm_v5te_gcc-4.6-linaro_uClibc-0.9.33.2_eabi/lib -lpthread -lm -ldl -lz -lgcc -Wl,-export-dynamic
DISTFILES=$(COMMON_DISTBIN_FILES) gmu-z2.sh gmuinput.zipit-z2.conf
