#!/bin/sh
cd `dirname $0`
SDL_NOMOUSE=1 LD_LIBRARY_PATH=libs.nanonote/ ./gmu.bin -c gmu.nanonote.conf
