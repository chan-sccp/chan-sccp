#!/bin/sh
#
# Automatically generate configure script
# 
# Date:     $Date$
# Revision: $Revision$
#

if [ -f src/Makefile ]; then
  rm -rf libtool config aclocal.m4 autom4te.cache/ src/Makefile.in src/config.h src/Makefile
fi
if ! [ -d m4 ]; then mkdir m4;fi

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
autoreconf -fi
#aclocal${MY_AM_VER} --force -I m4
#libtoolize${MY_AM_VER} -q --install --copy 2>/dev/null
#autoconf${MY_AC_VER}
#autoheader${MY_AC_VER}
#automake${MY_AM_VER} --add-missing --copy --force-missing 2>/dev/null

echo "Running configure script..."
echo
./configure

exit 0
