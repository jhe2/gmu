# 
# Gmu Music Player
#
# Copyright (c) 2006-2012 Johannes Heimansberg (wejp.k.vu)
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
Q?=@
STATIC?=0
TARGET?=unknown
include $(TARGET).mk

PREFIX?=/usr/local
CFLAGS+=$(COPTS) -Wall -Wno-variadic-macros -Wuninitialized -Wcast-align -Wredundant-decls -Wmissing-declarations -DFILE_HW_H="\"hw_$(TARGET).h\""

LFLAGS_CORE=$(SDL_LIB) -ldl -lrt
LFLAGS_SDLFE=$(SDL_LIB) -lSDL_image
ifneq ($(SDLFE_WITHOUT_SDL_GFX),1)
LFLAGS_SDLFE+=-lSDL_gfx
else
CFLAGS+=-DSDLFE_WITHOUT_SDL_GFX=1
endif

OBJECTFILES=core.o ringbuffer.o util.o dir.o trackinfo.o playlist.o wejpconfig.o m3u.o pls.o audio.o charset.o fileplayer.o decloader.o feloader.o eventqueue.o oss_mixer.o debug.o reader.o hw_$(TARGET).o fmath.o id3.o
ALLFILES=src/ Makefile  *.sh *.dge *.gpu *.mk gmu.png themes README.txt BUILD.txt COPYING *.keymap gmuinput.*.conf gmu.*.conf gmu.bmp gmu.desktop
BINARY=gmu.bin
COMMON_DISTBIN_FILES=$(BINARY) frontends decoders themes gmu.png README.txt libs.$(TARGET) COPYING gmu.bmp gmu.desktop

ifeq (0,$(STATIC))
# normal dynamic build (with runtime-loadable plugins)
FRONTEND_PLUGIN_LOADER_FUNCTION=gmu_register_frontend
DECODER_PLUGIN_LOADER_FUNCTION=gmu_register_decoder
CFLAGS+=-DSTATIC=0
PLUGIN_CFLAGS=-shared -o $@ -fpic $(COPTS)
GENERATED_HEADERFILES_STATIC=
PLUGIN_OBJECTFILES=
else
# static build (with builtin plugins)
FRONTEND_PLUGIN_LOADER_FUNCTION=f`echo $@|md5sum|cut -d ' ' -f 1`
DECODER_PLUGIN_LOADER_FUNCTION=f`echo $@|md5sum|cut -d ' ' -f 1`
CFLAGS+=-DSTATIC=1
PLUGIN_CFLAGS=-c -fPIC $(COPTS)
GENERATED_HEADER_FILES_STATIC=$(TEMP_HEADER_FILES)
PLUGIN_OBJECTFILES+=$(DECODERS_TO_BUILD)
LFLAGS+=$(LFLAGS_SDLFE)
endif

# Frontend configs
PLUGIN_FE_SDL_OBJECTFILES=sdl.o kam.o skin.o textrenderer.o question.o filebrowser.o plbrowser.o about.o textbrowser.o coverimg.o coverviewer.o plmanager.o playerdisplay.o gmuwidget.o png.o jpeg.o bmp.o inputconfig.o help.o
PLUGIN_FE_HTTP_OBJECTFILES=gmuhttp.o sha1.o base64.o httpd.o queue.o json.o websocket.o net.o

# Decoder configs
DEC_vorbis_LFLAGS=-lvorbisidec
DEC_mpg123_LFLAGS=-lmpg123
DEC_flac_LFLAGS=-lFLAC
DEC_musepack_LFLAGS=-lmpcdec
DEC_mikmod_LFLAGS=-lmikmod
DEC_speex_LFLAGS=-logg -lspeex

ifeq (1,$(STATIC))
LFLAGS+=$(foreach i, $(DECODERS_TO_BUILD), $(DEC_$(basename $(i))_LFLAGS))
endif

TEMP_HEADER_FILES=tmp-felist.h tmp-declist.h

all: $(BINARY) decoders frontends gmu-cli
	@echo -e "All done for target \033[1m$(TARGET)\033[0m. \033[1m$(BINARY)\033[0m binary, \033[1mfrontends\033[0m and \033[1mdecoders\033[0m ready."

decoders: $(DECODERS_TO_BUILD)
	@echo -e "All \033[1mdecoders\033[0m have been built."

frontends: $(FRONTENDS_TO_BUILD)
	@echo -e "All \033[1mfrontends\033[0m have been built."

$(BINARY): $(OBJECTFILES) $(PLUGIN_OBJECTFILES)
	@echo -e "Linking \033[1m$(BINARY)\033[0m"
	$(Q)$(CC) $(OBJECTFILES) $(PLUGIN_OBJECTFILES) $(LFLAGS) $(LFLAGS_CORE) -o $(BINARY)

libs.$(TARGET):
	$(Q)-mkdir -p libs.$(TARGET)

projname=gmu-$(shell awk '/define VERSION_NUMBER/ { print $$3 }' src/core.h )

%.o: src/%.c $(GENERATED_HEADER_FILES_STATIC)
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -fPIC $(CFLAGS) -c -o $@ $<

%.o: src/frontends/sdl/%.c $(GENERATED_HEADER_FILES_STATIC)
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -fPIC $(CFLAGS) -Isrc/ -c -o $@ $< -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION)

%.o: src/frontends/web/%.c $(GENERATED_HEADER_FILES_STATIC)
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -fPIC $(CFLAGS) -Isrc/ -c -o $@ $< -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION)

dist: $(ALLFILES)
	@echo -e "Creating \033[1m$(projname).tar.gz\033[0m"
	$(Q)-rm -rf $(projname)
	$(Q)mkdir $(projname)
	$(Q)mkdir $(projname)/frontends
	$(Q)mkdir $(projname)/decoders
	$(Q)cp -rl --parents $(ALLFILES) $(projname)
	$(Q)tar chfz $(projname).tar.gz $(projname)
	$(Q)-rm -rf $(projname)

distbin: $(DISTFILES)
	@echo -e "Creating \033[1m$(projname)-$(TARGET).zip\033[0m"
	$(Q)-rm -rf $(projname)-$(TARGET)
	$(Q)-rm -rf $(projname)-$(TARGET).zip
	$(Q)mkdir $(projname)-$(TARGET)
	$(Q)cp -rl --parents $(DISTFILES) $(projname)-$(TARGET)
	$(Q)-cp gmu-cli $(projname)-$(TARGET)
	$(Q)-cp gmu.$(TARGET).conf $(projname)-$(TARGET)/gmu.$(TARGET).conf
	$(Q)-cp $(TARGET).keymap $(projname)-$(TARGET)/$(TARGET).keymap
	$(Q)$(STRIP) $(projname)-$(TARGET)/decoders/*.so
	$(Q)$(STRIP) $(projname)-$(TARGET)/$(BINARY)
	$(Q)-$(STRIP) $(projname)-$(TARGET)/gmu-cli
	$(Q)zip -r $(projname)-$(TARGET).zip $(projname)-$(TARGET)
	$(Q)-rm -rf $(projname)-$(TARGET)

install: $(DISTFILES)
	@echo -e "Installing Gmu: prefix=$(PREFIX) destdir=$(DESTDIR)"
	$(Q)-mkdir -p $(DESTDIR)$(PREFIX)/bin
	$(Q)-mkdir -p $(DESTDIR)$(PREFIX)/etc/gmu
	$(Q)-mkdir -p $(DESTDIR)$(PREFIX)/share/gmu/decoders
	$(Q)-mkdir -p $(DESTDIR)$(PREFIX)/share/gmu/frontends
	$(Q)-mkdir -p $(DESTDIR)$(PREFIX)/share/gmu/themes
	$(Q)cp $(BINARY) $(DESTDIR)$(PREFIX)/bin
	$(Q)-cp gmu-cli $(DESTDIR)$(PREFIX)/bin/gmu-cli
	$(Q)cp README.txt $(DESTDIR)$(PREFIX)/share/gmu/README.txt
	$(Q)cp -R frontends/* $(DESTDIR)$(PREFIX)/share/gmu/frontends
	$(Q)cp -R decoders/* $(DESTDIR)$(PREFIX)/share/gmu/decoders
	$(Q)cp -R themes/* $(DESTDIR)$(PREFIX)/share/gmu/themes
	$(Q)cp *.conf $(DESTDIR)$(PREFIX)/etc/gmu
	$(Q)cp *.keymap $(DESTDIR)$(PREFIX)/etc/gmu
	$(Q)echo "#!/bin/sh">$(DESTDIR)$(PREFIX)/bin/gmu
	$(Q)echo "cd $(PREFIX)/share/gmu">>$(DESTDIR)$(PREFIX)/bin/gmu
	$(Q)echo "$(PREFIX)/bin/$(BINARY) -e -d $(PREFIX)/etc/gmu -c gmu.$(TARGET).conf ">>$(DESTDIR)$(PREFIX)/bin/gmu
	$(Q)chmod a+x $(DESTDIR)$(PREFIX)/bin/gmu
	$(Q)-mkdir -p $(DESTDIR)$(PREFIX)/share/applications
	$(Q)cp gmu.desktop $(DESTDIR)$(PREFIX)/share/applications/gmu.desktop
	$(Q)-mkdir -p $(DESTDIR)$(PREFIX)/share/pixmaps
	$(Q)cp gmu.png $(DESTDIR)$(PREFIX)/share/pixmaps/gmu.png

clean:
	$(Q)-rm -rf *.o $(BINARY) decoders/*.so frontends/*.so
	$(Q)-rm -f $(TEMP_HEADER_FILES)
	@echo -e "\033[1mAll clean.\033[0m"

gmu-cli: src/tools/gmu-cli.c wejpconfig.o
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) -o gmu-cli src/tools/gmu-cli.c wejpconfig.o

gmuc: gmuc.o window.o listwidget.o websocket.o base64.o debug.o ringbuffer.o net.o json.o dir.o wejpconfig.o ui.o charset.o nethelper.o
	@echo -e "Linking \033[1mgmuc\033[0m"
	$(Q)$(CC) $(CFLAGS) $(LFLAGS) -o gmuc gmuc.o wejpconfig.o websocket.o base64.o debug.o ringbuffer.o net.o json.o window.o listwidget.o dir.o ui.o charset.o nethelper.o -lncursesw

%.o: src/tools/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

decoders/%.so: src/decoders/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(LFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_DECODER=$(DECODER_PLUGIN_LOADER_FUNCTION) $(DEC_$(*)_LFLAGS)

decoders/wavpack.so: src/decoders/wavpack.c util.o
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(LFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_DECODER=$(DECODER_PLUGIN_LOADER_FUNCTION) util.o src/decoders/wavpack/*.c

%.o: src/decoders/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -fPIC $(CFLAGS) -DGMU_REGISTER_DECODER=$(DECODER_PLUGIN_LOADER_FUNCTION) -Isrc/ -c -o $@ $<

frontends/httpserv.so: src/frontends/httpserv.c util.o
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -Wall -pedantic $(PLUGIN_CFLAGS) -O2 $< util.o

frontends/sdl.so: $(PLUGIN_FE_SDL_OBJECTFILES)
	@echo -e "Linking \033[1m$@\033[0m"
	$(Q)$(CC) $(CFLAGS) $(LFLAGS) $(LFLAGS_SDLFE) -Isrc/ $(PLUGIN_CFLAGS) $(PLUGIN_FE_SDL_OBJECTFILES)

frontends/fltkfe.so: src/frontends/fltk/fltkfe.cxx
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CXX) -Wall -pedantic $(PLUGIN_CFLAGS) -O2 $< -L/usr/lib/fltk/ -lfltk2 -lXext -lXinerama -lXft -lX11 -lXi -lm

frontends/log.so: src/frontends/log.c util.o
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION) util.o -lpthread

frontends/lirc.so: src/frontends/lirc.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION) -lpthread -llirc_client

frontends/gmusrv.so: src/frontends/gmusrv.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION) -lpthread

%.o: src/frontends/web/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -fPIC $(CFLAGS) -Isrc/ -c -o $@ $<

frontends/gmuhttp.so: $(PLUGIN_FE_HTTP_OBJECTFILES)
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) $(LFLAGS) -o frontends/gmuhttp.so src/frontends/web/gmuhttp.c -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION) -lpthread sha1.o base64.o httpd.o queue.o json.o websocket.o net.o

tmp-felist.h:
	@echo -e "Creating file \033[1mtmp-felist.h\033[0m"
	$(Q)echo "/* Generated file. Do not edit. */">tmp-felist.h
	$(Q)$(foreach i, $(FRONTENDS_TO_BUILD), echo "GmuFrontend *f`echo $(i)|md5sum|cut -d ' ' -f 1`(void);">>tmp-felist.h;)
	$(Q)echo "GmuFrontend *(*feload_funcs[])(void) = {">>tmp-felist.h
	$(Q)$(foreach i, $(FRONTENDS_TO_BUILD), echo "f`echo $(i)|md5sum|cut -d ' ' -f 1`,">>tmp-felist.h;)
	$(Q)echo "NULL };">>tmp-felist.h

tmp-declist.h:
	@echo -e "Creating file \033[1mtmp-declist.h\033[0m"
	$(Q)echo "/* Generated file. Do not edit. */">tmp-declist.h
	$(Q)$(foreach i, $(DECODERS_TO_BUILD), echo "GmuDecoder *f`echo $(i)|md5sum|cut -d ' ' -f 1`(void);">>tmp-declist.h;)
	$(Q)echo "GmuDecoder *(*decload_funcs[])(void) = {">>tmp-declist.h
	$(Q)$(foreach i, $(DECODERS_TO_BUILD), echo "f`echo $(i)|md5sum|cut -d ' ' -f 1`,">>tmp-declist.h;)
	$(Q)echo "NULL };">>tmp-declist.h
