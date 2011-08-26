#!/bin/sh
cd `dirname $0`
LD_LIBRARY_PATH=libs.pandora/ ./gmu.bin -c gmu.pandora.conf
