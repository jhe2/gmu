#!/bin/sh
cd `dirname $0`
SDL_AUDIODRIVER=dsp SDL_NOMOUSE=1 SDL_VIDEO_FBCON_ROTATION="CCW" LD_LIBRARY_PATH=libs.z2/ ./gmu -c gmu.zipit-z2.conf
