#!/bin/sh
cd `dirname $0`
LD_LIBRARY_PATH=libs.pandora/ ./gmu -c gmu.pandora.conf
