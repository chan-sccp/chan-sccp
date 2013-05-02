
AC_DEFUN([CS_SETUP_DEFAULTS], [
	ac_default_prefix=/usr
	if test ${sysconfdir} = '${prefix}/etc'; then
	  sysconfdir=/etc
	fi
	if test ${mandir} = '${prefix}/man'; then
	  mandir=/usr/share/man
	fi
])

AC_DEFUN([CS_SETUP_BUILD],[

	AC_PATH_TOOL([UNAME], [uname], No)
	AC_PATH_PROGS(DATE,date,No)
	AC_PATH_PROGS(UNAME,uname,No)
	AC_PATH_PROGS(WHOAMI,whoami,No)
	AC_PATH_PROGS(FINGER,finger,No)
	AC_PATH_PROGS(HEAD,head,No)
	AC_PATH_PROGS(CUT,cut,No)
	AC_PATH_PROGS(AWK,awk,No)

	if test ! x"${UNAME}" = xNo; then
	    if test -n $BUILD_OS ; then
		BUILD_DATE="`date -u "+%Y-%m-%d %H:%M:%S"` UTC"
		BUILD_OS="`${UNAME} -s`"
		BUILD_MACHINE="`${UNAME} -m`"
		BUILD_HOSTNAME="`${UNAME} -n`"
		BUILD_KERNEL="`${UNAME} -r`"
		if test "_${AWK}" != "_No" && test "_${FINGER}" != "_No"; then
			BUILD_USER="`${FINGER} -lp |${HEAD} -n1 |${CUT} -d: -f3|${CUT} -c 2-`"
dnl			BUILD_USER="`${FINGER} -lp |${HEAD} -n1 |${AWK} -F:\  '{print $3}'`"
		elif test ! -z "${USERNAME}"; then
			BUILD_USER="${USERNAME}"		
		else
			BUILD_USER="`${WHOAMI}`"
		fi
		AC_DEFINE_UNQUOTED([BUILD_DATE],"`echo ${BUILD_DATE}`",[The date of this build])
		AC_DEFINE_UNQUOTED([BUILD_OS],"`echo ${BUILD_OS}`",[Operating System we are building on])
		AC_DEFINE_UNQUOTED([BUILD_MACHINE],"`echo ${BUILD_MACHINE}`",[Machine we are building on])
		AC_DEFINE_UNQUOTED([BUILD_HOSTNAME],"`echo ${BUILD_HOSTNAME}`",[Hostname of our Box])
		AC_DEFINE_UNQUOTED([BUILD_KERNEL],"`echo ${BUILD_KERNEL}`",[Kernel version of this build])
		AC_DEFINE_UNQUOTED([BUILD_USER],"`echo ${BUILD_USER}`",[building user])
		AC_SUBST([BUILD_DATE])
		AC_SUBST([BUILD_OS])
		AC_SUBST([BUILD_MACHINE])
		AC_SUBST([BUILD_HOSTNAME])
		AC_SUBST([BUILD_KERNEL])
		AC_SUBST([BUILD_USER])
	    fi
	fi
])

AC_DEFUN([CS_SETUP_HOST_PLATFORM],[
	astlibdir=''
	case "${host}" in
	  *-*-darwin*)
		AC_DEFINE([__Darwin__],1,[Using Darwin / Apple OSX])
		AC_DEFINE([DARWIN],1,[Using Darwin / Apple OSX])
		AC_SUBST(__Darwin__)
		use_poll_compat=yes
		no_libcap=yes
		ostype=bsd
		;;
	  *-*-freebsd*)
	    	AC_DEFINE([BSD], 1, [using BSD])
		use_poll_compat=yes
		no_libcap=yes
		ostype=bsd
		;;
	  *-*-netbsd*)
	    	AC_DEFINE([BSD], 1, [using BSD])
		use_poll_compat=yes
		no_libcap=yes
		ostype=bsd
		;;
	  *-*-openbsd*)
	    	AC_DEFINE([BSD], 1, [using BSD])
		use_poll_compat=yes
		no_libcap=yes
		ostype=bsd
		;;
	  *-*-dragonfly*)
	    	AC_DEFINE([BSD], 1, [using BSD])
		use_poll_compat=yes
		no_libcap=yes
		ostype=bsd
		;;
	  *-aix*)
	    	AC_DEFINE([AIX], 1, [using IAX])
		use_poll_compat=yes
		broken_types=yes
		no_libcap=yes
		ostype=aix
	    ;;
	  *-osf4*)
		AC_DEFINE([OSF1], 1, [using my beloved Digital OS])
		use_poll_compat=yes
		tru64_types=yes
		no_libcap=yes
		ostype=osf
	    ;;
	  *-osf5.1*)
		AC_DEFINE([OSF1], 1, [using my beloved Digital OS])
		use_poll_compat=yes
		no_libcap=yes
		ostype=osf
	    ;;
	  *-tru64*)
		AC_DEFINE([OSF1], 1, [using my beloved Digital OS])
		use_poll_compat=yes
		tru64_types=yes
		no_libcap=yes
		ostype=osf
	    ;;
	  *-*-linux*)
		AC_DEFINE([LINUX],[1],[using LINUX])
		LARGEFILE_FLAGS="-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
		CFLAGS_saved="$CFLAGS_saved $LARGEFILE_FLAGS"
		ostype=linux
		;;
	  *cygwin*)
		AC_DEFINE([_WIN32_WINNT],[0x0500],[Heya, it's windows])
		AC_DEFINE([INET_ADDRSTRLEN],[16],[cygwin detection does not work for that, anybody has an idea ?])
		AC_DEFINE([INET6_ADDRSTRLEN],[46],[cygwin detection does not work for that, anybody has an idea ?])
		AC_DEFINE([__CYGWIN__],[1],[make sure __CYGWIN__ is defined ...])
		ostype=cygwin
		;;
	  *-*-solaris2*)
		AC_DEFINE([SOLARIS],[1],[using SOLARIS])
		use_poll_compat=yes
		no_libcap=yes
		ostype=solaris
		force_generic_timers=yes
		;;
	  *)
		AC_MSG_RESULT(["Unsupported/Unknown operating system: ${host}"])
		use_poll_compat=yes
		no_libcap=yes
		ostype=unknown
		;;
	esac
	if test "x$use_poll_compat" = "xyes"; then
		AC_DEFINE([CS_USE_POLL_COMPAT], 1, [Define to 1 if internal poll should be used.])
	fi
	AC_SUBST(ostype)
])

AC_DEFUN([CS_SETUP_ENVIRONMENT], [
	AC_LANG_SAVE
	AC_LANG_C
	AC_SYS_LARGEFILE
	AC_DISABLE_STATIC
	AC_FUNC_ALLOCA
	AC_HEADER_RESOLV
dnl	AC_GNU_SOURCE

dnl	CFLAGS_saved="$CFLAGS_saved -std=gnu89"
	CFLAGS_saved="$CFLAGS_saved -std=gnu99 -fgnu89-inline"

	if test "${cross_compiling}" = "yes"; 
	then
		AC_CHECK_TOOL(CC, gcc, :)
		AC_CHECK_TOOL(LD, ld, :)
		AC_CHECK_TOOL(RANLIB, ranlib, :)
	fi
])

AC_DEFUN([CS_FIND_PROGRAMS], [
	AC_LANG_SAVE
	AC_LANG_C
	AC_PATH_PROGS(SVN, svn, [echo Missing subversion],${PATH}:/opt/csw/bin)
	AC_PATH_PROGS(SVNVERSION, svnversion, [echo Missing subversion],${PATH}:/opt/csw/bin)
	AC_PATH_PROGS(GIT, git, [echo Missing git],${PATH}:/opt/csw/bin)
	AC_PATH_PROGS(HG, hg, [echo Missing mercurial],${PATH}:/opt/csw/bin)
	AC_PATH_PROGS(SHELL,bash sh,[echo No compatible shell found])
	AC_PATH_PROGS(SH,bash sh,[echo No compatible shell found])
	AC_PATH_PROGS(M4,gm4 m4,[echo No m4 found, who will process my macros now ?])
	AC_PATH_PROGS(GREP, ggrep grep,[echo Missing grep so skipping but I doubt we will get anywhere])
	AC_PATH_PROGS(SED,gsed sed,[echo sed not found, doh!])
	AC_PATH_PROGS(CAT,cat,[echo cat not found, Doh!])
	AC_PATH_PROGS(TR,tr,[echo tr not found, need this for upper/lowercase conversions])
	AC_PATH_PROGS(UNAME,uname,[echo uname not found so no version info will be available])
	AC_PATH_PROGS(WHOAMI,whoami,[echo whoami not found so no version info will be available])
	AC_PATH_PROGS(RPMBUILD,rpmbuild,[echo rpmbuild not found so you cannot build rpm packages (no problem)])
	AC_PATH_PROGS(OBJCOPY,objcopy,[echo objcopy not found so we can not safe debug information (no problem)])
	AC_PATH_PROGS(GDB,gdb,[echo gdb not found so we can not generate backtraces (no problem)])
	AC_PROG_CC
	AC_PROG_CC_C_O
	AC_PROG_GCC_TRADITIONAL
	AC_PROG_CPP
	AC_PROG_INSTALL
	AC_PROG_AWK
	AC_PROG_LN_S
	AC_PROG_MAKE_SET
	AC_FUNC_STRERROR_R
	AC_C_CONST
	AC_C_INLINE
	AC_PROG_LIBTOOL
	AC_SUBST(SVN)
	AC_SUBST(SVNVERSION)
	AC_SUBST(GIT)
	AC_SUBST(HG)
	AC_SUBST(GREP)
	AC_SUBST(RPMBUILD)
	AC_SUBST(OBJCOPY)
	AC_SUBST(GDB)
])

AC_DEFUN([CS_FIND_LIBRARIES], [
	LIBS_save=$LIBS
	LIBS="$LIBS"
	AC_CHECK_LIB([c], [main])
	AC_CHECK_LIB([pthread], [main])
	AC_CHECK_LIB([socket], [main])
	AC_CHECK_HEADERS([sys/ioctl.h]) 
	AC_CHECK_HEADERS([sys/socket.h])
	AC_CHECK_HEADERS([netinet/in.h])
	AC_CHECK_HEADERS([pthread.h])
	AC_CHECK_FUNCS([gethostbyname inet_ntoa memset mkdir select socket strsep strcasecmp strchr strdup strerror strncasecmp strerror strchr malloc calloc realloc free]) 
	AC_HEADER_STDC    
	AC_HEADER_STDBOOL 
	AC_CHECK_HEADERS([netinet/in.h fcntl.h sys/signal.h stdio.h errno.h ctype.h assert.h sys/sysinfo.h])
	AC_STRUCT_TM
	AC_STRUCT_TIMEZONE
])

AC_DEFUN([CS_CHECK_CROSSCOMPILE],[
	dnl cross-compile checks (set HOST_CC)
	if test "$host" = "$build"; then
		HOST_CC="${CC}"
	else
		HOST_CC="${HOST_CC-gcc}"
	fi
dnl	AC_CHECK_PROG(have_host_cc, ${HOST_CC}, yes, no)
dnl	if test "$have_host_cc" = "no"; then
dnl		AC_MSG_ERROR(No valid host compiler set with HOST_CC)
dnl	fi
	AC_SUBST(HOST_CC)
])

AC_DEFUN([CS_WITH_CCACHE],[
	dnl Compile With CCACHE
	AC_ARG_WITH(ccache,
	  AC_HELP_STRING([--with-ccache[=PATH]], [use ccache during compile]), [ac_cv_use_ccache="${withval}"], [ac_cv_use_ccache="no"])
	AS_IF([test "_${ac_cv_use_ccache}" != "_no"], [
		if test "${ac_cv_use_ccache}" = "yes"; then
			AC_PATH_PROGS(CCACHE,ccache,[echo ccache not found])
			if test -n "${CCACHE}"; then
				CC="$CCACHE $CC"    
				CPP="$CCACHE $CPP"  
				AC_SUBST([CC])
				AC_SUBST([CPP])
				AC_SUBST([CCACHE])
			fi
		else
			if -f "${ac_cv_use_ccache}"; then
				CCACHE="${ac_cv_use_ccache}"
				CC="$CCACHE $CC"  
				CPP="$CCACHE $CPP"
				AC_SUBST([CC])
				AC_SUBST([CPP])
				AC_SUBST([CCACHE])
				AC_MSG_NOTICE([using ccache: ${ac_cv_use_ccache}])
			else
				CCACHE=""
				echo ccache not found
			fi
		fi
	])
])

AC_DEFUN([CS_SETUP_LIBTOOL], [
	dnl LibTool
	CONFIGURE_PART([Checking Libtool:])
	LT_PREREQ([2.0.0])
	LT_INIT([dlopen])
	LTDL_INIT([])
	if test "x$with_included_ltdl" != "xyes"; then
	  save_CFLAGS="${CFLAGS}"
	  save_CPPFLAGS="${CPPFLAGS}"
	  save_LDFLAGS="${LDFLAGS}"
	  CFLAGS="${CFLAGS} ${LTDLINCL}"
	  LDFLAGS="${LDFLAGS} ${LIBLTDL}"
	  AC_CHECK_LIB([ltdl], [lt_dladvise_init],[],[AC_MSG_ERROR([installed libltdl is too old])])
	  LDFLAGS="${save_LDFLAGS}"
	  CFLAGS="${save_CFLAGS}"
	fi
	AC_SUBST([LIBTOOL_DEPS])
])

AC_DEFUN([CS_CHECK_TYPES], [ 
	dnl check compiler specifics
	AC_C_INLINE
	AC_C_CONST 
	AC_C_VOLATILE
	AC_TYPE_SIZE_T
	AC_TYPE_SSIZE_T
	AC_TYPE_INT8_T
	AC_TYPE_INT16_T
	AC_TYPE_INT32_T
	AC_TYPE_INT64_T
	AC_TYPE_UINT8_T
	AC_TYPE_UINT16_T
	AC_TYPE_UINT32_T
	AC_TYPE_UINT64_T
	AC_FUNC_FSEEKO
dnl	AC_FUNC_MALLOC
dnl	AC_FUNC_REALLOC

	dnl check declarations
	AC_CHECK_DECLS(INET_ADDRSTRLEN,[],[],[#if HAVE_NETINET_IN_H
	# include <netinet/in.h>
	# endif
	#if HAVE_W32API_WS32TCPIP_H
	# include w32api/windows.h
	# include w32api/winsock2.h  
	# include w32api/ws2tcpip.h
	#endif
	])
	AC_CHECK_DECLS(INET6_ADDRSTRLEN,[],[],[#if HAVE_NETINET_IN_H
	# include <netinet/in.h>
	# endif
	#if HAVE_W32API_WS32TCPIP_H
	# include w32api/windows.h
	# include w32api/winsock2.h
	# include w32api/ws2tcpip.h
	#endif
	])   
	# more type checks, horrible shit
	if test "$tru64_types" = "yes"; then
		AC_CHECK_TYPE(u_int8_t, unsigned char)
		AC_CHECK_TYPE(u_int16_t, unsigned short)
		AC_CHECK_TYPE(u_int32_t, unsigned int)
	else
		if test "$broken_types" = "yes" ; then
			AC_CHECK_TYPE(u_int8_t, unsigned char)
			AC_CHECK_TYPE(u_int16_t, unsigned short)
			AC_CHECK_TYPE(u_int32_t, unsigned long int)
		else
			AC_CHECK_TYPE(u_int8_t, uint8_t)
			AC_CHECK_TYPE(u_int16_t, uint16_t)
			AC_CHECK_TYPE(u_int32_t, uint32_t)
		fi
	fi
dnl replaced AC_CHECK_SIZEOF
#	AC_CHECK_SIZEOF([int])
#	AC_CHECK_SIZEOF([long])
#	AC_CHECK_SIZEOF([long long])
#	AC_CHECK_SIZEOF([char *])
dnl with
        AC_MSG_CHECKING([sizeof(long long) == sizeof(long)])
        AC_RUN_IFELSE([AC_LANG_SOURCE([
            int main(void)
            {
                    if (sizeof(long long) == sizeof(long))
                    	return 0;
		    else
		    	return -1;
            }
        ])], [
                AC_DEFINE(ULONG, [long unsigned int], [Define ULONG as long unsigned int])
                AC_DEFINE(UI64FMT, ["%lu"], [Define UI64FMT as "%lu"])
                AC_MSG_RESULT([yes])
        ],  [
                AC_DEFINE(ULONG, [long long unsigned int], [Define ULONG as long long unsigned int])
                AC_DEFINE(UI64FMT, ["%llu"], [Define UI64FMT as "%llu"])
                AC_MSG_RESULT([no])
        ])
dnl end replace
	# Big Endian / Little Endian	
	AC_C_BIGENDIAN(AC_DEFINE([__BYTE_ORDER],__BIG_ENDIAN,[Big Endian]),AC_DEFINE([__BYTE_ORDER],__LITTLE_ENDIAN,[Little Endian]))
	AC_C_BIGENDIAN(AC_DEFINE(SCCP_BIG_ENDIAN,1,[SCCP_BIG_ENDIAN]),AC_DEFINE(SCCP_LITTLE_ENDIAN,1,[SCCP_LITTLE_ENDIAN]))
	AC_C_BIGENDIAN(AC_DEFINE(SCCP_PLATFORM_BYTE_ORDER,SCCP_BIG_ENDIAN,[SCCP_PLATFORM_BYTE_ORDER]),AC_DEFINE(SCCP_PLATFORM_BYTE_ORDER,SCCP_LITTLE_ENDIAN,[SCCP_PLATFORM_BYTE_ORDER]))
	AC_DEFINE([__LITTLE_ENDIAN],1234,[for the places where it is not defined])
	AC_DEFINE([__BIG_ENDIAN],4321,[for the places where it is not defined])

        AC_CHECK_HEADERS([byteswap.h sys/endian.h sys/byteorder.h], [break])
        # Even if we have byteswap.h, we may lack the specific macros/functions.
        if test x$ac_cv_header_byteswap_h = xyes ; then
                m4_foreach([FUNC], [bswap_16,bswap_32,bswap_64], [
                        AC_MSG_CHECKING([if FUNC is available])
                        AC_LINK_IFELSE([AC_LANG_SOURCE([
                            #include <byteswap.h>
                            int
                            main(void)
                            {
                                    FUNC[](42);
                                    return 0;
                            }
                        ])], [
                                AC_DEFINE(HAVE_[]m4_toupper(FUNC), [1],
                                                [Define to 1 if] FUNC [is available.])
                                AC_MSG_RESULT([yes])
                        ], [AC_MSG_RESULT([no])])
                ])dnl
        fi
])

AC_DEFUN([CS_CHECK_SVN2CL], [
	enable_svn2cl=yes
	AC_PATH_PROG([SVN2CL], [svn2cl])
	if test -z "$SVN2CL"; then
	  enable_svn2cl=no
	fi
	AM_CONDITIONAL(ENABLE_SVN2CL, test x$enable_svn2cl != xno)
])

AC_DEFUN([CS_WITH_CHANGELOG_OLDEST], [
	AC_ARG_WITH(changelog-oldest,
	    AC_HELP_STRING([--with-changelog-oldest=NUMBER], [Oldest revision to include in ChangeLog])
	)
	CHANGELOG_OLDEST=3365
	if test "x$with_changelog_oldest" != "x" ; then
	  CHANGELOG_OLDEST=$with_changelog_oldest
	fi
	AC_SUBST(CHANGELOG_OLDEST) 
])

dnl Conditional Makefile.am Macros
AC_DEFUN([AST_SET_PBX_AMCONDITIONALS],[
	AM_CONDITIONAL([BUILD_AST], test "$PBX_TYPE" == "Asterisk")
	dnl Now using Conditional-Libtool-Sources
	if test "$PBX_TYPE" == "Asterisk"; then
		PBX_GENERAL="chan_sccp_la-ast.lo"
		if [ test "${ASTERISK_REPOS_LOCATION}" == "TRUNK" ];then
                  PBX_MAJOR="chan_sccp_la-astTrunk.lo"
                else  
	  	  PBX_MAJOR="chan_sccp_la-ast${ASTERISK_VER_GROUP}.lo"
	  	fi
                if test -f src/pbx_impl/ast/ast${ASTERISK_VERSION_NUMBER}.c; then
                  PBX_MINOR="chan_sccp_la-ast${ASTERISK_VERSION_NUMBER}.lo"
                else
                  PBX_MINOR=""
                fi
	else
		PBX_GENERAL=""
		PBX_MAJOR=""
		PBX_MINOR=""
	fi
	AC_SUBST([PBX_GENERAL])
	AC_SUBST([PBX_MAJOR])
	AC_SUBST([PBX_MINOR])
])

AC_DEFUN([CS_WITH_PBX], [
	AC_ARG_WITH([asterisk],
	    [AC_HELP_STRING([--with-asterisk=PATH],[Location of the Asterisk installation])],[NEW_PBX_PATH="${withval}"],)

	if test "x${NEW_PBX_PATH}" != "xyes" && test "x${NEW_PBX_PATH}" != "x"; then 
		PBX_PATH="${NEW_PBX_PATH}";
	else
		PBX_PATH="/usr /usr/local /opt"
		if test -d /usr/pkg ; then
			PBX_PATH="$PBX_PATH /usr/pkg"
		fi
		if test -d /usr/sfw ; then
			PBX_PATH="$PBX_PATH /usr/sfw"
		fi
	fi
	   
	AC_SUBST([PBX_PATH])
	PBX_MANDATORY="yes"
	
	CS_CHECK_PBX

	if test "${PBX_TYPE}" = "Asterisk"; then
	   AC_DEFINE_UNQUOTED([PBX_TYPE],ASTERISK,[PBX Type])
	   AC_DEFINE([HAVE_ASTERISK], 1, [Uses Asterisk as PBX])
	   AST_GET_VERSION
	   AST_CHECK_HEADERS
	elif test "${PBX_TYPE}" = "Callweaver"; then
	   AC_DEFINE_UNQUOTED([PBX_TYPE],CALLWEAVER,[PBX Type])
	   AC_DEFINE([HAVE_CALLWEAVER], 1, [Uses Callweaver as PBX])
	   dnl Figure out the Asterisk Version
	   echo "We are working on a Callweaver version"
	else
	   echo ""
	   echo ""
	   echo "PBX type could not be determined"
	   echo "================================"
	   echo "Either install asterisk and asterisk-devel packages"
	   echo "Or specify the location where asterisk can be found, using ./configure --with-asterisk=[path]"
	   exit
	fi
	AST_SET_PBX_AMCONDITIONALS
	AC_SUBST([PBX_TYPE])
])

AC_DEFUN([CS_SETUP_DOXYGEN], [
	dnl Doxygen
	CONFIGURE_PART([Checking for Doxygen:])
	AC_ARG_ENABLE(devdoc, 
	  AC_HELP_STRING([--enable-devdoc], [enable developer documentation]), 
	  ac_cv_use_devdoc=$enableval, ac_cv_use_devdoc=no)
	AS_IF([test "_${ac_cv_use_devdoc}" == "_yes"], [DX_ENV_APPEND([INPUT],[. src src/pbx_impl src/pbx_impl/ast])])
	AC_MSG_NOTICE([--enable-devdoc: ${ac_cv_use_devdoc}])
	DX_HTML_FEATURE(ON)
	DX_CHM_FEATURE(OFF)
	DX_CHI_FEATURE(OFF)
	DX_MAN_FEATURE(OFF)
	DX_RTF_FEATURE(OFF)
	DX_XML_FEATURE(OFF)
	DX_PDF_FEATURE(OFF)
	DX_PS_FEATURE(OFF)
	DX_INIT_DOXYGEN($PACKAGE, doc/doxygen.cfg)
])


AC_DEFUN([CS_ENABLE_OPTIMIZATION], [
	AC_ARG_ENABLE(optimization, [AC_HELP_STRING([--disable-optimization],[no detection or tuning flags for cpu version])], enable_optimization=$enableval,enable_optimization=yes)
	AC_ARG_ENABLE(debug,[AC_HELP_STRING([--enable-debug],[enable debug information])],enable_debug=$enableval,enable_debug=no)
	AC_MSG_NOTICE([--enable-optimization: ${enable_optimization}])
	AC_MSG_NOTICE([--enable-debug: ${enable_debug}])

 	CPPFLAGS_saved="${CPPFLAGS_saved} -U_FORTIFY_SOURCE"
	if test "${SCCP_BRANCH}" == "TRUNK" || test "$enable_optimization" == "no" || test "${enable_debug}" = "yes"; then 
		strip_binaries="no"
		CFLAGS_saved="${CFLAGS_saved} -O0 "
	else
		strip_binaries="yes"
		CFLAGS_saved="${CFLAGS_saved} -O3 "
		GDB_FLAGS=""
                CPPFLAGS_saved="${CPPFLAGS_saved} -D_FORTIFY_SOURCE=2"
	fi

	if test "${enable_debug}" = "yes"; then
		AC_DEFINE([GC_DEBUG],[1],[Enable extra garbage collection debugging.])
		AC_DEFINE([DEBUG],[1],[Extra debugging.])
		DEBUG=1
		enable_do_crash="yes"
		enable_debug_mutex="yes"
		strip_binaries="no"

		CFLAGS_saved="`echo ${CFLAGS_saved}|${SED} 's/^[ \t]*//;s/[ \t]*$//'`" 	dnl Remove leading/ending spaces
		CFLAGS_saved="${CFLAGS_saved} -Wall"
		GDB_FLAGS="-g3 -ggdb3"
		if test "x${GCC}" = "xyes"; then
                        AX_CFLAGS_GCC_OPTION_NEW(-Wstrict-prototypes)
                        AX_CFLAGS_GCC_OPTION_NEW(-Wmissing-prototypes)
                        AX_CFLAGS_GCC_OPTION_NEW(-Wmissing-declarations)
                        AX_CFLAGS_GCC_OPTION_NEW(-Wnested-externs)
                        AX_CFLAGS_GCC_OPTION_NEW(-Wno-long-long)
			AX_CFLAGS_GCC_OPTION_NEW(-Wnonnull)
dnl			AX_CFLAGS_GCC_OPTION_NEW(-Wno-unused-but-set-variable)
dnl			AX_CFLAGS_GCC_OPTION_NEW(-Wno-unused-parameter)
			AX_CFLAGS_GCC_OPTION_NEW(-fstack-protector-all)
    		fi 
	else
		AC_DEFINE([DEBUG],[0],[No Extra debugging.])
		DEBUG=0
		enable_do_crash="no"
		enable_debug_mutex="no"
		CFLAGS_saved="${CFLAGS_saved}"
		if test "x${GCC}" = "xyes"; then
                        AX_CFLAGS_GCC_OPTION_NEW(-Wno-long-long)
                        AX_CFLAGS_GCC_OPTION_NEW(-Wno-unused-parameter)
dnl                        AX_CFLAGS_GCC_OPTION_NEW(-Wno-unused-but-set-variable)	// has negative side effect on certain platforms (http://xen.1045712.n5.nabble.com/xen-4-0-testing-test-7147-regressions-FAIL-td4415622.html)
                        AX_CFLAGS_GCC_OPTION_NEW(-Wno-ignored-qualifiers)
			AX_CFLAGS_GCC_OPTION_NEW(-fstack-protector)
		fi		
	fi
	CFLAGS_saved="${CFLAGS_saved} -I."		dnl include our own directory first, so that we can find config.h when using a builddir
	CFLAGS="${CFLAGS_saved}"
	CPPFLAGS="${CPPFLAGS_saved}"
	AC_SUBST([DEBUG])
	AC_SUBST([GDB_FLAGS])
])

AC_DEFUN([CS_ENABLE_DEBUG], [
	AC_MSG_NOTICE([--enable-debug: ${enable_debug}])
	AM_CONDITIONAL([WANT_DEBUG],[test "${enable_debug}" = "yes"])
	AC_SUBST([strip_binaries])
])

AC_DEFUN([CS_ENABLE_REFCOUNT_DEBUG], [
	AC_ARG_ENABLE(refcount_debug, 
	  AC_HELP_STRING([--enable-refcount-debug], [enable refcount debug]), 
	  ac_cv_refcount_debug=$enableval, ac_cv_refcount_debug=no)
	AS_IF([test "_${ac_cv_refcount_debug}" == "_yes"], [AC_DEFINE(CS_REFCOUNT_DEBUG, 1, [refcount debug enabled])])
	AC_MSG_NOTICE([--enable-refcount-debug: ${ac_cv_refcount_debug}])
])

AC_DEFUN([CS_DISABLE_PICKUP], [
	AC_ARG_ENABLE(pickup, 
	  AC_HELP_STRING([--disable-pickup], [disable pickup function]), 
	  ac_cv_use_pickup=$enableval, ac_cv_use_pickup=yes)
	AS_IF([test "_${ac_cv_use_pickup}" == "_yes"], [AC_DEFINE(CS_SCCP_PICKUP, 1, [pickup function enabled])])
	AC_MSG_NOTICE([--enable-pickup: ${ac_cv_use_pickup}])
])

AC_DEFUN([CS_DISABLE_PARK], [
	AC_ARG_ENABLE(park, 
	  AC_HELP_STRING([--disable-park], [disable park functionality]), 
	  ac_cv_use_park=$enableval, ac_cv_use_park=yes)
	AS_IF([test "_${ac_cv_use_park}" == "_yes"], [AC_DEFINE(CS_SCCP_PARK, 1, [park functionality enabled])])
	AC_MSG_NOTICE([--enable-park: ${ac_cv_use_park}])
])

AC_DEFUN([CS_DISABLE_DIRTRFR], [
	AC_ARG_ENABLE(dirtrfr, 
	  AC_HELP_STRING([--disable-dirtrfr], [disable direct transfer]), 
	  ac_cv_use_dirtrfr=$enableval, ac_cv_use_dirtrfr=yes)
	AS_IF([test "_${ac_cv_use_dirtrfr}" == "_yes"], [AC_DEFINE(CS_SCCP_DIRTRFR, 1, [direct transfer enabled])])
	AC_MSG_NOTICE([--enable-dirtrfr: ${ac_cv_use_dirtrfr}])
])

AC_DEFUN([CS_DISABLE_MONITOR], [
	AC_ARG_ENABLE(monitor, 
	  AC_HELP_STRING([--disable-monitor], [disable feature monitor)]), 
	  ac_cv_use_monitor=$enableval, ac_cv_use_monitor=yes)
	AS_IF([test "_${ac_cv_use_monitor}" == "_yes"], [AC_DEFINE(CS_SCCP_FEATURE_MONITOR, 1, [feature monitor enabled])])
	AC_MSG_NOTICE([--enable-monitor: ${ac_cv_use_monitor}])
])

AC_DEFUN([CS_ENABLE_CONFERENCE], [
	AC_ARG_ENABLE(conference, 
	  AC_HELP_STRING([--enable-conference], [enable conference (>ast 1.6.2)(experimental)]), 
	  ac_cv_use_conference=$enableval, ac_cv_use_conference=no)
	AS_IF([test "_${ac_cv_use_conference}" == "_yes"], [AC_DEFINE(CS_SCCP_CONFERENCE, 1, [conference enabled])])
	AC_MSG_NOTICE([--enable-conference: ${ac_cv_use_conference}])
])

AC_DEFUN([CS_DISABLE_MANAGER], [
	AC_ARG_ENABLE(manager, 
	  AC_HELP_STRING([--disable-manager], [disabled ast manager events]), 
	  ac_cv_use_manager=$enableval, ac_cv_use_manager=yes)
	AS_IF([test "_${ac_cv_use_manager}" == "_yes"], [
		AC_DEFINE(CS_MANAGER_EVENTS, 1, [manager events enabled])
		AC_DEFINE(CS_SCCP_MANAGER, 1, [manager console control enabled])
	])
	AC_MSG_NOTICE([--enable-manager: ${ac_cv_use_manager}])
])

AC_DEFUN([CS_DISABLE_FUNCTIONS], [
	AC_ARG_ENABLE(functions, 
	  AC_HELP_STRING([--disable-functions], [disabled Dialplan functions]), 
	  ac_cv_use_functions=$enableval, ac_cv_use_functions=yes)
	AS_IF([test "_${ac_cv_use_functions}" == "_yes"], [AC_DEFINE(CS_SCCP_FUNCTIONS, 1, [dialplan function enabled])])
	AC_MSG_NOTICE([--enable-functions: ${ac_cv_use_functions}])
])

AC_DEFUN([CS_ENABLE_INDICATIONS], [
	AC_ARG_ENABLE(indications, 
	  AC_HELP_STRING([--enable-indications], [enable debug indications]), 
	  ac_cv_debug_indications=$enableval, ac_cv_debug_indications=no)
	AS_IF([test "_${ac_cv_debug_indications}" == "_yes"], [AC_DEFINE(CS_DEBUG_INDICATIONS, 1, [debug indications enabled])])
	AC_MSG_NOTICE([--enable-indications: ${ac_cv_debug_indications}])
])

AC_DEFUN([CS_DISABLE_REALTIME], [
	AC_ARG_ENABLE(realtime, 
	  AC_HELP_STRING([--disable-realtime], [disable realtime support]), 
	  ac_cv_realtime=$enableval, ac_cv_realtime=yes)
	AS_IF([test "_${ac_cv_realtime}" == "_yes"], [AC_DEFINE(CS_SCCP_REALTIME, 1, [realtime enabled])])
	AC_MSG_NOTICE([--enable-realtime: ${ac_cv_realtime}])
])

AC_DEFUN([CS_DISABLE_FEATURE_MONITOR], [
	AC_ARG_ENABLE(feature_monitor, 
	  AC_HELP_STRING([--disable-feature-monitor], [disable feature monitor]), 
	  ac_cv_feature_monitor=$enableval, ac_cv_feature_monitor=yes)
	AS_IF([test "_${ac_cv_feature_monitor}" == "_yes"], [AC_DEFINE(CS_SCCP_FEATURE_MONITOR, 1, [feature monitor enabled])])
	AC_MSG_NOTICE([--enable-feature-monitor: ${ac_cv_feature_monitor}])
])

AC_DEFUN([CS_ENABLE_ADVANCED_FUNCTIONS], [
	AC_ARG_ENABLE(advanced_functions, 
	  AC_HELP_STRING([--enable-advanced-functions], [enable advanced functions (experimental)]), 
	    ac_cv_advanced_functions=$enableval, ac_cv_advanced_functions=no)
	AS_IF([test "_${ac_cv_advanced_functions}" == "_yes"], [AC_DEFINE(CS_ADV_FEATURES, 1, [advanced functions enabled])])
	AC_MSG_NOTICE([--enable-advanced-functions: ${ac_cv_advanced_functions}])
])

AC_DEFUN([CS_ENABLE_EXPERIMENTAL_MODE], [
	AC_ARG_ENABLE(experimental_mode, 
	  AC_HELP_STRING([--enable-experimental-mode], [enable experimental mode (only for developers)]), 
	    ac_cv_experimental_mode=$enableval, ac_cv_experimental_mode=no)
	AS_IF([test "_${ac_cv_experimental_mode}" == "_yes"], [AC_DEFINE(CS_EXPERIMENTAL, 1, [experimental mode enabled])])
	AC_MSG_NOTICE([--enable-experimental-mode: ${ac_cv_experimental_mode} (only for developers)])
])

AC_DEFUN([CS_ENABLE_EXPERIMENTAL_CODEC], [
	AC_ARG_ENABLE(experimental_codec, 
	  AC_HELP_STRING([--enable-experimental-codec], [enable experimental codec (only for developers)]), 
	    ac_cv_experimental_codec=$enableval, ac_cv_experimental_codec=no)
	AS_IF([test "_${ac_cv_experimental_codec}" == "_yes"], [AC_DEFINE(CS_EXPERIMENTAL_CODEC, 1, [experimental codec enabled])])
	AC_MSG_NOTICE([--enable-experimental-codec: ${ac_cv_experimental_codec} (only for developers)])
])

AC_DEFUN([CS_ENABLE_EXPERIMENTAL_RTP], [
	AC_ARG_ENABLE(experimental_rtp, 
	  AC_HELP_STRING([--enable-experimental-rtp], [enable experimental rtp (only for developers)]), 
	    ac_cv_experimental_rtp=$enableval, ac_cv_experimental_rtp=no)
	AS_IF([test "_${ac_cv_experimental_rtp}" == "_yes"], [AC_DEFINE(CS_EXPERIMENTAL_RTP, 1, [experimental rtp enabled])])
	AC_MSG_NOTICE([--enable-experimental-rtp: ${ac_cv_experimental_rtp} (only for developers)])
])

AC_DEFUN([CS_ENABLE_EXPERIMENTAL_NEWIP], [
	AC_ARG_ENABLE(experimental_newip, 
	  AC_HELP_STRING([--enable-experimental-newip], [enable experimental newip (only for developers)]), 
	    ac_cv_experimental_newip=$enableval, ac_cv_experimental_newip=no)
	AS_IF([test "_${ac_cv_experimental_newip}" == "_yes"], [AC_DEFINE(CS_EXPERIMENTAL_NEWIP, 1, [experimental newip enabled])])
	AC_MSG_NOTICE([--enable-experimental-newip: ${ac_cv_experimental_newip} (only for developers)])
])

AC_DEFUN([CS_DISABLE_DEVSTATE_FEATURE], [
	AC_ARG_ENABLE(devstate_feature, 
	  AC_HELP_STRING([--disable-devstate-feature], [disable device state feature button]), 
	    ac_cv_devstate_feature=$enableval, ac_cv_devstate_feature=yes)
	AS_IF([test ${ASTERISK_VERSION_NUMBER} -lt 10601], [ac_cv_devstate_feature=no])
	AS_IF([test "_${DEVICESTATE_H}" != "_yes"], [ac_cv_devstate_feature=no])
	AS_IF([test "_${ac_cv_devstate_feature}" == "_yes"], [AC_DEFINE(CS_DEVSTATE_FEATURE, 1, [devstate feature enabled])])
	AC_MSG_NOTICE([--enable-devstate-feature: ${ac_cv_devstate_feature}])
])

AC_DEFUN([CS_DISABLE_DYNAMIC_SPEEDDIAL], [
	AC_ARG_ENABLE(dynamic_speeddial, 
	  AC_HELP_STRING([--disable-dynamic-speeddial], [disable dynamic speeddials]), 
	    ac_cv_dynamic_speeddial=$enableval, ac_cv_dynamic_speeddial=yes)
	AS_IF([test "_${ac_cv_dynamic_speeddial}" == "_yes"], [AC_DEFINE(CS_DYNAMIC_SPEEDDIAL, 1, [dynamic speeddials enabled])])
	AC_MSG_NOTICE([--enable-dynamic-speeddial: ${ac_cv_dynamic_speeddial}])
])

AC_DEFUN([CS_DISABLE_DYNAMIC_SPEEDDIAL_CID], [
	AC_ARG_ENABLE(dynamic_speeddial_cid, 
	  AC_HELP_STRING([--disable-dynamic-speeddial-cid], [disable dynamic speeddials with call info]), 
            ac_cv_dynamic_speeddial_cid=$enableval, ac_cv_dynamic_speeddial_cid=${ac_cv_dynamic_speeddial})
        AS_IF([test "${ac_cv_dynamic_speeddial}" == "yes"], [
                AS_IF([test "${ac_cv_dynamic_speeddial_cid}" == "yes"], [
                        AC_DEFINE(CS_DYNAMIC_SPEEDDIAL_CID, 1, [dynamic speeddials with callinfo enabled])
                ])
        ])
	AC_MSG_NOTICE([--enable-dynamic-speeddial_cid: ${ac_cv_dynamic_speeddial_cid}])
])

AC_DEFUN([CS_ENABLE_VIDEO], [
	AC_ARG_ENABLE(video, 
	  AC_HELP_STRING([--enable-video], [enable streaming video (experimental)]), 
	  ac_cv_streaming_video=$enableval, ac_cv_streaming_video=no)
	AS_IF([test "_${ac_cv_streaming_video}" == "_yes"], [AC_DEFINE(CS_SCCP_VIDEO, 1, [Using streaming video])])
	AC_MSG_NOTICE([--enable-video: ${ac_cv_streaming_video}])
])

AC_DEFUN([CS_ENABLE_VIDEOLAYER], [
	AC_ARG_ENABLE(videolayer, 
	  AC_HELP_STRING([--enable-videolayer], [enable video layer (experimental)]), 
	  ac_cv_streaming_videolayer=$enableval, ac_cv_streaming_videolayer=no)
	AS_IF([test "${ac_cv_streaming_videolayer}" == "yes"], [AC_DEFINE(CS_SCCP_VIDEOLAYER, 1, [Using video layer])])
	AC_MSG_NOTICE([--enable-videolayer: ${ac_cv_streaming_videolayer}])
])

AC_DEFUN([CS_IPv6], [
	AC_ARG_ENABLE(IPv6, 
	  AC_HELP_STRING([--enable-IPv6], [enable IPv6 support (experimental, developers only)]), 
	  ac_cv_ipv6=$enableval, ac_cv_ipv6=no)
	AS_IF([test "${ac_cv_ipv6}" == "yes"], [AC_DEFINE([CS_IPV6], 1, [Enable IPv6 support])])
	AC_MSG_NOTICE([--enable-IPv6: ${ac_cv_ipv6}])
])

AC_DEFUN([CS_ENABLE_DISTRIBUTED_DEVSTATE], [
	AC_ARG_ENABLE(distributed_devicestate, 
	  AC_HELP_STRING([--enable-distributed-devicestate], [enable distributed devicestate (>ast 1.6.2)(experimental)]), 
	  ac_cv_use_distributed_devicestate=$enableval, ac_cv_use_distributed_devicestate=no)
	AS_IF([test "_${ac_cv_use_distributed_devicestate}" == "_yes"], [AC_DEFINE(distributed_devicestate, 1, [conference distributed devicestate])])
	AC_MSG_NOTICE([--enable-distributed-devicestate: ${ac_cv_use_distributed_devicestate}])
])

AC_DEFUN([CS_PARSE_WITH_AND_ENABLE], [
	CS_ENABLE_OPTIMIZATION
	CS_ENABLE_DEBUG  
	CS_ENABLE_REFCOUNT_DEBUG
	CS_DISABLE_PICKUP
	CS_DISABLE_PARK
	CS_DISABLE_DIRTRFR
	CS_DISABLE_MONITOR
	CS_ENABLE_CONFERENCE
	CS_DISABLE_MANAGER
	CS_DISABLE_FUNCTIONS
	CS_ENABLE_INDICATIONS
	CS_DISABLE_REALTIME
	CS_DISABLE_FEATURE_MONITOR
	CS_ENABLE_ADVANCED_FUNCTIONS
	CS_ENABLE_EXPERIMENTAL_MODE
	CS_ENABLE_EXPERIMENTAL_CODEC
	CS_ENABLE_EXPERIMENTAL_RTP
	CS_ENABLE_EXPERIMENTAL_NEWIP
	CS_DISABLE_DEVSTATE_FEATURE
	CS_DISABLE_DYNAMIC_SPEEDDIAL
	CS_DISABLE_DYNAMIC_SPEEDDIAL_CID
	CS_ENABLE_VIDEO
	CS_ENABLE_VIDEOLAYER
	CS_ENABLE_DISTRIBUTED_DEVSTATE
	CS_IPv6
])

AC_DEFUN([CS_PARSE_WITH_LIBGC], [
	AC_ARG_WITH(libgc,
	  [AC_HELP_STRING([--with-libgc=yes|no],[use garbage collector (libgc) as allocator (experimental)])],
	    uselibgc="$withval")
	if test "x$uselibgc" = "xyes"; then
	  AC_CHECK_LIB(gc,GC_malloc,[
	    AC_CHECK_LIB(gc,GC_pthread_create,[
		AC_DEFINE(HAVE_LIBGC, 1, [Define to 1 if libgc is available])
		GCLIBS="-lgc"
		GCFLAGS="-DGC_LINUX_THREADS -D_REENTRANT"
		msg_gc=yes
	    ])
	  ], [AC_MSG_ERROR(*** LIBGC support will not be built (LIBGC library not found) ***)
	  ])
	fi
	if test "${enable_debug}" = "yes"; then
		GCFLAGS="$GCFLAGS -DGC_FIND_LEAK -DGC_PRINT_STATS"
	fi
	AC_SUBST(GCLIBS)
	AC_SUBST(GCFLAGS)
])

AC_DEFUN([CS_SETUP_MODULE_DIR], [
	AC_ARG_WITH([astmoddir],
	    [AC_HELP_STRING([--with-astmoddir=PATH],[Location of the Asterisk Module Directory])],
	    [PBX_MODDIR="${withval}"],
	    [case "${host}" in
                        *-*-darwin*)
                                PBX_MODDIR='/Library/Application Support/Asterisk/Modules/modules'
                                ;;
                        *)
                                if test -d "${PBX_TEMPMODDIR}"; then
                                    PBX_MODDIR="${PBX_TEMPMODDIR}"
                                elif test "x${prefix}" != "xNONE"; then
                                    case "$build_cpu" in
                                        x86_64|amd64|ppc64)
                                            if test -d ${prefix}/lib64/asterisk/modules; then
                                                    PBX_MODDIR=${prefix}/lib64/asterisk/modules
                                            else
                                                    PBX_MODDIR=${prefix}/lib/asterisk/modules
                                            fi
                                            ;;
                                        *)
                                            PBX_MODDIR=${prefix}/lib/asterisk/modules;
                                            ;;
                                    esac
                                fi
                                ;;
             esac])
        AC_SUBST([PBX_MODDIR]) 
        csmoddir=${PBX_MODDIR}
        AC_SUBST([csmoddir])
])

AC_DEFUN([CS_PARSE_WITH_LIBEV], [
	EVENT_LIBS=""
	EVENT_CFLAGS=""
	EVENT_TYPE=""
	AC_ARG_WITH(libevent,
	  [AC_HELP_STRING([--with-libevent=yes|no],[use garbage collector (libgc) as allocator (experimental)])],
	    uselibevent="$withval")
	if test "x$uselibevent" = "xyes"; then
		if test -z "$EVENT_HOME" ; then
			AC_CHECK_LIB([ev], [event_init], [HAVE_EVENT="yes"], [])
			if test "$HAVE_EVENT" = "yes" ; then
				EVENT_LIBS="-lev"
				EVENT_TYPE="ev"
			else 
				AC_CHECK_LIB([event], [event_init], [HAVE_EVENT="yes"], [])
				if test "$HAVE_EVENT" = "yes" ; then
					EVENT_LIBS="-levent"
					EVENT_TYPE="event"
				fi
			fi	
		else
			EVENT_OLD_LDFLAGS="$LDFLAGS" ; LDFLAGS="$LDFLAGS -L$EVENT_HOME/lib"
			EVENT_OLD_CFLAGS="$CFLAGS" ; CFLAGS="$CFLAGS -I$EVENT_HOME/include"
			AC_CHECK_LIB([ev], [event_init], [HAVE_EVENT="yes"], [])
			if test "$HAVE_EVENT" = "yes"; then
				CFLAGS="$EVENT_OLD_CFLAGS"
				LDFLAGS="$EVENT_OLD_LDFLAGS"
				if test "$HAVE_EVENT" = "yes" ; then
					EVENT_LIBS="-L$EVENT_HOME/lib -lev"
					test -d "$EVENT_HOME/include" && EVENT_CFLAGS="-I$EVENT_HOME/include"
					EVENT_TYPE="ev"
				fi
			else
				AC_CHECK_LIB([event], [event_init], [HAVE_EVENT="yes"], [])
				CFLAGS="$EVENT_OLD_CFLAGS"
				LDFLAGS="$EVENT_OLD_LDFLAGS"
				if test "$HAVE_EVENT" = "yes" ; then
					EVENT_LIBS="-L$EVENT_HOME/lib -levent"
					test -d "$EVENT_HOME/include" && EVENT_CFLAGS="-I$EVENT_HOME/include"
					EVENT_TYPE="event"
				fi
			fi
		fi
		AC_MSG_CHECKING([for libev/libevent...])
		if test "$HAVE_EVENT" = "yes" ; then
			if test "$EVENT_TYPE" = "ev"; then
				AC_MSG_RESULT([libev])
				AC_DEFINE(HAVE_LIBEV, 1, [Define to 1 if libev is available])
				AC_DEFINE(HAVE_LIBEVENT_COMPAT, 1, [Define to 1 if libev-libevent is available])
			else
				AC_MSG_RESULT([libevent])
				AC_DEFINE(HAVE_LIBEVENT, 1, [Define to 1 if libevent is available])
			fi
		else
			AC_MSG_RESULT([no])
	dnl		AC_MSG_ERROR([
	dnl			*** ERROR: cannot find libev or libevent!
	dnl			***
	dnl			*** Either install libev + libev-libevent-dev (preferred) or the older libevent
	dnl			*** Sources can be found here: http://software.schmorp.de/pkg/libev.html or http://www.monkey.org/~provos/libevent/
	dnl			*** If it's already installed, specify its path using --with-libevent=PATH
	dnl		])
		fi
	fi
	AM_CONDITIONAL([BUILD_WITH_LIBEVENT], test "$EVENT_TYPE" != "")
	AC_SUBST([EVENT_LIBS])
	AC_SUBST([EVENT_CFLAGS])
	AC_SUBST([EVENT_TYPE])
])

AC_DEFUN([AX_COUNT_CPUS], [
    AC_REQUIRE([AC_PROG_EGREP])
    AC_MSG_CHECKING(the number of available CPUs)
    CPU_COUNT="0"

    if test -f /proc/cpuinfo; then
        #On Linux
        if test "x$CPU_COUNT" = "x0" -a -e /proc/cpuinfo; then
            CPU_COUNT=`$EGREP -c '^processor' /proc/cpuinfo`
        fi
    else 
        #On BSD/MacOS
        if test -x /usr/sbin/sysctl -a `/sbin/sysctl -a 2>/dev/null| grep -c hw.cpu`; then
            CPU_COUNT=`/usr/sbin/sysctl -n hw.ncpu`
        fi
        #On Sparc
        if test "x${CPU_COUNT}" = "x0" -a -e /proc/cpuinfo; then
            CPU_COUNT=`${EGREP} -c 'ClkTck' /proc/cpuinfo`
        fi
    fi
    if test "x$CPU_COUNT" = "x0"; then
        CPU_COUNT="1"
        AC_MSG_RESULT( [unable to detect (assuming 1)] )
    else
        AC_MSG_RESULT( $CPU_COUNT )
    fi
    AC_DEFINE_UNQUOTED([CS_CPU_COUNT],`echo ${CPU_COUNT}`,[CS_CPU Count])
])
