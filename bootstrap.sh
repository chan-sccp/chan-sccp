#!/bin/sh
if [ -f src/Makefile]; then
  rm -rf libtool m4 config aclocal.m4 autom4te.cache/ src/Makefile.in src/config.h src/Makefile
fi
if ! [ -d m4 ]; then mkdir m4;fi
autoreconf -fvi 
./configure
