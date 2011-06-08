# 
# Gmu Music Player
#
# Copyright (c) 2006-2010 Johannes Heimansberg (wejp.k.vu)
#
# File: Makefile  Created: 060904
#
# Description: Gmu's main Makefile
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License. See the file COPYING in the Gmu's main directory
# for details.
#

ifeq ($(TARGET),)
TARGET=unknown
endif
include $(TARGET).mk

PREFIX?=/usr/local
CFLAGS+=$(COPTS) -Wall -Wno-variadic-macros -Wuninitialized -Wcast-align -Wredundant-decls -Wmissing-declarations

LFLAGS_CORE=$(SDL_LIB) -ldl
LFLAGS_SDLFE=$(SDL_LIB) -lSDL_image
ifneq ($(SDLFE_WITHOUT_SDL_GFX),1)
LFLAGS_SDLFE+=-lSDL_gfx
else
CFLAGS+=-DSDLFE_WITHOUT_SDL_GFX=1
endif

OBJECTFILES=core.o ringbuffer.o util.o dir.o trackinfo.o playlist.o wejpconfig.o m3u.o pls.o audio.o charset.o fileplayer.o decloader.o feloader.o eventqueue.o oss_mixer.o debug.o reader.o hw_$(TARGET).o fmath.o
ALLFILES=src/ Makefile *.mk gmu.png themes README.txt BUILD.txt COPYING gmu.conf.example *.keymap *.gpu *.dge *.nn gmuinput.*.conf gmu.*.conf gmu.bmp gmu.desktop
BINARY=gmu

all: $(BINARY) decoders frontends gmu-cli
	@echo -e "All done for target \033[1m$(TARGET)\033[0m. \033[1m$(BINARY)\033[0m binary, \033[1mfrontends\033[0m and \033[1mdecoders\033[0m ready."

decoders: $(DECODERS_TO_BUILD)
	@echo -e "All \033[1mdecoders\033[0m have been built."

frontends: $(FRONTENDS_TO_BUILD)
	@echo -e "All \033[1mfrontends\033[0m have been built."

$(BINARY): $(OBJECTFILES)
	@echo -e "Linking \033[1m$(BINARY)\033[0m"
	@$(CC) $(OBJECTFILES) $(LFLAGS) $(LFLAGS_CORE) -o $(BINARY)

projname=gmu-$(shell awk '/define VERSION_NUMBER/ { print $$3 }' src/core.h )

%.o: src/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) -fPIC $(CFLAGS) -c -o $@ $<

%.o: src/frontends/sdl/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) -fPIC $(CFLAGS) -Isrc/ -c -o $@ $<

dist: $(ALLFILES)
	@echo -e "Creating \033[1m$(projname).tar.gz\033[0m"
	@-rm -rf $(projname)
	@mkdir $(projname)
	@mkdir $(projname)/frontends
	@mkdir $(projname)/decoders
	@cp -rl --parents $(ALLFILES) $(projname)
	@tar chfz $(projname).tar.gz $(projname)
	@-rm -rf $(projname)

distbin: $(DISTFILES)
	@echo -e "Creating \033[1m$(projname)-$(TARGET).zip\033[0m"
	@-rm -rf $(projname)-$(TARGET)
	@-rm -rf $(projname)-$(TARGET).zip
	@mkdir $(projname)-$(TARGET)
	@cp -rl --parents $(DISTFILES) $(projname)-$(TARGET)
	@-cp gmu-cli $(projname)-$(TARGET)
	@-cp gmu.$(TARGET).conf $(projname)-$(TARGET)/gmu.$(TARGET).conf
	@-cp $(TARGET).keymap $(projname)-$(TARGET)/$(TARGET).keymap
	@$(STRIP) $(projname)-$(TARGET)/decoders/*.so
	@$(STRIP) $(projname)-$(TARGET)/gmu
	@-$(STRIP) $(projname)-$(TARGET)/gmu-cli
	@zip -r $(projname)-$(TARGET).zip $(projname)-$(TARGET)
	@-rm -rf $(projname)-$(TARGET)

install: $(DISTFILES)
	@echo -e "Installing Gmu: prefix=$(PREFIX) destdir=$(DESTDIR)"
	@-mkdir -p $(DESTDIR)$(PREFIX)/bin
	@-mkdir -p $(DESTDIR)$(PREFIX)/etc/gmu
	@-mkdir -p $(DESTDIR)$(PREFIX)/share/gmu/decoders
	@-mkdir -p $(DESTDIR)$(PREFIX)/share/gmu/frontends
	@-mkdir -p $(DESTDIR)$(PREFIX)/share/gmu/themes
	@cp gmu $(DESTDIR)$(PREFIX)/bin/gmu.bin
	@-cp gmu-cli $(DESTDIR)$(PREFIX)/bin/gmu-cli
	@cp README.txt $(DESTDIR)$(PREFIX)/share/gmu/README.txt
	@cp -R frontends/* $(DESTDIR)$(PREFIX)/share/gmu/frontends
	@cp -R decoders/* $(DESTDIR)$(PREFIX)/share/gmu/decoders
	@cp -R themes/* $(DESTDIR)$(PREFIX)/share/gmu/themes
	@cp *.conf $(DESTDIR)$(PREFIX)/etc/gmu
	@cp *.keymap $(DESTDIR)$(PREFIX)/etc/gmu
	@echo "#!/bin/sh">$(DESTDIR)$(PREFIX)/bin/gmu
	@echo "cd $(PREFIX)/share/gmu">>$(DESTDIR)$(PREFIX)/bin/gmu
	@echo "$(PREFIX)/bin/gmu.bin -e -d $(PREFIX)/etc/gmu -c gmu.$(TARGET).conf ">>$(DESTDIR)$(PREFIX)/bin/gmu
	@chmod a+x $(DESTDIR)$(PREFIX)/bin/gmu
	@-mkdir -p $(DESTDIR)$(PREFIX)/share/applications
	@cp gmu.desktop $(DESTDIR)$(PREFIX)/share/applications/gmu.desktop
	@-mkdir -p $(DESTDIR)$(PREFIX)/share/pixmaps
	@cp gmu.png $(DESTDIR)$(PREFIX)/share/pixmaps/gmu.png

clean:
	@-rm -rf *.o $(BINARY) decoders/*.so frontends/*.so
	@echo -e "\033[1mAll clean.\033[0m"

gmu-cli: src/tools/gmu-cli.c wejpconfig.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) -o gmu-cli src/tools/gmu-cli.c wejpconfig.o

decoders/musepack.so: src/decoders/musepack.c id3.o charset.o trackinfo.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) $(LFLAGS) -shared -fpic -o decoders/musepack.so src/decoders/musepack.c id3.o charset.o trackinfo.o -lmpcdec 

decoders/vorbis.so: src/decoders/vorbis.c
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) $(LFLAGS) -shared -fpic -o decoders/vorbis.so src/decoders/vorbis.c src/util.c -lvorbisidec

decoders/splay.so: src/decoders/splay.cc util.o id3.o charset.o trackinfo.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CXX) -Wall -O2 -shared -fPIC -o decoders/splay.so src/decoders/splay.cc -g util.o id3.o charset.o trackinfo.o splay/libmpegsound.a -pg -g

decoders/flac.so: src/decoders/flac.c util.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) $(LFLAGS) -shared -fpic -o decoders/flac.so src/decoders/flac.c util.o -lFLAC

decoders/mpg123.so: src/decoders/mpg123.c util.o id3.o charset.o trackinfo.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) $(LFLAGS) -shared -fpic -o decoders/mpg123.so src/decoders/mpg123.c util.o id3.o -lmpg123

decoders/wavpack.so: src/decoders/wavpack.c util.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) $(LFLAGS) -shared -fpic -o decoders/wavpack.so src/decoders/wavpack.c util.o src/decoders/wavpack/*.c

decoders/mikmod.so: src/decoders/mikmod.c util.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) $(LFLAGS) -shared -fpic -o decoders/mikmod.so src/decoders/mikmod.c util.o -lmikmod

decoders/speex.so: src/decoders/speex.c
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) $(LFLAGS) -shared -fpic -o decoders/speex.so src/decoders/speex.c src/util.c -logg -lspeex

frontends/httpserv.so: src/frontends/httpserv.c util.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) -Wall -pedantic -shared -O2 -fpic -o frontends/httpserv.so src/frontends/httpserv.c util.o

frontends/sdl.so: src/frontends/sdl/sdl.c util.o kam.o skin.o textrenderer.o question.o filebrowser.o plbrowser.o about.o textbrowser.o coverimg.o coverviewer.o plmanager.o playerdisplay.o gmuwidget.o png.o jpeg.o bmp.o inputconfig.o help.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) $(LFLAGS) $(LFLAGS_SDLFE) -Isrc/ -shared -fpic -o frontends/sdl.so src/frontends/sdl/sdl.c kam.o skin.o textrenderer.o question.o filebrowser.o plbrowser.o about.o textbrowser.o coverimg.o coverviewer.o plmanager.o playerdisplay.o gmuwidget.o png.o jpeg.o bmp.o inputconfig.o help.o

frontends/fltkfe.so: src/frontends/fltk/fltkfe.cxx
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CXX) -Wall -pedantic -shared -O2 -fpic -o frontends/fltkfe.so src/frontends/fltk/fltkfe.cxx -L/usr/lib/fltk/ -lfltk2 -lXext -lXinerama -lXft -lX11 -lXi -lm

frontends/log.so: src/frontends/log.c util.o
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) -shared -fpic -o frontends/log.so src/frontends/log.c util.o -lpthread

frontends/lirc.so: src/frontends/lirc.c
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) -shared -fpic -o frontends/lirc.so src/frontends/lirc.c -lpthread -llirc_client

frontends/gmusrv.so: src/frontends/gmusrv.c
	@echo -e "Compiling \033[1m$<\033[0m"
	@$(CC) $(CFLAGS) -shared -fpic -o frontends/gmusrv.so src/frontends/gmusrv.c -lpthread
