# 
# Gmu Music Player
#
# Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
#
# File: unknown.mk  Created: 060904
#
# Description: Makefile configuration (unknown default target)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License. See the file COPYING in the Gmu's main directory
# for details.
#

#DECODERS_TO_BUILD=decoders/vorbis.so decoders/musepack.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so
DECODERS_TO_BUILD=decoders/vorbis.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so decoders/musepack.so decoders/speex.so
FRONTENDS_TO_BUILD=frontends/sdl.so frontends/log.so frontends/gmusrv.so
CC=gcc
CXX=g++
STRIP=strip

SDL_LIB=$(shell sdl-config --libs)
SDL_CFLAGS=$(shell sdl-config --cflags)
MIKMOD_LIB=$(shell libmikmod-config --libs)
MIKMOD_CFLAGS=$(shell libmikmod-config --cflags)

COPTS?=-O2 -funroll-all-loops -finline-functions -ffast-math -pg -g
CFLAGS=$(SDL_CFLAGS) -fsigned-char -D_REENTRANT -DUSE_MEMORY_H -D_$(DEVICE) -DFILE_HW_H="\"hw_$(TARGET).h\""
#-fprofile-arcs -ftest-coverage
LFLAGS=-I/usr/local/include -L/usr/local/lib $(SDL_LIB) -lSDL_image -lSDL_gfx -ldl -Wl,-export-dynamic
#-lgcov
DISTFILES=$(BINARY) frontends decoders themes gmu.png README.txt COPYING gmuinput.unknown.conf
