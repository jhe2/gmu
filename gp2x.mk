# 
# Gmu Music Player
#
# Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
#
# File: gp2x.mk  Created: 060904
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
DECODERS_TO_BUILD=decoders/vorbis.so decoders/flac.so decoders/wavpack.so decoders/mpg123.so decoders/mikmod.so
FRONTENDS_TO_BUILD=frontends/sdl.so frontends/log.so
DEVICE=GP2X

SDL_LIB=$(shell /usr/local/gp2xdev/bin/sdl-config --libs)
SDL_CFLAGS=$(shell /usr/local/gp2xdev/bin/sdl-config --cflags)
CXX=gp2x-g++
CC=gp2x-gcc
COPT=-O3 -Wall -ffast-math -fomit-frame-pointer $(SDL_CFLAGS) -I/usr/local/gp2xdev/include -D_$(DEVICE)
CFLAGS=-static -L/usr/local/gp2xdev/lib -lSDL_image -lSDL_gfx $(SDL_LIB) -lpng -lFLAC -lvorbisidec -lmikmod -lmpcdec -ladplug -lmpg123 -lbinio -ljpeg -lpthread -lm -ldl -lz
DISTFILES=$(BINARY) frontends decoders themes gmu.png README.txt libs.wiz gmu-wiz.gpu gmu-gp2x.gpu COPYING
