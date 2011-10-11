#!/bin/sh
#
# FILE: Automatically generate configure script
# COPYRIGHT: chan-sccp-b.sourceforge.net group 2009
# CREATED BY: Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
#          See the LICENSE file at the top of the source tree.
# DATE:     $Date$
# REVISION: $Revision$

if [ -f src/Makefile ]; then
  rm -rf config aclocal.m4 autom4te.cache/ src/Makefile.in src/config.h src/Makefile
fi

if [ -f config.cache ]; then
  rm config.cache
fi
check_for_app() {
	$1 --version 2>&1 >/dev/null
	if [ $? != 0 ]
	then
		echo "Please install $1 and run bootstrap.sh again!"
		exit 1
	fi
}

# On FreeBSD and OpenBSD, multiple autoconf/automake versions have different names.
# On linux, environment variables tell which one to use.

uname -s | grep -q BSD
if [ $? = 0 ] ; then	# BSD case
	case `uname -sr` in
		'FreeBSD 4'*)	# FreeBSD 4.x has a different naming
			MY_AC_VER=259
			MY_AM_VER=19
			;;
		*)
			MY_AC_VER=-2.60
			MY_AM_VER=-1.10
			;;
	esac
else	# linux case
	MY_AC_VER=
	MY_AM_VER=
	AUTOCONF_VERSION=2.60
	AUTOMAKE_VERSION=1.10
	export AUTOCONF_VERSION
	export AUTOMAKE_VERSION
fi

check_for_app autoconf${MY_AC_VER}
check_for_app autoheader${MY_AC_VER}
check_for_app automake${MY_AM_VER}
check_for_app aclocal${MY_AM_VER}

echo "Generating the configure script ..."
echo 
aclocal${MY_AM_VER} --force -I autoconf
#libtoolize --force --copy
autoheader${MY_AC_VER}
automake${MY_AM_VER} --add-missing --copy --force-missing 2>/dev/null
autoconf${MY_AC_VER}

echo "Running configure script..."
echo
./configure

exit 0
