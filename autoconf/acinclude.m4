dnl FILE: acinclude.m4
dnl COPYRIGHT: chan-sccp-b.sourceforge.net group 2009
dnl CREATED BY: Created by Diederik de Groot
dnl LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
dnl          See the LICENSE file at the top of the source tree.

## ------------------##
## CS_Check_PBX	(DdG)##
## ------------------##
AC_DEFUN([CS_CHECK_PBX], [
	found_pbx="no";
	found_asterisk="no";
	AS_IF([test -z "`${CC} -isystem /usr/include -dM -E - </dev/null 2>&1 >/dev/null`" && test $? == 0],[replace_include_with_isystem=1],[replace_include_with_isystem=0])
	if test -z "$NEW_PBX_PATH" && test ! x"${PKGCONFIG}" = xNo; then
		AC_MSG_CHECKING([pkg-config asterisk])
	 	if $(${PKGCONFIG} --exists asterisk); then
			PBX_TYPE="Asterisk"
			PBX_ETC="$(${PKGCONFIG} --variable=etcdir asterisk)"
			PBX_PREFIX="$(${PKGCONFIG} --variable=install_prefix asterisk)"
			PBX_VERSION="$(${PKGCONFIG} --modversion asterisk)"
			if test -n "${PBX_ETC}"; then
				PBX_INCLUDE="${PBX_PREFIX}/usr/include"
				PBX_CFLAGS="-I${PBX_INCLUDE} $(${PKGCONFIG} --cflags asterisk)"
				if test -n "`echo ${PBX_CFLAGS} | grep libxml2-g3`"; then			dnl fix old errornous asterisk.pc (asterisk < 11.13)
					AC_MSG_NOTICE([Fixed fauly asterisk.pc pkg-config file. Advise:You ought to update your asterisk version])
					PBX_CFLAGS="`echo ${PBX_CFLAGS} | ${SED} 's/libxml2-g3/libxml2/'`"
				fi
				if test ${replace_include_with_isystem} -eq 1; then
					PBX_CFLAGS="`echo ${PBX_CFLAGS} | ${SED} 's/-I/-isystem /'`"
				fi
				PBX_CPPFLAGS="${PBX_CFLAGS}"
				CFLAGS="${CFLAGS} ${PBX_CFLAGS}"
				CPPFLAGS="${CPPFLAGS} ${PBX_CPPFLAGS}"
	 			PBX_LIB="$(${PKGCONFIG} --variable=libdir asterisk)"
		 		if test -f "${PBX_ETC}/asterisk.conf"; then
					PBX_TEMPMODDIR="`${SED} -n 's/astmoddir[[[[:space:]]]]*=>[[[[:space:]]]]*\(.*\)$/\1/p' ${PBX_ETC}/asterisk.conf`"
					if test -z "$PBX_TEMPMODDIR"; then
						echo ""
						AC_MSG_NOTICE([astmoddir could not be found in ${PBX_ETC}/asterisk.conf (your asterisk.conf file is faulty or not readable). Note: Path separators use '=>', not '='.])
						AC_MSG_NOTICE([See https://github.com/chan-sccp/chan-sccp/wiki/astmoddir-could-not-be-found for more info])
					fi
					PBX_VARLIB="`${SED} -n 's/astvarlibdir[[[[:space:]]]]*=>[[[[:space:]]]]*\(.*\)$/\1/p' ${PBX_ETC}/asterisk.conf`"
					if test -z "$PBX_VARLIB"; then
						echo ""
						AC_MSG_NOTICE([astvarlibdir could not be found in ${PBX_ETC}/asterisk.conf (your asterisk.conf file is faulty or not readable). Note: Path separators use '=>', not '='.])
						AC_MSG_NOTICE([See https://github.com/chan-sccp/chan-sccp/wiki/astmoddir-could-not-be-found for more info])
					fi
					PBX_SBINDIR="`${SED} -n 's/astsbindir[[[[:space:]]]]*=>[[[[:space:]]]]*\(.*\)$/\1/p' ${PBX_ETC}/asterisk.conf`"
					if test -z "$PBX_SBINDIR"; then
						echo ""
						AC_MSG_NOTICE([astsbindir could not be found in ${PBX_ETC}/asterisk.conf (your asterisk.conf file is faulty or not readable). Note: Path separators use '=>', not '='.])
						AC_MSG_NOTICE([See https://github.com/chan-sccp/chan-sccp/wiki/astmoddir-could-not-be-found for more info])
					fi
					PBX_DATADIR="`${SED} -n 's/astdatadir[[[[:space:]]]]*=>[[[[:space:]]]]*\(.*\)$/\1/p' ${PBX_ETC}/asterisk.conf`"
		 		else
		 			PBX_TEMPMODDIR="$(${PKGCONFIG} --variable=moddir asterisk)"
		 			PBX_VARLIB="$(${PKGCONFIG} --variable=varlibdir asterisk)"
					PBX_SBINDIR="${PBX_PREFIX}/sbin/"
		 		fi
                                if test -z "$PBX_SDATADIR"; then
                                        PBX_DATADIR="${PBX_VARLIB}"
                                fi
				LDFLAGS="$LDFLAGS -L${PBX_LIB} $(${PKGCONFIG} --libs asterisk)"
				PBX_LDFLAGS="$LDFLAGS"
				HAVE_ASTERISK=yes
				AC_MSG_RESULT(found)				
			fi 
		else
			AC_MSG_RESULT(not found)
	 	fi
	fi
	if test x_$HAVE_ASTERISK != x_yes; then
		AC_MSG_CHECKING([Search Path: $PBX_PATH])
		for dir in $PBX_PATH; do
			if test "`echo $dir | cut -c1`" == "."; then 
				checkdir="`pwd`/${dir%/}"
			else
				checkdir="${dir%/}"
			fi
			if test -f "${checkdir}/include/asterisk/asterisk.h"; then
				found_pbx="yes";
				found_asterisk="yes";
				PBX_TYPE="Asterisk"
				PBX_INCLUDE="${checkdir}/include/asterisk"
				if test $replace_include_with_isystem -eq 1; then
					PBX_CFLAGS="-isystem ${checkdir}/include -DHAVE_ASTERISK";
					PBX_CPPFLAGS="-isystem ${checkdir}/include -DHAVE_ASTERISK";
					CFLAGS="$CFLAGS -isystem ${checkdir}/include -DHAVE_ASTERISK";
					CPPFLAGS="$CPPFLAGS -isystem ${checkdir}/include -DHAVE_ASTERISK";
				else
					PBX_CFLAGS="-I${checkdir}/include -DHAVE_ASTERISK";
					PBX_CPPFLAGS="-I${checkdir}/include -DHAVE_ASTERISK";
					CFLAGS="$CFLAGS -I${checkdir}/include -DHAVE_ASTERISK";
					CPPFLAGS="$CPPFLAGS -I${checkdir}/include -DHAVE_ASTERISK";
				fi
				AC_MSG_RESULT(found)
				break;
			fi
			if test -f "${checkdir}/include/asterisk.h"; then
				found_pbx="yes";
				found_asterisk="yes";
				PBX_TYPE="Asterisk"
				PBX_INCLUDE="${checkdir}/include/asterisk"
				if test $replace_include_with_isystem -eq 1; then
					PBX_CFLAGS="-isystem ${checkdir}/include -DHAVE_ASTERISK";
					PBX_CPPFLAGS="-isystem ${checkdir}/include -DHAVE_ASTERISK";
					CFLAGS="$CFLAGS -isystem ${checkdir}/include -DHAVE_ASTERISK";
					CPPFLAGS="$CPPFLAGS -isystem ${checkdir}/include -DHAVE_ASTERISK";
					PBX_SBINDIR="${checkdir}/sbin"
				else
					PBX_CFLAGS="-I${checkdir}/include -DHAVE_ASTERISK";
					PBX_CPPFLAGS="-I${checkdir}/include -DHAVE_ASTERISK";
					CFLAGS="$CFLAGS -I${checkdir}/include -DHAVE_ASTERISK";
					CPPFLAGS="$CPPFLAGS -I${checkdir}/include -DHAVE_ASTERISK";
					PBX_SBINDIR="/usr/sbin"
				fi
				AC_MSG_RESULT(found)
				break
			fi
			AC_MSG_RESULT(not-found)
		done
		if test x_$found_pbx != x_yes; then
			AC_MSG_NOTICE([Please install either the asterisk-devel package.])
			AC_MSG_NOTICE([Or run ./configure --with-asterisk=PATH with PATH pointing to the directory where you installed asterisk])
			AC_MSG_ERROR([Cannot find pbx libraries - these are required.])
		else
			if test x_$found_asterisk = x_yes; then
				AC_MSG_RESULT([Asterisk found in ${checkdir}])
				PBX_PREFIX="/usr"
				PBX_ETC="/etc/asterisk"
				PBX_LIB="/usr/lib"
				if test -f "${checkdir}/etc/asterisk/asterisk.conf"; then
					PBX_PREFIX="${checkdir}"
					PBX_ETC="${checkdir}/etc/asterisk"
					if test -d "${checkdir}/var/lib/asterisk"; then
						PBX_VARLIB="${checkdir}/var/lib/asterisk"
					fi
					if test -d "${checkdir}/usr/sbin"; then
						PBX_SBINDIR="${checkdir}/usr/sbin"
					fi
					if test -d "${checkdir}/lib"; then
						PBX_LIB="${checkdir}/lib"
					fi
				fi
				if test -n "${PBX_ETC}"; then
					PBX_TEMPMODDIR="`${SED} -n 's/astmoddir[[[[:space:]]]]*=>[[[[:space:]]]]*\(.*\)$/\1/p' ${PBX_ETC}/asterisk.conf`"
					if test -z "$PBX_TEMPMODDIR"; then
						echo ""
						AC_MSG_NOTICE([astmoddir could not be found in ${PBX_ETC}/asterisk.conf (your asterisk.conf file is faulty or not readable). Note: Path seperators use '=>', not '='])
						AC_MSG_NOTICE([See https://github.com/chan-sccp/chan-sccp/wiki/astmoddir-could-not-be-found for more info])
					fi
					PBX_VARLIB="`${SED} -n 's/astvarlibdir[[[[:space:]]]]*=>[[[[:space:]]]]*\(.*\)$/\1/p' ${PBX_ETC}/asterisk.conf`"
					if test -z "$PBX_VARLIB"; then
						echo ""
						AC_MSG_NOTICE([astvarlibdir could not be found in ${PBX_ETC}/asterisk.conf (your asterisk.conf file is faulty or not readable). Note: Path seperators use '=>', not '='])
						AC_MSG_NOTICE([See https://github.com/chan-sccp/chan-sccp/wiki/astmoddir-could-not-be-found for more info])
					fi
					PBX_SBINDIR="`${SED} -n 's/astsbindir[[[[:space:]]]]*=>[[[[:space:]]]]*\(.*\)$/\1/p' ${PBX_ETC}/asterisk.conf`"
					if test -z "$PBX_SBINDIR"; then
						echo ""
						AC_MSG_NOTICE([astsbindir could not be found in ${PBX_ETC}/asterisk.conf (your asterisk.conf file is faulty or not readable). Note: Path seperators use '=>', not '='])
						AC_MSG_NOTICE([See https://github.com/chan-sccp/chan-sccp/wiki/astmoddir-could-not-be-found for more info])
					fi
					PBX_DATADIR="`${SED} -n 's/astdatadir[[[[:space:]]]]*=>[[[[:space:]]]]*\(.*\)$/\1/p' ${PBX_ETC}/asterisk.conf`"
				fi
				PBX_LDFLAGS="$LDFLAGS -L${checkdir}/lib"
				case "${build_cpu}" in
					x86_64|amd64|ppc64)
						if test -d "${checkdir}/lib64/asterisk"; then
							PBX_LIB="${checkdir}/lib64"
							PBX_LDFLAGS="$LDFLAGS -L${checkdir}/lib64"
						fi
						;;
				esac;
				if test -z "${PBX_TEMPMODDIR}"; then
					PBX_TEMPMODDIR="${PBX_LIB}/asterisk/modules"
				fi
				if test -z "${PBX_VARLIB}"; then
					PBX_VARLIB="/var/lib/asterisk"
				fi 
				if test -z "${PBX_SBINDIR}"; then
					PBX_SBINDIR="/usr/sbin"
				fi 
                                if test -z "$PBX_SDATADIR"; then
                                        PBX_DATADIR="${PBX_VARLIB}"
                                fi

				LDFLAGS="$PBX_LDFLAGS"
				HAVE_ASTERISK=yes
				AC_MSG_RESULT(done)
			fi
		fi
	fi
	if test -z "$PBX_TEMPMODDIR"; then
		echo ""
		AC_MSG_ERROR([astmoddir could not be established, giving up])
		AC_MSG_NOTICE([See https://github.com/chan-sccp/chan-sccp/wiki/astmoddir-could-not-be-found for more info])
	fi
	if test -z "$PBX_VARLIB"; then
		echo ""
		AC_MSG_ERROR([astvarlib could not be established, giving up])
		AC_MSG_NOTICE([See https://github.com/chan-sccp/chan-sccp/wiki/astmoddir-could-not-be-found for more info])
	fi
	AC_DEFINE_UNQUOTED([PBX_ETC],"${PBX_ETC}",[Define PBX_ETC])
	AC_DEFINE_UNQUOTED([PBX_PREFIX],"${PBX_PREFIX}",[Define PBX_PREFIX])
	AC_DEFINE_UNQUOTED([PBX_MODDIR],"${PBX_TEMPMODDIR}",[Define PBX_MODDIR])
	AC_DEFINE_UNQUOTED([PBX_VARLIB],"${PBX_VARLIB}",[Define PBX_VARLIB])
	AC_DEFINE_UNQUOTED([PBX_DATADIR],"${PBX_DATADIR}",[Define PBX_VARLIB])
	AC_SUBST([PBX_TYPE])
	AC_SUBST([PBX_VERSION])
	AC_SUBST([HAVE_ASTERISK])
	AC_SUBST([HAVE_CALLWEAVER])
	AC_SUBST([PBX_ETC])
	AC_SUBST([PBX_PREFIX])
	AC_SUBST([PBX_LIB])
	AC_SUBST([PBX_VARLIB])
	AC_SUBST([PBX_DATADIR])
	AC_SUBST([PBX_INCLUDE])
	AC_SUBST([PBX_TEMPMODDIR])
	AC_SUBST([PBX_CFLAGS])
	AC_SUBST([PBX_LDFLAGS])
	AC_SUBST([PBX_SBINDIR])
])

## -------------------------##
## CS_CV_TRY_COMPILE_IFELSE ##
## -------------------------##
dnl CS_CV_TRY_COMPILE_IFELSE(MSG, CACHE_VAL, INCLUDE, C_SOURCE[, IF COMPILE_OK[, IF_NOT_COMPILE]])
AC_DEFUN([CS_CV_TRY_COMPILE_IFELSE],
	[m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
	AC_LANG_PUSH([C])
        AC_CACHE_CHECK(
        	[$1],
        	[$2],
        	[
                        AC_COMPILE_IFELSE(
                       		[AC_LANG_PROGRAM(
                        		[$3], 
         	                	[$4],
       	                	)], 
                        	[
                                        $2="yes"
                                        AC_CACHE_VAL([$2], "yes")
                                ], 
                                [
                                        $2="no"
                                        AC_CACHE_VAL([$2], "no")
                                ]
                        )
        	]
        )	
        eval val=\$$2
        if test "x$val" = "xyes"; then
             ifelse([$5], , :, $5) :
             ifelse([$6], , :, else $6) :
        fi
	AC_LANG_POP
])

## -------------------------##
## CS_CV_TRY_COMPILE_DEFINE ##
## -------------------------##
dnl CS_CV_TRY_COMPILE_DEFINE(MSG, CACHE_VAL, INCLUDE, C_SOURCE, DEFINE, DEFINE_COMMENT)
AC_DEFUN([CS_CV_TRY_COMPILE_DEFINE],
[
	CS_CV_TRY_COMPILE_IFELSE([$1],[$2],[$3],[$4],[AC_DEFINE([$5],1,[$6])],[])
])

## ---------------------##
## CS_CHECK_AST_TYPEDEF ##
## ---------------------##
dnl CS_CHECK_AST_TYPEDEF(TYPEDEF, HEADER [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]])
AC_DEFUN([CS_CHECK_AST_TYPEDEF],
[
	AC_MSG_CHECKING([ - checking for typedef '$1'...])
	ac_lib_var=`echo $1['_']$2 | sed 'y%./+- %__p__%'`
	AC_CACHE_VAL(ac_cv_lib_$ac_lib_var,
	[
	  eval "ac_cv_type_$ac_lib_var='not-found'"
	  ac_cv_check_typedef_header=`echo ifelse([$2], , stddef.h, $2)`
	  AC_COMPILE_IFELSE(
	  	[AC_LANG_PROGRAM(
	  		[
				#undef PACKAGE
				#undef PACKAGE_BUGREPORT
				#undef PACKAGE_NAME
				#undef PACKAGE_STRING
				#undef PACKAGE_TARNAME
				#undef PACKAGE_VERSION
				#undef VERSION
			        #if ASTERISK_VERSION_NUMBER >= 10400
			        #define AST_MODULE_SELF_SYM __internal_chan_sccp_la_self
				#define AST_MODULE "chan_sccp"
			        #include <asterisk.h>
			        #endif
				#include <asterisk/autoconfig.h>
				#include <asterisk/buildopts.h>
				#include <$ac_cv_check_typedef_header>
			],[
				int x = sizeof($1); x = x;
			]
		)],[
	        	AC_MSG_RESULT(yes)
	        	eval "ac_cv_type_$ac_lib_var=yes"
	        ],[
	        	AC_MSG_RESULT(no)
	        	eval "ac_cv_type_$ac_lib_var=no" 
	       	])
	  if test `eval echo '$ac_cv_type_'$ac_lib_var` = "no" ; then
	    m4_ifvaln([$4],[$4],[:])dnl
	    m4_ifvaln([$3],[else $3])dnl
	  fi
	])
])


## ---------------------##
## CS_EXT_LIB_SETUP     ##
## ---------------------##
# CS_EXT_LIB_SETUP([package symbol name], [package friendly name], [package option name], [additional help text])
AC_DEFUN([CS_EXT_LIB_SETUP],
[
$1_DESCRIP="$2"
$1_OPTION="$3"
AC_ARG_WITH([$3], AC_HELP_STRING([--with-$3=PATH],[use $2 files in PATH $4]),[
case ${withval} in
     n|no)
     USE_$1=no
     ;;
     y|ye|yes)
     $1_MANDATORY="yes"
     ;;
     *)
     $1_DIR="${withval}"
     $1_MANDATORY="yes"
     ;;
esac
])
PBX_$1=0
AC_SUBST([$1_LIB])
AC_SUBST([$1_INCLUDE])
AC_SUBST([PBX_$1])
])

## ---------------------##
## CS_EXT_LIB_CHECK     ##
## ---------------------##
# CS_EXT_LIB_CHECK([package symbol name], [package library name], [function to check], [package header], [additional LIB data])

AC_DEFUN([CS_EXT_LIB_CHECK],
[
if test "${USE_$1}" != "no"; then
   pbxlibdir=""
   if test "x${$1_DIR}" != "x"; then
      if test -d ${$1_DIR}/lib; then
      	 pbxlibdir="-L${$1_DIR}/lib"
      else
      	 pbxlibdir="-L${$1_DIR}"
      fi
   fi
   AC_CHECK_LIB([$2], [$3], [CS_$1_FOUND=yes], [CS_$1_FOUND=no], ${pbxlibdir} $5)

   if test "${CS_$1_FOUND}" = "yes"; then
      $1_LIB="-l$2 $5"
      $1_HEADER_FOUND="1"
      if test "x${$1_DIR}" != "x"; then
         $1_LIB="${pbxlibdir} ${$1_LIB}"
	 $1_INCLUDE="-I${$1_DIR}/include"
	 if test "x$4" != "x" ; then
	    AC_CHECK_HEADER([${$1_DIR}/include/$4], [$1_HEADER_FOUND=1], [$1_HEADER_FOUND=0] )
	 fi
      else
	 if test "x$4" != "x" ; then
            AC_CHECK_HEADER([$4], [$1_HEADER_FOUND=1], [$1_HEADER_FOUND=0] )
	 fi
      fi
      if test "x${$1_HEADER_FOUND}" = "x0" ; then
         if test ! -z "${$1_MANDATORY}" ;
         then
            AC_MSG_NOTICE( ***)
            AC_MSG_NOTICE( *** It appears that you do not have the $2 development package installed.)
            AC_MSG_NOTICE( *** Please install it to include ${$1_DESCRIP} support, or re-run configure)
            AC_MSG_NOTICE( *** without explicitly specifying --with-${$1_OPTION})
            exit 1
         fi
         $1_LIB=""
         $1_INCLUDE=""
         PBX_$1=0
      else
         PBX_$1=1
         AC_DEFINE_UNQUOTED([HAVE_$1], 1, [Define to indicate the ${$1_DESCRIP} library])
      fi
   elif test ! -z "${$1_MANDATORY}";
   then
      AC_MSG_NOTICE(***)
      AC_MSG_NOTICE(*** The ${$1_DESCRIP} installation on this system appears to be broken.)
      AC_MSG_NOTICE(*** Either correct the installation, or run configure)
      AC_MSG_NOTICE(*** without explicitly specifying --with-${$1_OPTION})
      exit 1
   fi
fi
])

## -----------------------------##
## PATCH_LIBTOOL_TO_ADD_HOST_CC ##
## -----------------------------##
AC_DEFUN([PATCH_LIBTOOL_TO_ADD_HOST_CC],
[# patch libtool to add HOST_CC sometimes needed in crosscompiling a win32 dll
if test -f libtool; then
  if grep "HOST_CC" libtool >/dev/null; then
    if test "$build" != "$host" ; then
      if test "_$HOST_CC" = "_" ; then
        HOST_CC="false"
        for i in `echo $PATH | sed 's,:, ,g'` ; do
        test -x $i/cc && HOST_CC=$i/cc
        done
      fi
  AC_MSG_RESULT(patching libtool to add HOST_CC=$HOST_CC)
      test -f libtool.old || (mv libtool libtool.old && cp libtool.old libtool)
      sed -e "/BEGIN.*LIBTOOL.*CONFIG/a\\
  HOST_CC=$HOST_CC" libtool >libtool.new 
      (test -s libtool.new || rm libtool.new) 2>/dev/null
      test -f libtool.new && mv libtool.new libtool # not 2>/dev/null !!
      test -f libtool     || mv libtool.old libtool
    fi
  fi
fi  
])  

## ------------------------##
## AC_AUTO_INCLUDE_HEADERS ##
## ------------------------##
dnl AC_AUTO_INCLUDE_HEADERS([config-inc.h], [sys/foobar.h arpa/inet.h dlfcn.h errno.h])
AC_DEFUN([AC_AUTO_INCLUDE_HEADERS],
[touch $1
for ac_auto_include_file in $2; do
  ac_auto_include_have=`echo $ac_auto_include_file | sed 'y%./*abcdefghijklmnopqrstuvwxyz%__PABCDEFGHIJKLMNOPQRSTUVWXYZ%;s%[^_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789]%_%g'`
  echo "#ifdef HAVE_$ac_auto_include_have" &gt;&gt; $1
  echo "# include &lt;$ac_auto_include_file&gt;" &gt;&gt; $1
  echo "#endif" &gt;&gt; $1
  echo "" &gt;&gt; $1
done])

## ------------------##
## CONFIGURE_PART    ##
## ------------------##
AC_DEFUN([CONFIGURE_PART],[
case $TERM in
       #   for the most important terminal types we directly know the sequences
       xterm|xterm*|vt220|vt220*)
               T_MD=`awk 'BEGIN { printf("%c%c%c%c", 27, 91, 49, 109); }' </dev/null 2>/dev/null`
               T_ME=`awk 'BEGIN { printf("%c%c%c", 27, 91, 109); }' </dev/null 2>/dev/null`
        ;;
       vt100|vt100*|cygwin)
               T_MD=`awk 'BEGIN { printf("%c%c%c%c%c%c", 27, 91, 49, 109, 0, 0); }' </dev/null 2>/dev/null`
               T_ME=`awk 'BEGIN { printf("%c%c%c%c%c", 27, 91, 109, 0, 0); }' </dev/null 2>/dev/null`
       ;;
       *)
               T_MD=''
               T_ME=''
       ;;
esac
  AC_MSG_RESULT()
  AC_MSG_RESULT([${T_MD}$1${T_ME}])
])

## ------------------##
## Check Make	     ##
## ------------------##
AC_DEFUN([CS_CHECK_GNU_MAKE], [AC_CACHE_CHECK([for GNU make], [ac_cv_GNU_MAKE],
   ac_cv_GNU_MAKE='Not Found' ;
   ac_cv_GNU_MAKE_VERSION_MAJOR=0 ;
   ac_cv_GNU_MAKE_VERSION_MINOR=0 ;
   for a in make gmake gnumake ; do
      if test -z "$a" ; then continue ; fi ;
      if ( sh -c "$a --version" 2> /dev/null | grep GNU  2>&1 > /dev/null ) ;  then
         ac_cv_GNU_MAKE=$a ;
         ac_cv_GNU_MAKE_VERSION_MAJOR=`$ac_cv_GNU_MAKE --version | grep "GNU Make" | cut -f3 -d' ' | cut -f1 -d'.'`
         ac_cv_GNU_MAKE_VERSION_MINOR=`$ac_cv_GNU_MAKE --version | grep "GNU Make" | cut -f2 -d'.' | cut -c1-2`
         break;
      fi
   done ;
) ;
if test  "x$ac_cv_GNU_MAKE" = "xNot Found"  ; then
   AC_MSG_ERROR( *** Please install GNU make.  It is required to build Asterisk!)
   exit 1
fi
AC_SUBST([GNU_MAKE], [$ac_cv_GNU_MAKE])
])

## ------------------##
## Get Version (DdG) ##
## ------------------##
AC_DEFUN([CS_GET_VERSION], [
#  AC_UNDEFINE("PACKAGE_BUGREPORT")
#  AC_UNDEFINE("PACKAGE_NAME")
#  AC_UNDEFINE("PACKAGE_STRING")
#  AC_UNDEFINE("PACKAGE_TARNAME")
#  AC_UNDEFINE("PACKAGE_VERSION")
  SCCP_BRANCH="unknown"
  SCCP_VERSION="unknown"
  SCCP_REVISION="unknown"

  CURRENT=`pwd`
  BASE=`dirname $ac_dir`
  cd $BASE >/dev/null
  . ./tools/versioncheck silent
  cd $CURRENT >/dev/null

  AC_DEFINE_UNQUOTED([SCCP_VERSION],  "${SCCP_VERSION}", [Define the SCCP Version])
  AC_DEFINE_UNQUOTED([SCCP_BRANCH],   "${SCCP_BRANCH}", [Define the SCCP Branch])
  AC_DEFINE_UNQUOTED([SCCP_REVISION], "${SCCP_REVISION}", [Define the SCCP Revision])
  AC_DEFINE_UNQUOTED([REPOS_TYPE], "${REPOS_TYPE}", [Define the REPOS TYPE])
  AC_SUBST([SCCP_VERSION])
  AC_SUBST([SCCP_BRANCH])
  AC_SUBST([SCCP_REVISION])
  AC_SUBST([REPOS_TYPE])
  if test "${SCCP_BRANCH}" = "TRUNK"; then
    PACKAGE_NAME="Chan_SCCP_${SCCP_VERSION}_${SCCP_BRANCH}"
    PACKAGE_VERSION="${SCCP_VERSION}_${SCCP_BRANCH}_r${SCCP_REVISION}"
  else
    PACKAGE_NAME="Chan_SCCP_${SCCP_VERSION}_(${SCCP_BRANCH})"
    PACKAGE_VERSION="${SCCP_VERSION}_${SCCP_BRANCH}"
  fi
  AC_SUBST([PACKAGE_NAME])
])

## -----------------------------------------------------##
## CHECK_ALIGNED_ACCESS_REQUIRED (i.e. sparc/ppc/alpha) ##
## -----------------------------------------------------##

AC_DEFUN([AX_CHECK_ALIGNED_ACCESS_REQUIRED],
[AC_CACHE_CHECK([if pointers to integers require aligned access],
  [ax_cv_have_aligned_access_required],
  [AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>

int main()
{
  char* string = malloc(40);
  int i;
  for (i=0; i < 40; i++) string[[i]] = i;
  {
     void* s = string;
     int* p = s+1;
     int* q = s+2;
   
     if (*p == *q) { return 1; }
  }
  return 0;
}
              ],
     [ax_cv_have_aligned_access_required=yes],
     [ax_cv_have_aligned_access_required=no],
     [ax_cv_have_aligned_access_required=no])
  ])
if test "$ax_cv_have_aligned_access_required" = yes ; then
  AC_DEFINE([HAVE_ALIGNED_ACCESS_REQUIRED], [1],
    [Define if pointers to integers require aligned access])
fi  
])

AC_DEFUN([AX_CHECK_UNALIGNED_BUSERROR],
[AC_CACHE_CHECK([whether uint64_t misalignment causes a buserror on this system],
  [ax_cv_have_unaligned_buserror],
  [AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
int main()
{
          uint64_t buf64[3];
          uint8_t *p;
          memcpy(buf64, "AAAAAAA" "BBBBBBBB" "CCCCCCCC", 24); /* 7+8+8+1 */
          p = (uint8_t*)&buf64[0] + 7;
          /* Either bad result or SIGBUS on some systems */
          return (*((uint64_t*)p) == 0x4242424242424242ull) ? 0 : 1;
}
              ],
     [ax_cv_have_unaligned_buserror=no],
     [ax_cv_have_unaligned_buserror=yes],
     [ax_cv_have_unaligned_buserror=no])
  ])
if test "$ax_cv_have_unaligned_buserror" = yes ; then
  AC_DEFINE([HAVE_UNALIGNED_BUSERROR], [1],
    [Define if pointers to unaligned uint64_t integers would cause buserror])
fi  
])

## ------------------##
## Doxygen Defaults. ##
## ------------------##

#DX_ENV=""
AC_DEFUN([DX_FEATURE_doc],  ON)
AC_DEFUN([DX_FEATURE_dot],  ON)
AC_DEFUN([DX_FEATURE_man],  OFF)
AC_DEFUN([DX_FEATURE_html], ON)
AC_DEFUN([DX_FEATURE_chm],  OFF)
AC_DEFUN([DX_FEATURE_chi],  OFF)
AC_DEFUN([DX_FEATURE_rtf],  OFF)
AC_DEFUN([DX_FEATURE_xml],  OFF)
AC_DEFUN([DX_FEATURE_pdf],  ON)
AC_DEFUN([DX_FEATURE_ps],   ON)

## --------------- ##
## Private macros. ##
## --------------- ##

# DX_ENV_APPEND(VARIABLE, VALUE)
# ------------------------------
# Append VARIABLE="VALUE" to DX_ENV for invoking doxygen.
AC_DEFUN([DX_ENV_APPEND], [AC_SUBST([DX_ENV], ["$DX_ENV $1='$2'"])])

# DX_DIRNAME_EXPR
# ---------------
# Expand into a shell expression prints the directory part of a path.
AC_DEFUN([DX_DIRNAME_EXPR],
         [[expr ".$1" : '\(\.\)[^/]*$' \| "x$1" : 'x\(.*\)/[^/]*$']])

# DX_IF_FEATURE(FEATURE, IF-ON, IF-OFF)
# -------------------------------------
# Expands according to the M4 (static) status of the feature.
AC_DEFUN([DX_IF_FEATURE], [ifelse(DX_FEATURE_$1, ON, [$2], [$3])])

# DX_REQUIRE_PROG(VARIABLE, PROGRAM)
# ----------------------------------
# Require the specified program to be found for the DX_CURRENT_FEATURE to work.
AC_DEFUN([DX_REQUIRE_PROG], [
AC_PATH_TOOL([$1], [$2])
if test "$DX_FLAG_DX_CURRENT_FEATURE$$1" = 1; then
    dnl AC_MSG_WARN([$2 not found - will not DX_CURRENT_DESCRIPTION])
    AC_SUBST([DX_FLAG_DX_CURRENT_FEATURE], 0)
fi
])

# DX_TEST_FEATURE(FEATURE)
# ------------------------
# Expand to a shell expression testing whether the feature is active.
AC_DEFUN([DX_TEST_FEATURE], [test "$DX_FLAG_$1" = 1])

# DX_CHECK_DEPEND(REQUIRED_FEATURE, REQUIRED_STATE)
# -------------------------------------------------
# Verify that a required features has the right state before trying to turn on
# the DX_CURRENT_FEATURE.
AC_DEFUN([DX_CHECK_DEPEND], [
test "$DX_FLAG_$1" = "$2" \
|| AC_MSG_ERROR([doxygen-DX_CURRENT_FEATURE ifelse([$2], 1,
                            requires, contradicts) doxygen-DX_CURRENT_FEATURE])
])

# DX_CLEAR_DEPEND(FEATURE, REQUIRED_FEATURE, REQUIRED_STATE)
# ----------------------------------------------------------
# Turn off the DX_CURRENT_FEATURE if the required feature is off.
AC_DEFUN([DX_CLEAR_DEPEND], [
test "$DX_FLAG_$1" = "$2" || AC_SUBST([DX_FLAG_DX_CURRENT_FEATURE], 0)
])


# DX_FEATURE_ARG(FEATURE, DESCRIPTION,
#                CHECK_DEPEND, CLEAR_DEPEND,
#                REQUIRE, DO-IF-ON, DO-IF-OFF)
# --------------------------------------------
# Parse the command-line option controlling a feature. CHECK_DEPEND is called
# if the user explicitly turns the feature on (and invokes DX_CHECK_DEPEND),
# otherwise CLEAR_DEPEND is called to turn off the default state if a required
# feature is disabled (using DX_CLEAR_DEPEND). REQUIRE performs additional
# requirement tests (DX_REQUIRE_PROG). Finally, an automake flag is set and
# DO-IF-ON or DO-IF-OFF are called according to the final state of the feature.
AC_DEFUN([DX_ARG_ABLE], [
    AC_DEFUN([DX_CURRENT_FEATURE], [$1])
    AC_DEFUN([DX_FLAG_DX_CURRENT_FEATURE], [DX_FLAG_$1])
    AC_DEFUN([DX_CURRENT_DESCRIPTION], [$2])
    AC_ARG_ENABLE(doxygen-$1,
                  [AS_HELP_STRING(DX_IF_FEATURE([$1], [--disable-doxygen-$1],
                                                      [--enable-doxygen-$1]),
                                  DX_IF_FEATURE([$1], [don't $2], [$2]))],
                  [
case "$enableval" in
#(
y|Y|yes|Yes|YES)
    AC_SUBST([DX_FLAG_$1], 1)
    $3
;; #(
n|N|no|No|NO)
    AC_SUBST([DX_FLAG_$1], 0)
;; #(
*)
    AC_MSG_ERROR([invalid value '$enableval' given to doxygen-$1])
;;
esac
], [
AC_SUBST([DX_FLAG_$1], [DX_IF_FEATURE([$1], 1, 0)])
$4
])
if DX_TEST_FEATURE([$1]); then
    $5
    :
fi
if DX_TEST_FEATURE([$1]); then
    AM_CONDITIONAL(DX_COND_$1, :)
    $6
    :
else
    AM_CONDITIONAL(DX_COND_$1, false)
    $7
    :
fi
])

## -------------- ##
## Public macros. ##
## -------------- ##

# DX_XXX_FEATURE(DEFAULT_STATE)
# -----------------------------
AC_DEFUN([DX_DOXYGEN_FEATURE], [AC_DEFUN([DX_FEATURE_doc],  [$1])])
AC_DEFUN([DX_MAN_FEATURE],     [AC_DEFUN([DX_FEATURE_man],  [$1])])
AC_DEFUN([DX_HTML_FEATURE],    [AC_DEFUN([DX_FEATURE_html], [$1])])
AC_DEFUN([DX_CHM_FEATURE],     [AC_DEFUN([DX_FEATURE_chm],  [$1])])
AC_DEFUN([DX_CHI_FEATURE],     [AC_DEFUN([DX_FEATURE_chi],  [$1])])
AC_DEFUN([DX_RTF_FEATURE],     [AC_DEFUN([DX_FEATURE_rtf],  [$1])])
AC_DEFUN([DX_XML_FEATURE],     [AC_DEFUN([DX_FEATURE_xml],  [$1])])
AC_DEFUN([DX_XML_FEATURE],     [AC_DEFUN([DX_FEATURE_xml],  [$1])])
AC_DEFUN([DX_PDF_FEATURE],     [AC_DEFUN([DX_FEATURE_pdf],  [$1])])
AC_DEFUN([DX_PS_FEATURE],      [AC_DEFUN([DX_FEATURE_ps],   [$1])])

# DX_INIT_DOXYGEN(PROJECT, [CONFIG-FILE], [OUTPUT-DOC-DIR])
# ---------------------------------------------------------
# PROJECT also serves as the base name for the documentation files.
# The default CONFIG-FILE is "Doxyfile" and OUTPUT-DOC-DIR is "doxygen-doc".
AC_DEFUN([DX_INIT_DOXYGEN], [

# Files:
AC_SUBST([DX_PROJECT], [$1])
AC_SUBST([DX_CONFIG], [ifelse([$2], [], Doxyfile, [$2])])
AC_SUBST([DX_DOCDIR], [ifelse([$3], [], doxygen-doc, [$3])])

# Environment variables used inside doxygen.cfg:
DX_ENV_APPEND(SRCDIR, $srcdir)
DX_ENV_APPEND(PROJECT, $DX_PROJECT)
DX_ENV_APPEND(DOCDIR, $DX_DOCDIR)
DX_ENV_APPEND(VERSION, $PACKAGE_VERSION)

# Doxygen itself:
DX_ARG_ABLE(doc, [generate any doxygen documentation],
            [],
            [],
            [DX_REQUIRE_PROG([DX_DOXYGEN], doxygen)
             DX_REQUIRE_PROG([DX_PERL], perl)],
            [DX_ENV_APPEND(PERL_PATH, $DX_PERL)])

# Dot for graphics:
DX_ARG_ABLE(dot, [generate graphics for doxygen documentation],
            [DX_CHECK_DEPEND(doc, 1)],
            [DX_CLEAR_DEPEND(doc, 1)],
            [DX_REQUIRE_PROG([DX_DOT], dot)],
            [DX_ENV_APPEND(HAVE_DOT, YES)
             DX_ENV_APPEND(DOT_PATH, [`DX_DIRNAME_EXPR($DX_DOT)`])],
            [DX_ENV_APPEND(HAVE_DOT, NO)])

# Man pages generation:
DX_ARG_ABLE(man, [generate doxygen manual pages],
            [DX_CHECK_DEPEND(doc, 1)],
            [DX_CLEAR_DEPEND(doc, 1)],
            [],
            [DX_ENV_APPEND(GENERATE_MAN, YES)],
            [DX_ENV_APPEND(GENERATE_MAN, NO)])

# RTF file generation:
DX_ARG_ABLE(rtf, [generate doxygen RTF documentation],
            [DX_CHECK_DEPEND(doc, 1)],
            [DX_CLEAR_DEPEND(doc, 1)],
            [],
            [DX_ENV_APPEND(GENERATE_RTF, YES)],
            [DX_ENV_APPEND(GENERATE_RTF, NO)])

# XML file generation:
DX_ARG_ABLE(xml, [generate doxygen XML documentation],
            [DX_CHECK_DEPEND(doc, 1)],
            [DX_CLEAR_DEPEND(doc, 1)],
            [],
            [DX_ENV_APPEND(GENERATE_XML, YES)],
            [DX_ENV_APPEND(GENERATE_XML, NO)])

# (Compressed) HTML help generation:
DX_ARG_ABLE(chm, [generate doxygen compressed HTML help documentation],
            [DX_CHECK_DEPEND(doc, 1)],
            [DX_CLEAR_DEPEND(doc, 1)],
            [DX_REQUIRE_PROG([DX_HHC], hhc)],
            [DX_ENV_APPEND(HHC_PATH, $DX_HHC)
             DX_ENV_APPEND(GENERATE_HTML, YES)
             DX_ENV_APPEND(GENERATE_HTMLHELP, YES)],
            [DX_ENV_APPEND(GENERATE_HTMLHELP, NO)])

# Seperate CHI file generation.
DX_ARG_ABLE(chi, [generate doxygen seperate compressed HTML help index file],
            [DX_CHECK_DEPEND(chm, 1)],
            [DX_CLEAR_DEPEND(chm, 1)],
            [],
            [DX_ENV_APPEND(GENERATE_CHI, YES)],
            [DX_ENV_APPEND(GENERATE_CHI, NO)])

# Plain HTML pages generation:
DX_ARG_ABLE(html, [generate doxygen plain HTML documentation],
            [DX_CHECK_DEPEND(doc, 1) DX_CHECK_DEPEND(chm, 0)],
            [DX_CLEAR_DEPEND(doc, 1) DX_CLEAR_DEPEND(chm, 0)],
            [],
            [DX_ENV_APPEND(GENERATE_HTML, YES)],
            [DX_TEST_FEATURE(chm) || DX_ENV_APPEND(GENERATE_HTML, NO)])

# PostScript file generation:
DX_ARG_ABLE(ps, [generate doxygen PostScript documentation],
            [DX_CHECK_DEPEND(doc, 1)],
            [DX_CLEAR_DEPEND(doc, 1)],
            [DX_REQUIRE_PROG([DX_LATEX], latex)
             DX_REQUIRE_PROG([DX_MAKEINDEX], makeindex)
             DX_REQUIRE_PROG([DX_DVIPS], dvips)
             DX_REQUIRE_PROG([DX_EGREP], egrep)])

# PDF file generation:
DX_ARG_ABLE(pdf, [generate doxygen PDF documentation],
            [DX_CHECK_DEPEND(doc, 1)],
            [DX_CLEAR_DEPEND(doc, 1)],
            [DX_REQUIRE_PROG([DX_PDFLATEX], pdflatex)
             DX_REQUIRE_PROG([DX_MAKEINDEX], makeindex)
             DX_REQUIRE_PROG([DX_EGREP], egrep)])

# LaTeX generation for PS and/or PDF:
if DX_TEST_FEATURE(ps) || DX_TEST_FEATURE(pdf); then
    AM_CONDITIONAL(DX_COND_latex, :)
    DX_ENV_APPEND(GENERATE_LATEX, YES)
else
    AM_CONDITIONAL(DX_COND_latex, false)
    DX_ENV_APPEND(GENERATE_LATEX, NO)
fi

# Paper size for PS and/or PDF:
AC_ARG_VAR(DOXYGEN_PAPER_SIZE,
           [a4wide (default), a4, letter, legal or executive])
case "$DOXYGEN_PAPER_SIZE" in
#(
"")
    AC_SUBST(DOXYGEN_PAPER_SIZE, "")
;; #(
a4wide|a4|letter|legal|executive)
    DX_ENV_APPEND(PAPER_SIZE, $DOXYGEN_PAPER_SIZE)
;; #(
*)
    AC_MSG_ERROR([unknown DOXYGEN_PAPER_SIZE='$DOXYGEN_PAPER_SIZE'])
;;
esac

#For debugging:
#echo DX_FLAG_doc=$DX_FLAG_doc
#echo DX_FLAG_dot=$DX_FLAG_dot
#echo DX_FLAG_man=$DX_FLAG_man
#echo DX_FLAG_html=$DX_FLAG_html
#echo DX_FLAG_chm=$DX_FLAG_chm
#echo DX_FLAG_chi=$DX_FLAG_chi
#echo DX_FLAG_rtf=$DX_FLAG_rtf
#echo DX_FLAG_xml=$DX_FLAG_xml
#echo DX_FLAG_pdf=$DX_FLAG_pdf
#echo DX_FLAG_ps=$DX_FLAG_ps
#echo DX_ENV=$DX_ENV
])

dnl -------------------------------------------------------------------------
AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1 -Werror"
  AC_COMPILE_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS

AC_DEFUN([AX_APPEND_FLAG], [
AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_SET_IF
AS_VAR_PUSHDEF([FLAGS], [m4_default($2,_AC_LANG_PREFIX[FLAGS])])
AS_VAR_SET_IF(FLAGS,[
  AS_CASE([" AS_VAR_GET(FLAGS) "],
    [*" $1 "*], [AC_RUN_LOG([: FLAGS already contains $1])],
    [
     AS_VAR_APPEND(FLAGS," $1")
     AC_RUN_LOG([: FLAGS="$FLAGS"])
    ])
  ],
  [
  AS_VAR_SET(FLAGS,[$1])
  AC_RUN_LOG([: FLAGS="$FLAGS"])
  ])
AS_VAR_POPDEF([FLAGS])dnl
])dnl AX_APPEND_FLAG

AC_DEFUN([AX_APPEND_COMPILE_FLAGS],
[
for flag in $1; do
  AX_CHECK_COMPILE_FLAG([$flag], [AX_APPEND_FLAG([$flag], [$2])], [], [$3])
done
])dnl AX_APPEND_COMPILE_FLAGS

AC_DEFUN([AX_CHECK_LINK_FLAG], [
AC_PREREQ(2.64)
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_ldflags_$4_$1])dnl
AC_CACHE_CHECK([whether the linker accepts $1], CACHEVAR, [
  ax_check_save_flags=$LDFLAGS
  LDFLAGS="$LDFLAGS $4 $1"
  AC_LINK_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  LDFLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_LINK_FLAGS

AC_DEFUN([AX_APPEND_LINK_FLAGS], [
for flag in $1; do
  AX_CHECK_LINK_FLAG([$flag], [AX_APPEND_FLAG([$flag], [m4_default([$2], [LDFLAGS])])], [], [$3], [$4])
done
])dnl AX_APPEND_LINK_FLAGS


