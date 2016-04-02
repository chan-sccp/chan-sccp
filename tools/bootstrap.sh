#!/bin/sh
#
# Helps generate autoconf stuff, when code is checked out from SCM.
#
# Copyright (C) 2006-2014 - Karel Zak <kzak@redhat.com>
#
srcdir=`dirname $0`/..
test -z "$srcdir" && srcdir=.

THEDIR=`pwd`
cd $srcdir
DIE=0
CONFIG_DIR="autoconf"
AL_OPTS="--force -I $CONFIG_DIR"

test -f src/chan_sccp.c || {
	echo
	echo "You must run this script in the top-level chan-sccp-b directory"
	echo
	DIE=1
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to generate chan-sccp-b build system."
	echo
	DIE=1
}
(autoheader --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoheader installed to generate chan-sccp-b build system."
	echo "The autoheader command is part of the GNU autoconf package."
	echo
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to generate chan-sccp-b build system."
	echo 
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

echo
echo "Generate build-system using:"
echo " - aclocal:    $(aclocal --version | head -1)"
echo " - autoconf:   $(autoconf --version | head -1)"
echo " - autoheader: $(autoheader --version | head -1)"
echo " - automake:   $(automake --version | head -1)"
echo

#chmod +x version.sh
rm -rf autom4te.cache

touch autoconf/config.rpath
echo "bootstrapping:"
echo " - running aclocal..."
aclocal $AL_OPTS
echo " - running autoconf..."
autoconf $AC_OPTS
echo " - running autoheader..."
autoheader $AH_OPTS
echo " - running automake..."
automake --add-missing --copy --force-missing 2>/dev/null
echo "bootstrap done."

cd $THEDIR
if test -f config.status; then
	echo
	echo "Already configured, re-using current settings..."
	echo
	make
else
	echo
	echo "Now type '$srcdir/configure' and 'make' to compile."
	echo
fi
