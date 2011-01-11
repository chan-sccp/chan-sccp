dnl FILE: asterisk.m4
dnl COPYRIGHT: chan-sccp-b.sourceforge.net group 2009
dnl CREATED BY: Created by Diederik de Groot
dnl LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
dnl          See the LICENSE file at the top of the source tree.
dnl DATE: $Date: $
dnl REVISION: $Revision: $

AC_DEFUN([GET_ASTERISK_VERSION], [
  CONFIGURE_PART([Checking Asterisk Version:])
  AC_CHECK_HEADER([asterisk/version.h],[
    AC_MSG_CHECKING([for version in asterisk/version.h])
    if grep -q "\"1\.2" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_2, 1, [Define ASTERISK_CONF_1_2])
      AC_MSG_RESULT([Found 'Asterisk Version 1.2.x'])
      REALTIME_USEABLE=0
      ASTERISK_VER_GROUP=1.2
      ASTERISK_VER=1.2
      AC_SUBST([ASTERISK_VER])
    elif grep -q "\"1\.4" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_4, 1, [Define ASTERISK_CONF_1_4])
      AC_MSG_RESULT([Found 'Asterisk Version 1.4.x'])
      REALTIME_USEABLE=1
      ASTERISK_VER_GROUP=1.4
      ASTERISK_VER=1.4
      AC_SUBST([ASTERISK_VER])
    elif grep -q "\"1\.6" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_6, 1, [Define ASTERISK_CONF_1_6])
      AC_MSG_RESULT([Found 'Asterisk Version 1.6.x'])
      ASTERISK_VER_GROUP=1.6
      if grep -q "\"1\.6\.0" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_0, 1, [Define ASTERISK_CONF_1_6_0])
        AC_MSG_RESULT([Specifically 1.6.0])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.0
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"1\.6\.1" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_1, 1, [Define ASTERISK_CONF_1_6_1])
        AC_MSG_RESULT([Specifically 1.6.1])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.1
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"1\.6\.2" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_2, 1, [Define ASTERISK_CONF_1_6_2])
        AC_MSG_RESULT([Specifically 1.6.2])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.2
        AC_SUBST([ASTERISK_VER])
      fi
    elif grep -q "\"1\.8" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_8, 1, [Define ASTERISK_CONF_1_8])
      AC_MSG_RESULT([Found 'Asterisk Version 1.8.x'])
      ASTERISK_VER_GROUP=1.8
      if grep -q "\"1\.8\.0" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_8_0, 1, [Define ASTERISK_CONF_1_8_0])
        AC_MSG_RESULT([Specifically 1.8.0])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.8.0
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"1\.8\.1" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_8_1, 1, [Define ASTERISK_CONF_1_8_1])
        AC_MSG_RESULT([Specifically 1.8.1])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.8.1
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"1\.8\.2" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_8_2, 1, [Define ASTERISK_CONF_1_8_2])
        AC_MSG_RESULT([Specifically 1.8.2])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.8.2
        AC_SUBST([ASTERISK_VER])
      fi
    elif grep -q "\"SVN-trunk" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_8, 1, [Define ASTERISK_CONF_1_8])
      AC_MSG_RESULT([Found 'Asterisk Version SVN Trunk --> Using 1.8.x as version number'])
      REALTIME_USEABLE=1
      ASTERISK_VER_GROUP=1.8
      ASTERISK_VER=1.8
      AC_SUBST([ASTERISK_VER])
    elif grep -q "\"SVN-branch" $PBX_INCLUDE/version.h; then
      if grep -q "\"SVN-branch-1\.2" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_2, 1, [Define ASTERISK_CONF_1_2])
        AC_MSG_RESULT([Found 'Asterisk Version 1.2.x'])
        REALTIME_USEABLE=0
	ASTERISK_VER_GROUP=1.2
        ASTERISK_VER=1.2
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"SVN-branch-1\.4" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_4, 1, [Define ASTERISK_CONF_1_4])
        AC_MSG_RESULT([Found 'Asterisk Version 1.4.x'])
        REALTIME_USEABLE=1
	ASTERISK_VER_GROUP=1.4
        ASTERISK_VER=1.4
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"SVN-branch-1\.6" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6, 1, [Define ASTERISK_CONF_1_6])
        AC_MSG_RESULT([Found 'Asterisk Version 1.6.x'])
	ASTERISK_VER_GROUP=1.8
        if grep -q "\"SVN-branch-1\.6\.0" $PBX_INCLUDE/version.h; then
          AC_DEFINE(ASTERISK_CONF_1_6_0, 1, [Define ASTERISK_CONF_1_6_0])
          AC_MSG_RESULT([Specifically 1.6.0])
          REALTIME_USEABLE=1
          ASTERISK_VER=1.6.0
          AC_SUBST([ASTERISK_VER])
        elif grep -q "\"SVN-branch-1\.6\.1" $PBX_INCLUDE/version.h; then
          AC_DEFINE(ASTERISK_CONF_1_6_1, 1, [Define ASTERISK_CONF_1_6_1])
          AC_MSG_RESULT([Specifically 1.6.1])
          REALTIME_USEABLE=1
          ASTERISK_VER=1.6.1
          AC_SUBST([ASTERISK_VER])
        elif grep -q "\"SVN-branch-1\.6\.2" $PBX_INCLUDE/version.h; then
          AC_DEFINE(ASTERISK_CONF_1_6_2, 1, [Define ASTERISK_CONF_1_6_2])
          AC_MSG_RESULT([Specifically 1.6.2])
          REALTIME_USEABLE=1
          ASTERISK_VER=1.6.2
          AC_SUBST([ASTERISK_VER])
        fi
      elif grep -q "\"SVN-branch-1\.8" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_8, 1, [Define ASTERISK_CONF_1_8])
        AC_MSG_RESULT([Found 'Asterisk Version 1.8.x'])
	ASTERISK_VER_GROUP=1.8
        if grep -q "\"SVN-branch-1\.8\.0" $PBX_INCLUDE/version.h; then
          AC_DEFINE(ASTERISK_CONF_1_8_0, 1, [Define ASTERISK_CONF_1_8_0])
          AC_MSG_RESULT([Specifically 1.8.0])
          REALTIME_USEABLE=1
          ASTERISK_VER=1.8.0
          AC_SUBST([ASTERISK_VER])
        elif grep -q "\"SVN-branch-1\.8\.1" $PBX_INCLUDE/version.h; then
          AC_DEFINE(ASTERISK_CONF_1_8_1, 1, [Define ASTERISK_CONF_1_8_1])
          AC_MSG_RESULT([Specifically 1.8.1])
          REALTIME_USEABLE=1
          ASTERISK_VER=1.8.1
          AC_SUBST([ASTERISK_VER])
        elif grep -q "\"SVN-branch-1\.8\.2" $PBX_INCLUDE/version.h; then
          AC_DEFINE(ASTERISK_CONF_1_8_2, 1, [Define ASTERISK_CONF_1_8_2])
          AC_MSG_RESULT([Specifically 1.8.2])
          REALTIME_USEABLE=1
          ASTERISK_VER=1.8.2
          AC_SUBST([ASTERISK_VER])
        fi
      else
        echo ""
        echo ""
        echo "PBX branch version could not be determined"
        echo "==================================="
        echo "Either install asterisk and asterisk-devel packages"
        echo "Or specify the location where asterisk can be found, using ./configure --with-asterisk=[path]"
        exit
      fi
    else 
      echo ""
      echo ""
      echo "PBX version could not be determined"
      echo "==================================="
      echo "Either install asterisk and asterisk-devel packages"
      echo "Or specify the location where asterisk can be found, using ./configure --with-asterisk=[path]"
      exit
    fi

    PBX_VER_NUM=0
    if grep -q "SVN-branch" $PBX_INCLUDE/version.h; then
        PBX_VERSION_NUM="`grep 'ASTERISK_VERSION ' $PBX_INCLUDE/version.h|sed 's/#define ASTERISK_VERSION "SVN-branch-\(.*\)-r\(.*\)M"/\1/g' |sed 's/\./0/g'`"
        PBX_BRANCH="BRANCH"
        PBX_REVISION="`grep 'ASTERISK_VERSION ' $PBX_INCLUDE/version.h|sed 's/#define ASTERISK_VERSION "SVN-branch-\(.*\)-r\(.*\)M"/\2/g'`"
    elif grep -q "trunk" $PBX_INCLUDE/version.h; then
        PBX_VERSION_NUM=10800
        PBX_BRANCH="TRUNK"
        PBX_REVISION="`grep 'ASTERISK_VERSION ' $PBX_INCLUDE/version.h|sed 's/#define ASTERISK_VERSION "SVN-trunk-r\(.*\)"/\1/g'`"
    else
        PBX_VERSION_NUM="`grep 'ASTERISK_VERSION_NUM ' $PBX_INCLUDE/version.h|sed 's/#define ASTERISK_VERSION_NUM \(.*\)/\1/g'`"
        PBX_BRANCH="TGZ"
dnl        PBX_REVISION="`grep 'ASTERISK_VERSION ' $PBX_INCLUDE/version.h|sed 's/#define ASTERISK_VERSION "\(.*\)"/\1/g' | sed 's/\./0/g'`"
dnl        PBX_REVISION=${PBX_REVISION:${#PBX_REVISION}-2:2}
	PBX_REVISION="`grep 'ASTERISK_VERSION ' $PBX_INCLUDE/version.h|sed 's/#define ASTERISK_VERSION "\(.*\)"/\1/g' | awk '{print substr($0,length -1,2);}'`"
    fi
    AC_DEFINE_UNQUOTED([PBX_VERSION_NUM],`echo ${PBX_VERSION_NUM}`,[PBX Version Number])
    AC_SUBST([PBX_VERSION_NUM])
    AC_DEFINE_UNQUOTED([PBX_BRANCH],"`echo ${PBX_BRANCH}`",[PBX Branch Type])
    AC_SUBST([PBX_BRANCH])
    AC_DEFINE_UNQUOTED([PBX_REVISION],`echo ${PBX_REVISION}`,[PBX Revision Number])
    AC_SUBST([PBX_REVISION])
    AC_DEFINE_UNQUOTED([ASTERISK_VERSION_NUM],`echo ${PBX_VERSION_NUM}`,[ASTERISK Version Number])
    AC_DEFINE_UNQUOTED([ASTERISK_BRANCH],"`echo ${PBX_BRANCH}`",[ASTERISK Branch Type])
    AC_DEFINE_UNQUOTED([ASTERISK_REVISION],`echo ${PBX_REVISION}`,[ASTERISK Revision Number])
    ASTERISK_VERSION_NUM=${PBX_VERSION_NUM}
    AC_SUBST([ASTERISK_VERSION_NUM])
    ],
    [AC_MSG_RESULT(Not Found 'asterisk/version.h')]
  )
])

dnl Conditional Makefile.am Macros
AC_DEFUN([GET_PBX_AMCONDITIONALS],[
	AM_CONDITIONAL([BUILD_AST], test "$PBX_TYPE" == "ASTERISK")
	AM_CONDITIONAL([BUILD_AST10200], test "$ASTERISK_VERSION_NUM" == "10200")
	AM_CONDITIONAL([BUILD_AST10400], test "$ASTERISK_VERSION_NUM" == "10400")
	AM_CONDITIONAL([BUILD_AST10600], test "$ASTERISK_VERSION_NUM" == "10600")
	AM_CONDITIONAL([BUILD_AST10601], test "$ASTERISK_VERSION_NUM" == "10601")
	AM_CONDITIONAL([BUILD_AST10602], test "$ASTERISK_VERSION_NUM" == "10602")
	AM_CONDITIONAL([BUILD_AST10800], test "$ASTERISK_VERSION_NUM" == "10800")
	AM_CONDITIONAL([BUILD_AST10801], test "$ASTERISK_VERSION_NUM" == "10801")
	AM_CONDITIONAL([BUILD_AST10802], test "$ASTERISK_VERSION_NUM" == "10802")
	
])

AC_DEFUN([FIND_ASTERISK_HEADERS],[
  CONFIGURE_PART([Checking Asterisk Headers:])
  AC_CHECK_HEADER([asterisk/lock.h],AC_DEFINE(HAVE_PBX_LOCK_H,1,[Found 'asterisk/lock.h']),[lock_compiled=no],[#include <asterisk.h>])
  AS_IF([test "${lock_compiled}" != "no"], [
    AC_CHECK_HEADER([asterisk/acl.h],		AC_DEFINE(HAVE_PBX_ACL_H,1,[Found 'asterisk/acl.h']),,        	 	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/app.h],		AC_DEFINE(HAVE_PBX_APP_H,1,[Found 'asterisk/app.h']),,			[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/astdb.h],		AC_DEFINE(HAVE_PBX_ASTDB_H,1,[Found 'asterisk/astdb.h']),,   		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/callerid.h],	AC_DEFINE(HAVE_PBX_CALLERID_H,1,[Found 'asterisk/callerid.h']),, 	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/causes.h],	AC_DEFINE(HAVE_PBX_CAUSES_H,1,[Found 'asterisk/causes.h']),,      	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/channel.h],	AC_DEFINE(HAVE_PBX_CHANNEL_H,1,[Found 'asterisk/channel.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/channel_pvt.h],	AC_DEFINE(HAVE_PBX_CHANNEL_pvt_H,1,[Found 'asterisk/channel_pvt.h']),, 	[ #include <asterisk.h>]) 
    AC_CHECK_HEADER([asterisk/cli.h],		AC_DEFINE(HAVE_PBX_CLI_H,1,[Found 'asterisk/cli.h']),,       	  	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/config.h],	AC_DEFINE(HAVE_PBX_CONFIG_H,1,[Found 'asterisk/config.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/devicestate.h],	AC_DEFINE(HAVE_PBX_DEVICESTATE_H,1,[Found 'asterisk/devicestate.h']),,	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/endian.h],	AC_DEFINE(HAVE_PBX_ENDIAN_H,1,[Found 'asterisk/endian.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/event.h],		AC_DEFINE(HAVE_PBX_EVENT_H,1,[Found 'asterisk/event.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/features.h],	AC_DEFINE(HAVE_PBX_FEATURES_H,1,[Found 'asterisk/features.h']),,    	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/frame.h],		AC_DEFINE(HAVE_PBX_FRAME_H,1,[Found 'asterisk/frame.h']),,     	  	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/logger.h],	AC_DEFINE(HAVE_PBX_LOGGER_H,1,[Found 'asterisk/logger.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/manager.h],	AC_DEFINE(HAVE_PBX_MANAGER_H,1,[Found 'asterisk/manager.h']),,     	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/module.h],	AC_DEFINE(HAVE_PBX_MODULE_H,1,[Found 'asterisk/module.h']),,      	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/musiconhold.h],	AC_DEFINE(HAVE_PBX_MUSICONHOLD_H,1,[Found 'asterisk/musiconhold.h']),, 	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/options.h],	AC_DEFINE(HAVE_PBX_OPTIONS_H,1,[Found 'asterisk/options.h']),,     	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/pbx.h],		AC_DEFINE(HAVE_PBX_PBX_H,1,[Found 'asterisk/pbx.h']),,			[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/rtp.h],		AC_DEFINE(HAVE_PBX_RTP_H,1,[Found 'asterisk/rtp.h']),,			[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/rtp_engine.h],	AC_DEFINE(HAVE_PBX_RTP_H,2,[Found 'asterisk/rtp_engine.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/sched.h],		AC_DEFINE(HAVE_PBX_SCHED_H,1,[Found 'asterisk/sched.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/strings.h],	AC_DEFINE(HAVE_PBX_STRINGS_H,1,[Found 'asterisk/strings.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/stringfields.h],	AC_DEFINE(HAVE_PBX_STRINGFIELDS_H,1,[Found 'asterisk/stringfields.h']),,[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/translate.h],	AC_DEFINE(HAVE_PBX_TRANSLATE_H,1,[Found 'asterisk/translate.h']),,   	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/utils.h],		AC_DEFINE(HAVE_PBX_UTILS_H,1,[Found 'asterisk/utils.h']),,       	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/astobj.h],	AC_DEFINE(HAVE_PBX_ASTOBJ2_H,1,[Found 'asterisk/astobj.h']),,       	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/astobj2.h],	AC_DEFINE(HAVE_PBX_ASTOBJ2_H,1,[Found 'asterisk/astobj2.h']),,       	[ #include <asterisk.h>])
  ])
])

AC_DEFUN([CHECK_ASTERISK_HEADER_CONTENT],[
	CONFIGURE_PART([Checking Asterisk Header Content:])
	dnl AC_CHECK_HEADER (header-file, action-if-found, [action-if-not-found])
	dnl AC_EGREP_HEADER (pattern, header-file, action-if-found, [action-if-not-found])

	dnl Check Asterisk RealTime Options
	if test $REALTIME_USEABLE = 1 ; then
	AC_CHECK_HEADER(asterisk/buildopts.h,
		AC_MSG_CHECKING([checking DEBUG_CHANNEL_LOCKS in asterisk/buildopts.h])
			if grep -q "define DEBUG_CHANNEL_LOCKS" $PBX_INCLUDE/buildopts.h; then
				AC_DEFINE(CS_AST_DEBUG_CHANNEL_LOCKS,1,[Found 'DEBUG_CHANNEL_LOCKS' in asterisk/buildopts.h])
				AC_MSG_RESULT([Found])
			else
				AC_MSG_RESULT([Not Found])
			fi
		AC_MSG_CHECKING([checking DEBUG_THREADS in asterisk/buildopts.h])
		      if grep -q "define DEBUG_THREADS" $PBX_INCLUDE/buildopts.h; then
				AC_DEFINE(CS_AST_DEBUG_THREADS,1,[Found 'CS_AST_DEBUG_THREADS' in asterisk/buildopts.h])
				AC_MSG_RESULT([Found])
		      else
				AC_MSG_RESULT([Not Found])
		      fi
    	)
    dnl Check Asterisk Jitterbuffer
    AC_MSG_CHECKING([asterisk jitterbuffer for 'target_extra'])
    if grep -rq "target_extra" $PBX_INCLUDE/abstract_jb.h; then
      AC_DEFINE([CS_AST_JB_TARGET_EXTRA],1,[Found 'target_extra' in asterisk/abstract_jb.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Schedule Options
  if test -f $PBX_INCLUDE/sched.h; then
    AC_MSG_CHECKING([asterisk/sched.h for 'define AST_SCHED_DEL'])
    if grep -q "AST_SCHED_DEL(" $PBX_INCLUDE/sched.h; then
      AC_DEFINE(CS_AST_SCHED_DEL,1,[Found 'CS_AST_SCHED_DEL' in asterisk/sched.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk RTP Options
  if test -f $PBX_INCLUDE/rtp.h; then
    AC_MSG_CHECKING([asterisk/rtp.h for 'void ast_rtp_new_source'])
    if grep -q "void ast_rtp_new_source" $PBX_INCLUDE/rtp.h;then 
      AC_DEFINE(CS_AST_RTP_NEW_SOURCE,1,[Found 'void ast_rtp_new_source' in asterisk/rtp.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk RTP 1.8 Options
  if test -f $PBX_INCLUDE/rtp_engine.h; then
    AC_DEFINE(CS_AST_HAS_RTP_ENGINE,1,[Found asterisk/rtp_engine.h])
    AC_MSG_CHECKING([asterisk/rtp_engine.h for 'ast_rtp_instance_new'])
    if grep -q "ast_rtp_instance_new" $PBX_INCLUDE/rtp_engine.h;then 
      AC_DEFINE(CS_AST_RTP_INSTANCE_NEW,1,[Found 'void ast_rtp_instance_new' in asterisk/rtp_engine.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Channel Options
  if test -f $PBX_INCLUDE/channel.h; then
    AC_MSG_CHECKING([asterisk/channel.h for 'struct ast_channel_tech'])
    if grep -q "struct ast_channel_tech" $PBX_INCLUDE/channel.h;then 
      AC_DEFINE(CS_AST_HAS_TECH_PVT,1,[Found 'struct ast_channel_tech' in asterisk/channel.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/channel.h for 'ast_bridged_channel'])
    if grep -q "ast_bridged_channel" $PBX_INCLUDE/channel.h;then 
      AC_DEFINE(CS_AST_HAS_BRIDGED_CHANNEL,1,[Found 'ast_bridged_channel' in asterisk/channel.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/channel.h for 'struct ast_callerid'])
    if grep -q "struct ast_callerid" $PBX_INCLUDE/channel.h;then 
      AC_DEFINE(CS_AST_CHANNEL_HAS_CID,1,[Found 'struct ast_callerid' in asterisk/channel.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/channel.h for 'MAX_MUSICCLASS'])
    if grep -q "MAX_MUSICCLASS" $PBX_INCLUDE/channel.h;then 
      AC_MSG_RESULT([Found])
    else
      AC_DEFINE([MAX_MUSICCLASS],[MAX_LANGUAGE],[Not Found 'MAX_MUSICCLASS' in asterisk/channel.h])
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/channel.h for 'AST_FLAG_MOH'])
    if grep -q "AST_FLAG_MOH" $PBX_INCLUDE/channel.h;then  
      AC_DEFINE(CS_AST_HAS_FLAG_MOH,1,[Found 'AST_FLAG_MOH' in asterisk/channel.h])
      AC_MSG_RESULT([Found])
    else
      AC_DEFINE(AST_FLAG_MOH,1,[Not Found 'AST_FLAG_MOH' in asterisk/channel.h])
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/channel.h for 'AST_MAX_ACCOUNT_CODE'])
    if grep -q "AST_MAX_ACCOUNT_CODE" $PBX_INCLUDE/channel.h;then
      AC_DEFINE(SCCP_MAX_ACCOUNT_CODE,AST_MAX_ACCOUNT_CODE,[Found 'AST_MAX_ACCOUNT_CODE' in asterisk/channel.h]) 
      AC_MSG_RESULT([Found])
    else
      AC_DEFINE(SCCP_MAX_ACCOUNT_CODE,MAX_LANGUAGE,[Not Found 'AST_MAX_ACCOUNT_CODE' in asterisk/channel.h])
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/channel.h for 'typedef unsigned long long ast_group_t'])
    if grep -q "typedef unsigned long long ast_group_t" $PBX_INCLUDE/channel.h;then 
      AC_DEFINE(CS_AST_HAS_AST_GROUP_T,1,[Found 'ast_group_t' in asterisk/channel.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Config Options
  if test -f $PBX_INCLUDE/config.h; then
    AC_MSG_CHECKING([asterisk/config.h for 'ast_config_load'])
    if grep -q "ast_config_load" $PBX_INCLUDE/config.h; then
      AC_MSG_RESULT([Found])
    else
      AC_DEFINE([ast_config_load(x)],[ast_load(x)],[Found 'ast_config_load' in asterisk/config.h])
      AC_DEFINE([ast_config_destroy(x)],[ast_destroy(x)],[Found 'ast_config_load' in asterisk/config.h])
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Endianess
  if test -f $PBX_INCLUDE/endian.h; then
    AC_DEFINE([CS_AST_HAS_ENDIAN],1,[Found 'endian.h'])
  fi
  dnl Check Asterisk String Options
  if test -f $PBX_INCLUDE/strings.h; then
    AC_DEFINE([CS_AST_HAS_STRINGS],1,[Found 'strings.h'])
  fi
  dnl Check Asterisk Application Options
  if test -f $PBX_INCLUDE/app.h; then
    AC_MSG_CHECKING([asterisk/app.h for 'ast_app_has_voicemail.*folder'])
    if grep -q "ast_app_has_voicemail.*folder" $PBX_INCLUDE/app.h; then
      AC_DEFINE(CS_AST_HAS_NEW_VOICEMAIL,1,[Found 'new ast_app_has_voicemail' in asterisk/app.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/app.h for 'ast_app_separate_args'])
    if grep -q "ast_app_separate_args" $PBX_INCLUDE/app.h; then
      AC_DEFINE(CS_AST_HAS_APP_SEPARATE_ARGS,1,[Found 'ast_app_separate_args'])
      AC_DEFINE(sccp_app_separate_args(x,y,z,w),ast_app_separate_args(x,y,z,w),[Found 'ast_app_separate_args' in asterisk/app.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk PBX Options
  if test -f $PBX_INCLUDE/pbx.h; then
    AC_MSG_CHECKING([asterisk/pbx.h for 'ast_get_hint.*name'])
    if grep -q "ast_get_hint.*name" $PBX_INCLUDE/pbx.h; then
      AC_DEFINE(CS_AST_HAS_NEW_HINT,1,[Found 'new ast_get_hint' in asterisk/pbx.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/pbx.h for 'AST_EXTENSION_ONHOLD'])
    if grep -q "AST_EXTENSION_ONHOLD" $PBX_INCLUDE/pbx.h; then
      AC_DEFINE(CS_AST_HAS_EXTENSION_ONHOLD,1,[Found 'AST_EXTENSION_ONHOLD' in asterisk/pbx.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/pbx.h for 'AST_EXTENSION_RINGING'])
    if grep -q "AST_EXTENSION_RINGING" $PBX_INCLUDE/pbx.h; then
      AC_DEFINE(CS_AST_HAS_EXTENSION_RINGING,1,[Found 'AST_EXTENSION_RINGING' in asterisk/pbx.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Frame Options
  if test -f $PBX_INCLUDE/frame.h; then
    AC_MSG_CHECKING([asterisk/frame.h for 'void *data;'])
    if grep -q "void \*data;" $PBX_INCLUDE/frame.h; then
      AC_MSG_RESULT([Not Found])
    else
      AC_DEFINE(CS_AST_NEW_FRAME_STRUCT,1,[Found 'New Ast Frame Structure' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    fi
    AC_MSG_CHECKING([asterisk/frame.h for 'AST_CONTROL_VIDUPDATE'])
    if grep -q "AST_CONTROL_VIDUPDATE" $PBX_INCLUDE/frame.h; then
      AC_DEFINE(CS_AST_CONTROL_VIDUPDATE,1,[Found 'AST_CONTROL_VIDUPDATE' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/frame.h for 'AST_CONTROL_T38'])
    if grep -q "AST_CONTROL_T38_PARAMETERS" $PBX_INCLUDE/frame.h; then
      AC_DEFINE(CS_AST_CONTROL_T38_PARAMETERS,1,[Found 'AST_CONTROL_T38_PARAMETERS' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    elif grep -q "AST_CONTROL_T38" $PBX_INCLUDE/frame.h; then
      AC_DEFINE(CS_AST_CONTROL_T38,1,[Found 'AST_CONTROL_T38' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/frame.h for 'AST_CONTROL_SRCUPDATE'])
    if grep -q "AST_CONTROL_SRCUPDATE" $PBX_INCLUDE/frame.h; then
      AC_DEFINE(CS_AST_CONTROL_SRCUPDATE,1,[Found 'AST_CONTROL_SRCUPDATE' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/frame.h for 'AST_CONTROL_HOLD'])
    if grep -q "AST_CONTROL_HOLD" $PBX_INCLUDE/frame.h; then
      AC_DEFINE(CS_AST_CONTROL_HOLD,1,[Found 'AST_CONTROL_HOLD' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/frame.h for 'AST_CONTROL_CONNECTED_LINE'])
    if grep -q "AST_CONTROL_CONNECTED_LINE" $PBX_INCLUDE/frame.h; then
      AC_DEFINE(CS_AST_CONTROL_CONNECTED_LINE,1,[Found 'AST_CONTROL_CONNECTED_LINE' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/frame.h for 'AST_FORMAT_SIREN7'])
    if grep -q "AST_FORMAT_SIREN7" $PBX_INCLUDE/frame.h; then
      AC_DEFINE(CS_CODEC_G722_1_24K,1,[Found 'AST_FORMAT_SIREN7' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/frame.h for 'AST_CONTROL_SRCCHANGE'])
    if grep -q "AST_CONTROL_SRCCHANGE" $PBX_INCLUDE/frame.h; then
      AC_DEFINE(CS_AST_CONTROL_SRCCHANGE,1,[Found 'AST_CONTROL_SRCCHANGE' in asterisk/frame.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Device State Options
  if test -f $PBX_INCLUDE/devicestate.h; then
    AC_DEFINE(CS_AST_HAS_NEW_DEVICESTATE,1,[Found 'devicestate.h'])
    AC_MSG_CHECKING([asterisk/devicestate.h for 'enum ast_device_state' in asterisk/devicestate.h])
    if grep -q "enum ast_device_state" $PBX_INCLUDE/devicestate.h; then
      AC_DEFINE(CS_ENUM_AST_DEVICE,1,[Found 'ENUM AST_DEVICE' in asterisk/devicestate.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/devicestate.h for 'AST_DEVICE_RINGING' in asterisk/devicestate.h])
    if grep -q "AST_DEVICE_RINGING" $PBX_INCLUDE/devicestate.h; then
      AC_DEFINE(CS_AST_DEVICE_RINGING,1,[Found 'AST_DEVICE_RINGING' in asterisk/devicestate.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/devicestate.h for 'AST_DEVICE_RINGINUSE'])
    if grep -q "AST_DEVICE_RINGINUSE" $PBX_INCLUDE/devicestate.h; then
      AC_DEFINE(CS_AST_DEVICE_RINGINUSE,1,[Found 'AST_DEVICE_RINGINUSE' in asterisk/devicestate.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/devicestate.h for 'AST_DEVICE_ONHOLD'])
    if grep -q "AST_DEVICE_ONHOLD" $PBX_INCLUDE/devicestate.h; then
      AC_DEFINE(CS_AST_DEVICE_ONHOLD,1,[Found 'AST_DEVICE_ONHOLD' in asterisk/devicestate.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/devicestate.h for 'ast_devstate_changed'])
    if grep -q "ast_devstate_changed" $PBX_INCLUDE/devicestate.h; then
      AC_DEFINE(CS_NEW_DEVICESTATE,1,[Found 'new ast_devstate_changed' in asterisk/devicestate.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/devicestate.h for 'AST_DEVICE_TOTAL'])
    if grep -q "AST_DEVICE_TOTAL" $PBX_INCLUDE/devicestate.h; then
      AC_DEFINE(CS_AST_DEVICE_TOTAL,1,[Found 'AST_DEVICE_TOTAL' in asterisk/devicestate.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk CLI Options
  if test -f $PBX_INCLUDE/cli.h; then
    AC_MSG_CHECKING([asterisk/cli.h for 'ast_cli_generator'])
    if grep -q "ast_cli_generator" $PBX_INCLUDE/cli.h; then
      AC_DEFINE(CS_NEW_AST_CLI,1,[Found 'new ast_cli_generator definition' in asterisk/cli.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
    AC_MSG_CHECKING([asterisk/cli.h for 'const char * const *argv'])
    if grep -q "const char * const *argv" $PBX_INCLUDE/cli.h; then
      AC_DEFINE(CS_NEW_AST_CLI_CONST,1,[Found 'const char * const *argv' in asterisk/cli.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Lock Options
  if test -f $PBX_INCLUDE/lock.h; then
    AC_MSG_CHECKING([asterisk/lock.h for 'ast_lock_track'])
    if grep -q "ast_lock_track" $PBX_INCLUDE/lock.h; then
      AC_DEFINE(CS_AST_HAS_TRACK,1,[Found 'ast_lock_track' in asterisk/lock.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Event Options
  if test -f $PBX_INCLUDE/event.h; then
    AC_MSG_CHECKING([asterisk/event.h for 'ast_event_subscribe'])
    if grep -q "ast_event_subscribe" $PBX_INCLUDE/event.h; then
      AC_DEFINE(CS_AST_HAS_EVENT,1,[Found 'ast_event_subscribe' in asterisk/event.h])
      AC_MSG_RESULT([Found])
    else
      AC_MSG_RESULT([Not Found])
    fi
  fi
  dnl Check Asterisk Copy String Options
  AC_MSG_CHECKING([asterisk for 'ast_copy_string'])
  if grep -rq "ast_copy_string" $PBX_INCLUDE/; then
    AC_DEFINE([sccp_copy_string(x,y,z)],[ast_copy_string(x,y,z)],[Found 'ast_copy_string' in asterisk])
    AC_MSG_RESULT([Found])
  else
    AC_DEFINE([sccp_copy_string(x,y,z)],[strncpy(x,y,z - 1)],[Found 'ast_copy_string' in asterisk])
    AC_MSG_RESULT([Not Found])
  fi
  AC_MSG_CHECKING([asterisk for 'ast_string_field_'])
  if grep -rq "ast_string_field_" $PBX_INCLUDE/; then
    AC_DEFINE(CS_AST_HAS_AST_STRING_FIELD,1,[Found 'ast_string_field_' in asterisk])
    AC_MSG_RESULT([Found])
  fi



  dnl Asterisk 1.8 specific tests and settings
  if test "${ASTERISK_VER_GROUP}" == "1.8"; then
	if test "${ac_cv_have_variable_fdset}x" = "0x"; then
		AC_RUN_IFELSE(
			AC_LANG_PROGRAM([
			#include <unistd.h>
			#include <sys/types.h>
			#include <stdlib.h>
			], 
			[
			if (getuid() != 0) { exit(1); }
			]),
				dnl if ok then yes
				AC_DEFINE([CONFIGURE_RAN_AS_ROOT], 1, [Some configure tests will unexpectedly fail if configure is run by a non-root user.  These may be able to be tested at runtime.]))
			fi

	AC_MSG_CHECKING(if we can increase the maximum select-able file descriptor)
	AC_RUN_IFELSE(
		AC_LANG_PROGRAM([
			#include <stdio.h>
			#include <sys/select.h>
			#include <sys/time.h>  
			#include <sys/resource.h>
			#include <string.h>
			#include <errno.h>     
			#include <stdlib.h>
			#include <sys/types.h>
			#include <sys/stat.h>  
			#include <fcntl.h>   
			#include <unistd.h>
			], [
				dnl run program
				[
				struct rlimit rlim = { FD_SETSIZE * 2, FD_SETSIZE * 2 };
				int fd0, fd1;
				struct timeval tv = { 0, };
				struct ast_fdset { long fds_bits[[1024]]; } fds = { { 0, } };
				if (setrlimit(RLIMIT_NOFILE, &rlim)) { exit(1); }
				if ((fd0 = open("/dev/null", O_RDONLY)) < 0) { exit(1); }
				if (dup2(fd0, (fd1 = FD_SETSIZE + 1)) < 0) { exit(1); }  
				FD_SET(fd0, (fd_set *) &fds);
				FD_SET(fd1, (fd_set *) &fds);
				if (select(FD_SETSIZE + 2, (fd_set *) &fds, NULL, NULL, &tv) < 0) { exit(1); }
				exit(0)
				]
			]),
			dnl if ok then yes
			AC_MSG_RESULT(yes)
			AC_DEFINE([HAVE_VARIABLE_FDSET], 1, [Define to 1 if your system can support larger than default select bitmasks.]),
			dnl if nok else no
			AC_MSG_RESULT(no),
			dnl print cross-compile
			AC_MSG_RESULT(cross-compile)
	)

	AC_CHECK_SIZEOF([int])
	AC_CHECK_SIZEOF([long])
	AC_CHECK_SIZEOF([long long])
	AC_CHECK_SIZEOF([char *])
	AC_CHECK_SIZEOF(long)
	AC_CHECK_SIZEOF(long long)
	AC_COMPUTE_INT([ac_cv_sizeof_fd_set_fds_bits], [sizeof(foo.fds_bits[[0]])], [$ac_includes_default
	fd_set foo;])
	# This doesn't actually work; what it does is to use the variable set in the
	# previous test as a cached value to set the right output variables.
	AC_CHECK_SIZEOF(fd_set.fds_bits)

	# Set a type compatible with the previous.  We cannot just use a generic type
	# for these bits, because on big-endian systems, the bits won't match up
	# correctly if the size is wrong.  
	if test $ac_cv_sizeof_int = $ac_cv_sizeof_fd_set_fds_bits; then
	  AC_DEFINE([TYPEOF_FD_SET_FDS_BITS], [int], [Define to a type of the same size as fd_set.fds_bits[[0]]])
	  else if test $ac_cv_sizeof_long = $ac_cv_sizeof_fd_set_fds_bits; then
	    AC_DEFINE([TYPEOF_FD_SET_FDS_BITS], [long], [Define to a type of the same size as fd_set.fds_bits[[0]]])
	    else if test $ac_cv_sizeof_long_long = $ac_cv_sizeof_fd_set_fds_bits; then
	      AC_DEFINE([TYPEOF_FD_SET_FDS_BITS], [long long], [Define to a type of the same size as fd_set.fds_bits[[0]]])
	    fi
	  fi
	fi
  fi
])
