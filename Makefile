# 
# Gmu Music Player
#
# Copyright (c) 2006-2016 Johannes Heimansberg (wejp.k.vu)
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
include config.mk

PREFIX?=/usr/local
CFLAGS+=$(COPTS) -pipe -Wall -Wcast-qual -Wno-variadic-macros -Wuninitialized -Wcast-align -Wredundant-decls -Wmissing-declarations -DFILE_HW_H="\"hw_$(TARGET).h\"" -DGMU_INSTALL_PREFIX="\"$(PREFIX)\""

LIBS_CORE+=$(SDL_LIB) -lrt
ifeq ($(GMU_MEDIALIB),1)
LIBS_CORE+=-lsqlite3
endif
LIBS_SDLFE=$(SDL_LIB) -lSDL_image
ifneq ($(SDLFE_WITHOUT_SDL_GFX),1)
LIBS_SDLFE+=-lSDL_gfx
else
CFLAGS+=-DSDLFE_WITHOUT_SDL_GFX=1
endif

OBJECTFILES=core.o ringbuffer.o util.o dir.o trackinfo.o playlist.o wejconfig.o m3u.o pls.o audio.o charset.o fileplayer.o decloader.o feloader.o eventqueue.o oss_mixer.o debug.o reader.o hw_$(TARGET).o fmath.o id3.o metadatareader.o dirparser.o gmuerror.o pthread_helper.o
ifeq ($(GMU_MEDIALIB),1)
OBJECTFILES+=medialib.o
endif
ALLFILES=src/ htdocs/ Makefile configure *.sh *.dge *.gpu dingux.mk gp2xwiz.mk pandora.mk unknown.mk caanoo.mk dingoo-native.mk gp2x.mk nanonote.mk pre.mk zipit-z2.mk gmu.png themes README.txt BUILD.txt COPYING *.keymap gmuinput.*.conf gmuinput.conf gmu.*.conf gmu.bmp gmu.desktop PXML.xml
BINARY=gmu.bin
COMMON_DISTBIN_FILES=$(BINARY) frontends decoders themes gmu.png README.txt libs.$(TARGET) COPYING gmu.bmp gmu.desktop
DISTFILES?=$(COMMON_DISTBIN_FILES)

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
FRONTEND_PLUGIN_LOADER_FUNCTION=f`echo frontends/$(basename $@).so|md5sum|cut -d ' ' -f 1`
DECODER_PLUGIN_LOADER_FUNCTION=f`echo $(basename $@).so|md5sum|cut -d ' ' -f 1`
CFLAGS+=-DSTATIC=1
PLUGIN_CFLAGS=-c -fPIC $(COPTS)
GENERATED_HEADER_FILES_STATIC=$(TEMP_HEADER_FILES)
PLUGIN_OBJECTFILES=
LFLAGS+=$(LFLAGS_SDLFE)
endif

# Frontend configs
PLUGIN_FE_sdl_OBJECTFILES=sdl.o kam.o skin.o textrenderer.o question.o filebrowser.o plbrowser.o about.o setup.o textbrowser.o coverimg.o coverviewer.o plmanager.o playerdisplay.o gmuwidget.o png.o jpeg.o bmp.o inputconfig.o help.o
PLUGIN_FE_gmuhttp_OBJECTFILES=gmuhttp.o sha1.o base64.o httpd.o queue.o json.o websocket.o net.o
PLUGIN_FE_log_OBJECTFILES=log.o

# Decoder configs
DEC_vorbis_LIBS=-lvorbisidec
DEC_mpg123_LIBS=-lmpg123
DEC_flac_LIBS=-lFLAC
DEC_musepack_LIBS=-lmpcdec
DEC_mikmod_LIBS=-lmikmod
DEC_speex_LIBS=-logg -lspeex
DEC_modplug_LIBS=-lmodplug
DEC_openmpt_LIBS=-lopenmpt
DEC_opus_LIBS=-lopus -logg -lopusfile
DEC_wavpack_LIBS=-lwavpack

ifeq (1,$(STATIC))
LIBS+=$(foreach i, $(DECODERS_TO_BUILD), $(DEC_$(subst decoders/,,$(basename $(i)))_LIBS))
PLUGIN_OBJECTFILES+=$(foreach i, $(DECODERS_TO_BUILD), $(basename $(i)).o)
DECODERS=
FRONTENDS=
OBJECTFILES+=$(foreach i, $(FRONTENDS_TO_BUILD), $(PLUGIN_FE_$(subst frontends/,,$(basename $(i)))_OBJECTFILES))
else
DECODERS=decoders
FRONTENDS=frontends
endif

TOOLS_TO_BUILD?=$(BINARY) gmuc
DISTBIN_DEPS?=default_distbin

TEMP_HEADER_FILES=tmp-felist.h tmp-declist.h

all: $(DECODERS) $(FRONTENDS) $(TOOLS_TO_BUILD)
	@echo -e "All done for target \033[1m$(TARGET)\033[0m. \033[1m$(BINARY)\033[0m binary, \033[1mfrontends\033[0m and \033[1mdecoders\033[0m ready."

config.mk:
	$(error ERROR: Please run the configure script first)

decoders: $(DECODERS_TO_BUILD)
	@echo -e "All \033[1mdecoders\033[0m have been built."

frontends: $(FRONTENDS_TO_BUILD)
	@echo -e "All \033[1mfrontends\033[0m have been built."

frontendsdir:
	$(Q)-mkdir -p frontends

decodersdir:
	$(Q)-mkdir -p decoders

$(BINARY): $(OBJECTFILES) $(PLUGIN_OBJECTFILES)
	@echo -e "Linking \033[1m$(BINARY)\033[0m"
	$(Q)$(CC) $(LFLAGS) $(LFLAGS_CORE) -o $(BINARY) $(OBJECTFILES) $(PLUGIN_OBJECTFILES) $(LIBS_CORE) $(LIBS)

libs.$(TARGET):
	$(Q)-mkdir -p libs.$(TARGET)

ver=$(shell awk '/define VERSION_NUMBER/ { print $$3 }' src/core.h|cut -d '"' -f 2 )
projname=gmu-${ver}

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

distbin: $(DISTBIN_DEPS)

default_distbin: $(DISTFILES)
	@echo -e "Creating \033[1m$(projname)-$(TARGET).zip\033[0m"
	$(Q)-rm -rf $(projname)-$(TARGET)
	$(Q)-rm -rf $(projname)-$(TARGET).zip
	$(Q)mkdir $(projname)-$(TARGET)
	$(Q)cp -rl --parents $(DISTFILES) $(projname)-$(TARGET)
	$(Q)-cp gmuc $(projname)-$(TARGET)
	$(Q)-cp gmu.$(TARGET).conf $(projname)-$(TARGET)/gmu.$(TARGET).conf
	$(Q)-cp $(TARGET).keymap $(projname)-$(TARGET)/$(TARGET).keymap
	$(Q)-cp gmuinput.$(TARGET).conf $(projname)-$(TARGET)/gmuinput.conf
	$(Q)$(STRIP) $(projname)-$(TARGET)/decoders/*.so
	$(Q)$(STRIP) $(projname)-$(TARGET)/$(BINARY)
	$(Q)-$(STRIP) $(projname)-$(TARGET)/gmuc
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
	$(Q)-cp gmuc $(DESTDIR)$(PREFIX)/bin/gmuc
	$(Q)cp README.txt $(DESTDIR)$(PREFIX)/share/gmu/README.txt
	$(Q)cp -R frontends/* $(DESTDIR)$(PREFIX)/share/gmu/frontends
	$(Q)cp -R decoders/* $(DESTDIR)$(PREFIX)/share/gmu/decoders
	$(Q)cp -R themes/* $(DESTDIR)$(PREFIX)/share/gmu/themes
	$(Q)-mkdir -p $(DESTDIR)$(PREFIX)/share/gmu/htdocs
	$(Q)cp -R htdocs/* $(DESTDIR)$(PREFIX)/share/gmu/htdocs
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
	$(Q)-rm -rf *.o $(BINARY) gmuc decoders/*.so decoders/*.o frontends/*.so frontends/*.o
	$(Q)-rm -f $(TEMP_HEADER_FILES)
	@echo -e "\033[1mAll clean.\033[0m"

gmuc: gmuc.o window.o listwidget.o websocket.o base64.o debug.o ringbuffer.o net.o json.o dir.o wejconfig.o ui.o charset.o nethelper.o util.o
	@echo -e "Linking \033[1mgmuc\033[0m"
	$(Q)$(CC) $(CFLAGS) $(LFLAGS) -o gmuc gmuc.o wejconfig.o websocket.o base64.o debug.o ringbuffer.o net.o json.o window.o listwidget.o dir.o ui.o charset.o nethelper.o util.o -lncursesw

%.o: src/tools/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

decoders/%.so: src/decoders/%.c | decodersdir
	@echo -e "Building \033[1m$@\033[0m from \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(DEC_$(*)_CFLAGS) $(LFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_DECODER=$(DECODER_PLUGIN_LOADER_FUNCTION) $(DEC_$(*)_LIBS)

decoders/%.o: src/decoders/%.c | decodersdir
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -c -o $@ $(CFLAGS) $(DEC_$(*)_CFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_DECODER=$(DECODER_PLUGIN_LOADER_FUNCTION) $(DEC_$(*)_LIBS)

%.o: src/decoders/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -fPIC $(CFLAGS) -DGMU_REGISTER_DECODER=$(DECODER_PLUGIN_LOADER_FUNCTION) -Isrc/ -c -o $@ $<

frontends/sdl.so: $(PLUGIN_FE_sdl_OBJECTFILES) | frontendsdir
	@echo -e "Linking \033[1m$@\033[0m"
	$(Q)$(CC) $(CFLAGS) $(LFLAGS) $(LFLAGS_SDLFE) -Isrc/ $(PLUGIN_CFLAGS) $(PLUGIN_FE_sdl_OBJECTFILES) $(LIBS_SDLFE)

frontends/log.so: log.o util.o | frontendsdir
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION) util.o -lpthread

frontends/lirc.so: src/frontends/lirc.c | frontendsdir
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) $< -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION) -lpthread -llirc_client

%.o: src/frontends/web/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -fPIC $(CFLAGS) -Isrc/ -c -o $@ $<

%.o: src/frontends/%.c
	@echo -e "Compiling \033[1m$<\033[0m"
	$(Q)$(CC) -fPIC $(CFLAGS) -Isrc/ -c -o $@ $< -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION)

frontends/gmuhttp.so: $(PLUGIN_FE_gmuhttp_OBJECTFILES) | frontendsdir
	@echo -e "Building \033[1m$@\033[0m"
	$(Q)$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) $(LFLAGS) -o frontends/gmuhttp.so src/frontends/web/gmuhttp.c -DGMU_REGISTER_FRONTEND=$(FRONTEND_PLUGIN_LOADER_FUNCTION) $(PLUGIN_FE_gmuhttp_OBJECTFILES) -lpthread

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
