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
	AC_PATH_PROGS(DATE,date,[echo date not found so no daten info will be available])
	AC_PATH_PROGS(UNAME,uname,[echo uname not found so no version info will be available])
	AC_PATH_PROGS(WHOAMI,whoami,[echo whoami not found so no builduser info will be available])
	AC_PATH_PROGS(FINGER,finger,[echo finger not found so no builduser info will be available])
	AC_PATH_PROGS(HEAD,head,[echo finger not found so no builduser info will be available])
	AC_PATH_PROGS(CUT,cut,[echo finger not found so no builduser info will be available])
	AC_PATH_PROGS(AWK,awk,[echo awk not found so no builduser info will be available])

	if test ! x"${UNAME}" = xNo; then
	    if test -n $BUILD_OS ; then
		BUILD_DATE="`date -u "+%Y-%m-%d %H:%M:%S"` UTC"
		BUILD_OS="`${UNAME} -s`"
		BUILD_MACHINE="`${UNAME} -m`"
		BUILD_HOSTNAME="`${UNAME} -n`"
		BUILD_KERNEL="`${UNAME} -r`"
		if test ! x"${AWK}" = xNo; then
			BUILD_USER="`${FINGER} -lp $(echo "$USER") | ${HEAD} -n 1 | ${AWK} -F:\  '{print $3}'`"
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
		AC_DEFINE([__Darwin__],,[Define if Darwin])
		AC_SUBST(__Darwin__)
		no_libcap=yes
		ostype=bsd
		;;
	  *-*-freebsd*)
		no_libcap=yes
		ostype=bsd
		;;
	  *-*-netbsd*)
		no_libcap=yes
		ostype=bsd
		;;
	  *-*-openbsd*)
		no_libcap=yes
		ostype=bsd
		;;
	  *-*-dragonfly*)
		no_libcap=yes
		ostype=bsd
		;;
	  *-aix*)
	    AC_DEFINE(AIX,,[Define if AIX])
	     broken_types=yes
		no_libcap=yes
		ostype=aix
	    ;;
	  *-osf4*)
	    AC_DEFINE(OSF1,,[Define if OSF1])
	    tru64_types=yes
		no_libcap=yes
		ostype=osf
	    ;;
	  *-osf5.1*)
	    AC_DEFINE(OSF1)
		no_libcap=yes
		ostype=osf
	    ;;
	  *-tru64*)
	    AC_DEFINE(OSF1)
		tru64_types=yes
		no_libcap=yes
		ostype=osf
	    ;;
	  *-*-linux*)
		ostype=linux
		LARGEFILE_FLAGS="-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
		CFLAGS="$CFLAGS $LARGEFILE_FLAGS"
		;;
	  *cygwin*)
		AC_DEFINE([_WIN32_WINNT],[0x0500],[Heya, it's windows])
		AC_DEFINE([INET_ADDRSTRLEN],[16],[cygwin detection does not work for that, anybody has an idea ?])
		AC_DEFINE([INET6_ADDRSTRLEN],[46],[cygwin detection does not work for that, anybody has an idea ?])
		AC_DEFINE([__CYGWIN__],[1],[make sure __CYGWIN__ is defined ...])
		ostype=cygwin
		;;
	  *-*-solaris2*)
		AC_DEFINE([SOLARIS],[1],[needed for optional declarations to be visible])
		no_libcap=yes
	ostype=solaris
		force_generic_timers=yes
		;;
	  *)
		AC_MSG_RESULT(Unsupported operating system: ${host})
		no_libcap=yes
		ostype=unknown
		;;
	esac  
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

	CFLAGS="$CFLAGS -std=gnu89"

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
	AC_PATH_PROGS(SVN, svn, [echo Missing subversion so some stuff will be borked],${PATH}:/opt/csw/bin)
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
	AC_SUBST(GREP)
	AC_SUBST(RPMBUILD)
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
	AC_CHECK_HEADERS([netinet/in.h fcntl.h])
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
	AS_IF([test "${ac_cv_use_ccache}" != "no"], [
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
	  save_CFLAGS="$CFLAGS"
	  save_LDFLAGS="$LDFLAGS"
	  CFLAGS="$CFLAGS $LTDLINCL"
	  LDFLAGS="$LDFLAGS $LIBLTDL"
	  AC_CHECK_LIB([ltdl], [lt_dladvise_init],[],[AC_MSG_ERROR([installed libltdl is too old])])
	  LDFLAGS="$save_LDFLAGS"
	  CFLAGS="$save_CFLAGS"
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
	AC_FUNC_MALLOC
	AC_FUNC_REALLOC

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
	AC_CHECK_SIZEOF([int])
	AC_CHECK_SIZEOF([long])
	AC_CHECK_SIZEOF([long long])
	AC_CHECK_SIZEOF([char *])
	AC_CHECK_SIZEOF(long)
	AC_CHECK_SIZEOF(long long)
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
	CHANGELOG_OLDEST=1021
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
	PBX_PATH="$prefix /usr /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr/sfw";
	AC_ARG_WITH([asterisk],
	    [AC_HELP_STRING([--with-asterisk=PATH],[Location of the Asterisk installation])],[PBX_PATH="${withval}"],)
	    if test "x${PBX_PATHl}" = "xyes"; then 
		PBX_PATH="$prefix /usr /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr/sfw"; 
	    fi
	AC_ARG_WITH([callweaver],
	    [AC_HELP_STRING([--with-callwaver=PATH],[Location of the Callweaver installation])],[PBX_PATH="${withval}"],)
	    if test "x${PBX_PATH}" = "xyes"; then 
		PBX_PATH="$prefix /usr /usr/local/ssl /usr/lib/ssl /usr/ssl /usr/pkg /usr/local /usr/sfw"; 
	    fi
	   
	CFLAGS_saved="$CFLAGS"
	CPPFLAGS_saved="$CPPFLAGS"
	LDFLAGS_saved="$LDFLAGS"
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
	AS_IF([test "${ac_cv_use_devdoc}" == "yes"], 
	  [DX_ENV_APPEND([INPUT],[. doc src])],
	  [DX_ENV_APPEND([INPUT],[. doc])]
	)
	AC_MSG_NOTICE([--enable-devdoc: ${ac_cv_use_devdoc}])
	DX_HTML_FEATURE(ON)
	DX_CHM_FEATURE(OFF)
	DX_CHI_FEATURE(OFF)
	DX_MAN_FEATURE(ON)
	DX_RTF_FEATURE(OFF)
	DX_XML_FEATURE(OFF)
	DX_PDF_FEATURE(OFF)
	DX_PS_FEATURE(OFF)
	DX_INIT_DOXYGEN($PACKAGE, doc/doxygen.cfg)
])


AC_DEFUN([CS_ENABLE_OPTIMIZATION], [
	AC_ARG_ENABLE(optimization,     
		[AC_HELP_STRING([--disable-optimization],[no detection or tuning flags for cpu version])],
		OPTIMIZECPU=$enableval,OPTIMIZECPU=yes)
	if test "${SCCP_BRANCH}" == "TRUNK"; then 
		strip_binaries="no"
		GDB_FLAGS="-g"
	else 
		strip_binaries="yes"
		GDB_FLAGS=""
	fi

	AC_MSG_NOTICE([--enable-optimization: ${OPTIMIZECPU}])
		AC_ARG_ENABLE(debug,[AC_HELP_STRING([--enable-debug],[enable debug information])],enable_debug=$enableval,enable_debug=no)
	if test "${enable_debug}" = "yes"; then
		AC_DEFINE([GC_DEBUG],[1],[Enable extra garbage collection debugging.])
		AC_DEFINE([DEBUG],[1],[Extra debugging.])
		enable_do_crash="yes"
		enable_debug_mutex="yes"
		strip_binaries="no"
		CFLAGS_saved="$CFLAGS_saved -O0 -Os -Wall -Wextra -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs"
		dnl " -Wlong-long"
		CFLAGS="$CFLAGS_saved"
		GDB_FLAGS="-g"
	else
		AC_DEFINE([DEBUG],[0],[Extra debugging.])
		enable_do_crash="no"
		enable_debug_mutex="no"
		strip_binaries="yes"
		GDB_FLAGS="$GDB_FLAGS"
		CFLAGS_saved="$CFLAGS_saved -Wno-unused-function -Wno-long-long"
		CFLAGS="$CFLAGS_saved"
	fi
])

AC_DEFUN([CS_ENABLE_DEBUG], [
	AC_MSG_NOTICE([--enable-debug: ${enable_debug}])
	AM_CONDITIONAL([WANT_DEBUG],[test "${enable_debug}" = "yes"])
	AC_SUBST([strip_binaries])
	AC_SUBST([GDB_FLAGS])
])

AC_DEFUN([CS_DISABLE_PICKUP], [
	AC_ARG_ENABLE(pickup, 
	  AC_HELP_STRING([--disable-pickup], [disable pickup function]), 
	  ac_cv_use_pickup=$enableval, ac_cv_use_pickup=yes)
	AS_IF([test "${ac_cv_use_pickup}" == "yes"], [AC_DEFINE(CS_SCCP_PICKUP, 1, [pickup function enabled])])
	AC_MSG_NOTICE([--enable-pickup: ${ac_cv_use_pickup}])
])

AC_DEFUN([CS_DISABLE_PARK], [
	AC_ARG_ENABLE(park, 
	  AC_HELP_STRING([--disable-park], [disable park functionality]), 
	  ac_cv_use_park=$enableval, ac_cv_use_park=yes)
	AS_IF([test "${ac_cv_use_park}" == "yes"], [AC_DEFINE(CS_SCCP_PARK, 1, [park functionality enabled])])
	AC_MSG_NOTICE([--enable-park: ${ac_cv_use_park}])
])

AC_DEFUN([CS_DISABLE_DIRTRFR], [
	AC_ARG_ENABLE(dirtrfr, 
	  AC_HELP_STRING([--disable-dirtrfr], [disable direct transfer]), 
	  ac_cv_use_dirtrfr=$enableval, ac_cv_use_dirtrfr=yes)
	AS_IF([test "${ac_cv_use_dirtrfr}" == "yes"], [AC_DEFINE(CS_SCCP_DIRTRFR, 1, [direct transfer enabled])])
	AC_MSG_NOTICE([--enable-dirtrfr: ${ac_cv_use_dirtrfr}])
])

AC_DEFUN([CS_DISABLE_MONITOR], [
	AC_ARG_ENABLE(monitor, 
	  AC_HELP_STRING([--disable-monitor], [disable feature monitor)]), 
	  ac_cv_use_monitor=$enableval, ac_cv_use_monitor=yes)
	AS_IF([test "${ac_cv_use_monitor}" == "yes"], [AC_DEFINE(CS_SCCP_FEATURE_MONITOR, 1, [feature monitor enabled])])
	AC_MSG_NOTICE([--enable-monitor: ${ac_cv_use_monitor}])
])

AC_DEFUN([CS_ENABLE_CONFERENCE], [
	AC_ARG_ENABLE(conference, 
	  AC_HELP_STRING([--enable-conference], [enable conference (>ast 1.6.2)(experimental)]), 
	  ac_cv_use_conference=$enableval, ac_cv_use_conference=no)
	AS_IF([test "${ac_cv_use_conference}" == "yes"], [AC_DEFINE(CS_SCCP_CONFERENCE, 1, [conference enabled])])
	AC_MSG_NOTICE([--enable-conference: ${ac_cv_use_conference}])
])

AC_DEFUN([CS_DISABLE_MANAGER], [
	AC_ARG_ENABLE(manager, 
	  AC_HELP_STRING([--disable-manager], [disabled ast manager events]), 
	  ac_cv_use_manager=$enableval, ac_cv_use_manager=yes)
	AS_IF([test "${ac_cv_use_manager}" == "yes"], [
		AC_DEFINE(CS_MANAGER_EVENTS, 1, [manager events enabled])
		AC_DEFINE(CS_SCCP_MANAGER, 1, [manager console control enabled])
	])
	AC_MSG_NOTICE([--enable-manager: ${ac_cv_use_manager}])
])

AC_DEFUN([CS_DISABLE_FUNCTIONS], [
	AC_ARG_ENABLE(functions, 
	  AC_HELP_STRING([--disable-functions], [disabled Dialplan functions]), 
	  ac_cv_use_functions=$enableval, ac_cv_use_functions=yes)
	AS_IF([test "${ac_cv_use_functions}" == "yes"], [AC_DEFINE(CS_SCCP_FUNCTIONS, 1, [dialplan function enabled])])
	AC_MSG_NOTICE([--enable-functions: ${ac_cv_use_functions}])
])

AC_DEFUN([CS_ENABLE_INDICATIONS], [
	AC_ARG_ENABLE(indications, 
	  AC_HELP_STRING([--enable-indications], [enable debug indications]), 
	  ac_cv_debug_indications=$enableval, ac_cv_debug_indications=no)
	AS_IF([test "${ac_cv_debug_indications}" == "yes"], [AC_DEFINE(CS_DEBUG_INDICATIONS, 1, [debug indications enabled])])
	AC_MSG_NOTICE([--enable-indications: ${ac_cv_debug_indications}])
])

AC_DEFUN([CS_DISABLE_REALTIME], [
	AC_ARG_ENABLE(realtime, 
	  AC_HELP_STRING([--disable-realtime], [disable realtime support]), 
	  ac_cv_realtime=$enableval, ac_cv_realtime=yes)
	AS_IF([test "${ac_cv_realtime}" == "yes"], [AC_DEFINE(CS_SCCP_REALTIME, 1, [realtime enabled])])
	AC_MSG_NOTICE([--enable-realtime: ${ac_cv_realtime}])
])

AC_DEFUN([CS_DISABLE_FEATURE_MONITOR], [
	AC_ARG_ENABLE(feature_monitor, 
	  AC_HELP_STRING([--disable-feature-monitor], [disable feature monitor]), 
	  ac_cv_feature_monitor=$enableval, ac_cv_feature_monitor=yes)
	AS_IF([test "${ac_cv_feature_monitor}" == "yes"], [AC_DEFINE(CS_SCCP_FEATURE_MONITOR, 1, [feature monitor enabled])])
	AC_MSG_NOTICE([--enable-feature-monitor: ${ac_cv_feature_monitor}])
])

AC_DEFUN([CS_ENABLE_ADVANCED_FUNCTIONS], [
	AC_ARG_ENABLE(advanced_functions, 
	  AC_HELP_STRING([--enable-advanced-functions], [enable advanced functions (experimental)]), 
	    ac_cv_advanced_functions=$enableval, ac_cv_advanced_functions=no)
	AS_IF([test "${ac_cv_advanced_functions}" == "yes"], [AC_DEFINE(CS_ADV_FEATURES, 1, [advanced functions enabled])])
	AC_MSG_NOTICE([--enable-advanced-functions: ${ac_cv_advanced_functions}])
])

AC_DEFUN([CS_ENABLE_EXPERIMENTAL_MODE], [
	AC_ARG_ENABLE(experimental_mode, 
	  AC_HELP_STRING([--enable-experimental-mode], [enable experimental mode (only for developers)]), 
	    ac_cv_experimental_mode=$enableval, ac_cv_experimental_mode=no)
	AS_IF([test "${ac_cv_experimental_mode}" == "yes"], [AC_DEFINE(CS_EXPERIMENTAL, 1, [experimental mode enabled])])
	AC_MSG_NOTICE([--enable-experimental-mode: ${ac_cv_experimental_mode} (only for developers)])
])

AC_DEFUN([CS_DISABLE_DEVSTATE_FEATURE], [
	AC_ARG_ENABLE(devstate_feature, 
	  AC_HELP_STRING([--disable-devstate-feature], [disable device state feature button]), 
	    ac_cv_devstate_feature=$enableval, ac_cv_devstate_feature=yes)
	AS_IF([test ${ASTERISK_VERSION_NUMBER} -lt 10601], [ac_cv_devstate_feature=no])
	AS_IF([test "${DEVICESTATE_H}" != "yes"], [ac_cv_devstate_feature=no])
	AS_IF([test "${ac_cv_devstate_feature}" == "yes"], [AC_DEFINE(CS_DEVSTATE_FEATURE, 1, [devstate feature enabled])])
	AC_MSG_NOTICE([--enable-devstate-feature: ${ac_cv_devstate_feature}])
])

AC_DEFUN([CS_DISABLE_DYNAMIC_SPEEDDIAL], [
	AC_ARG_ENABLE(dynamic_speeddial, 
	  AC_HELP_STRING([--disable-dynamic-speeddial], [disable dynamic speeddials]), 
	    ac_cv_dynamic_speeddial=$enableval, ac_cv_dynamic_speeddial=yes)
	AS_IF([test "${ac_cv_dynamic_speeddial}" == "yes"], [AC_DEFINE(CS_DYNAMIC_SPEEDDIAL, 1, [dynamic speeddials enabled])])
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
	AS_IF([test "${ac_cv_streaming_video}" == "yes"], [AC_DEFINE(CS_SCCP_VIDEO, 1, [Using streaming video])])
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

AC_DEFUN([CS_DISABLE_DYNAMIC_CONFIG], [
	AC_ARG_ENABLE(dynamic_config, 
	  AC_HELP_STRING([--disable-dynamic-config], [disable sccp reload]), 
	  ac_cv_dynamic_config=$enableval, ac_cv_dynamic_config=yes)
	AS_IF([test "${ac_cv_dynamic_config}" == "yes"], [AC_DEFINE(CS_DYNAMIC_CONFIG, 1, [sccp reload enabled])])
	AC_MSG_NOTICE([--enable-dynamic-config: ${ac_cv_dynamic_config}])
])

AC_DEFUN([CS_PARSE_WITH_AND_ENABLE], [
	CS_ENABLE_OPTIMIZATION
	CS_ENABLE_DEBUG  
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
	CS_DISABLE_DEVSTATE_FEATURE
	CS_DISABLE_DYNAMIC_SPEEDDIAL
	CS_DISABLE_DYNAMIC_SPEEDDIAL_CID
	CS_ENABLE_VIDEO
	CS_ENABLE_VIDEOLAYER
	CS_IPv6
	CS_DISABLE_DYNAMIC_CONFIG
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
	case "${host}" in
        	*-*-darwin*)
        		csmoddir='/Library/Application Support/Asterisk/Modules/modules'
	             	;;
           	*)
                        if test "x${prefix}" != "xNONE"; then
                          if test "${build_cpu}" = "x86_64"; then
                                if test -d ${prefix}/lib64/asterisk/modules; then
                                        csmoddir=${prefix}/lib64/asterisk/modules
                                else
                                        csmoddir=${prefix}/lib/asterisk/modules
                                fi
                          else
                                csmoddir=${prefix}/lib/asterisk/modules;
                          fi
                        elif test -z "${csmoddir}"; then
                           csmoddir="${DESTDIR}${PBX_MODDIR}"
                           # directory for modules
                           if test -z "${csmoddir}"; then
                             # fallback to asterisk modules directory
                             csmoddir="${PBX_LIB}/asterisk/modules"
                           fi
                        fi
          		;;
	esac
	AC_SUBST([csmoddir])
])
