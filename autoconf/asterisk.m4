AC_DEFUN([GET_ASTERISK_VERSION], [
  CONFIGURE_PART([Checking Asterisk Version:])
  AC_CHECK_HEADER([asterisk/version.h],[
    AC_MSG_CHECKING([for version in asterisk/version.h])
dnl    if grep -q "ASTERISK_VERSION_NUM 999999" $PBX_INCLUDE/version.h; then
dnl    	if grep -q "ASTERISK_VERSION \"1.6" $PBX_INCLUDE/version.h; then
dnl        	ASTERISK_VERSION_NUM=`grep "ASTERISK_VERSION " $PBX_INCLUDE/version.h|awk -F. '{"10" $2 "0" $3}`
dnl        else
dnl        	ASTERISK_VERSION_NUM=`grep "ASTERISK_VERSION " $PBX_INCLUDE/version.h|awk -F. '{"10" $2 "00"}'`
dnl        fi
dnl        AC_DEFINE_UNQUOTED([ASTERISK_VERSION_NUM],`$ASTERISK_VERSION_NUM`, [Define ASTERISK_VERSION_NUM])
dnl    fi
dnl    AC_SUBST([ASTERISK_VERSION_NUM])
    dnl The rest is for backward support of old sources, should be removed shortly
    if grep -q "\"1\.2" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_2, 1, [Define ASTERISK_CONF_1_2])
      AC_MSG_RESULT([Found 'Asterisk Version 1.2.x'])
      REALTIME_USEABLE=0
      ASTERISK_VER=1.2
      AC_SUBST([ASTERISK_VER])
    elif grep -q "\"SVN-branch-1\.2" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_2, 1, [Define ASTERISK_CONF_1_2])
      AC_MSG_RESULT([Found 'Asterisk Version 1.2.x (Branch)'])
      REALTIME_USEABLE=0
      ASTERISK_VER=1.2
      AC_SUBST([ASTERISK_VER])
    elif grep -q "\"1\.4" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_4, 1, [Define ASTERISK_CONF_1_4])
      AC_MSG_RESULT([Found 'Asterisk Version 1.4.x'])
      REALTIME_USEABLE=1
      ASTERISK_VER=1.4
      AC_SUBST([ASTERISK_VER])
    elif grep -q "\"SVN-branch-1\.4" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_4, 1, [Define ASTERISK_CONF_1_4])
      AC_MSG_RESULT([Found 'Asterisk Version 1.4.x (Branch)'])
      REALTIME_USEABLE=1
      ASTERISK_VER=1.4
      AC_SUBST([ASTERISK_VER])
    elif grep -q "1\.6" $PBX_INCLUDE/version.h; then
      AC_DEFINE(ASTERISK_CONF_1_6, 1, [Define ASTERISK_CONF_1_6])
      AC_MSG_RESULT([Found 'Asterisk Version 1.6.x'])
      REALTIME_USEABLE=1
      if grep -q "\"1\.6\.0" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_0, 0, [Define ASTERISK_CONF_1_6_0])
        AC_MSG_RESULT([Found 'Asterisk Version 1.6.1'])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.0
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"1\.6\.1" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_1, 1, [Define ASTERISK_CONF_1_6_1])
        AC_MSG_RESULT([Found 'Asterisk Version 1.6.1'])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.1
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"1\.6\.2" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_2, 1, [Define ASTERISK_CONF_1_6_2])
        AC_MSG_RESULT([Found 'Asterisk Version 1.6.2'])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.2
        AC_SUBST([ASTERISK_VER])
        
      elif grep -q "\"SVN-branch-1\.6\.0" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_0, 1, [Define ASTERISK_CONF_1_6_0])
        AC_MSG_RESULT([Found 'Asterisk Version 1.6.x (Branch)'])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.0
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"SVN-branch-1\.6\.1" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_1, 1, [Define ASTERISK_CONF_1_6_1])
        AC_MSG_RESULT([Found 'Asterisk Version 1.6.x (Branch)'])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.1
        AC_SUBST([ASTERISK_VER])
      elif grep -q "\"SVN-branch-1\.6\.2" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_2, 1, [Define ASTERISK_CONF_1_6_2])
        AC_MSG_RESULT([Found 'Asterisk Version 1.6.x (Branch)'])
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.2
        AC_SUBST([ASTERISK_VER])

      elif grep -q "trunk" $PBX_INCLUDE/version.h; then
        AC_DEFINE(ASTERISK_CONF_1_6_2, 1, [Define ASTERISK_CONF_1_6_2])
        AC_MSG_RESULT([Found 'Asterisk Version 1.6.x (Trunk)'])
        REALTIME_USEABLE=1
        REALTIME_USEABLE=1
        ASTERISK_VER=1.6.2
        AC_SUBST([ASTERISK_VER])
      fi
    else 
      echo ""
      echo ""
      echo "PBX version could not be determined"
      echo "==================================="
      echo "Either install asterisk and asterisk-devel packages"
      echo "Or specify the location where asterisk can be found, using ./configure --with-asterisk=[path]"
      exit
    fi],
    [AC_MSG_RESULT(Not Found 'asterisk/version.h')]
  )
])

AC_DEFUN([FIND_ASTERISK_HEADERS],[
  CONFIGURE_PART([Checking Asterisk Headers:])
  AC_CHECK_HEADER([asterisk/lock.h],AC_DEFINE(HAVE_PBX_LOCK_H,1,[Found 'asterisk/lock.h']),[lock_compiled=no],[#include <asterisk.h>])
  AS_IF([test "${lock_compiled}" != "no"], [
    AC_CHECK_HEADER([asterisk/acl.h],		AC_DEFINE(HAVE_PBX_ACL_H,1,[Found 'asterisk/acl.h']),,        	 	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/app.h],		AC_DEFINE(HAVE_PBX_APP_H,1,[Found 'asterisk/app.h']),,			[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/astdb.h],		AC_DEFINE(HAVE_PBX_ASTDB_H,1,[Found 'asterisk/astdb.h']),,   		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/callerid.h],	AC_DEFINE(HAVE_PBX_CALLERID_H,1,[Found 'asterisk/callerid.h']),, 		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/causes.h],	AC_DEFINE(HAVE_PBX_CAUSES_H,1,[Found 'asterisk/causes.h']),,      		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/channel.h],	AC_DEFINE(HAVE_PBX_CHANNEL_H,1,[Found 'asterisk/channel.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/channel_pvt.h],	AC_DEFINE(HAVE_PBX_CHANNEL_pvt_H,1,[Found 'asterisk/channel_pvt.h']),, 	[ #include <asterisk.h> ]) 
    AC_CHECK_HEADER([asterisk/cli.h],		AC_DEFINE(HAVE_PBX_CLI_H,1,[Found 'asterisk/cli.h']),,       	  	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/config.h],	AC_DEFINE(HAVE_PBX_CONFIG_H,1,[Found 'asterisk/config.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/devicestate.h],	AC_DEFINE(HAVE_PBX_DEVICESTATE_H,1,[Found 'asterisk/devicestate.h']),,	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/endian.h],	AC_DEFINE(HAVE_PBX_ENDIAN_H,1,[Found 'asterisk/endian.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/event.h],		AC_DEFINE(HAVE_PBX_EVENT_H,1,[Found 'asterisk/event.h']),,			[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/features.h],	AC_DEFINE(HAVE_PBX_FEATURES_H,1,[Found 'asterisk/features.h']),,    	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/frame.h],		AC_DEFINE(HAVE_PBX_FRAME_H,1,[Found 'asterisk/frame.h']),,     	  	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/logger.h],	AC_DEFINE(HAVE_PBX_LOGGER_H,1,[Found 'asterisk/logger.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/manager.h],	AC_DEFINE(HAVE_PBX_MANAGER_H,1,[Found 'asterisk/manager.h']),,     	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/module.h],	AC_DEFINE(HAVE_PBX_MODULE_H,1,[Found 'asterisk/module.h']),,      		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/musiconhold.h],	AC_DEFINE(HAVE_PBX_MUSICONHOLD_H,1,[Found 'asterisk/musiconhold.h']),, 	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/options.h],	AC_DEFINE(HAVE_PBX_OPTIONS_H,1,[Found 'asterisk/options.h']),,     	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/pbx.h],		AC_DEFINE(HAVE_PBX_PBX_H,1,[Found 'asterisk/pbx.h']),,			[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/rtp.h],		AC_DEFINE(HAVE_PBX_RTP_H,1,[Found 'asterisk/rtp.h']),,			[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/sched.h],		AC_DEFINE(HAVE_PBX_SCHED_H,1,[Found 'asterisk/sched.h']),,			[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/strings.h],	AC_DEFINE(HAVE_PBX_STRINGS_H,1,[Found 'asterisk/strings.h']),,		[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/stringfields.h],	AC_DEFINE(HAVE_PBX_STRINGFIELDS_H,1,[Found 'asterisk/stringfields.h']),,	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/translate.h],	AC_DEFINE(HAVE_PBX_TRANSLATE_H,1,[Found 'asterisk/translate.h']),,   	[ #include <asterisk.h>])
    AC_CHECK_HEADER([asterisk/utils.h],		AC_DEFINE(HAVE_PBX_UTILS_H,1,[Found 'asterisk/utils.h']),,       		[ #include <asterisk.h>])
  ])
])


AC_DEFUN([CHECK_ASTERISK_HEADER_CONTENT],[
  CONFIGURE_PART([Checking Asterisk Header Content:])
  dnl AC_CHECK_HEADER (header-file, action-if-found, [action-if-not-found])
  dnl AC_EGREP_HEADER (pattern, header-file, action-if-found, [action-if-not-found])

  dnl Check Asterisk RealTime Options
  if test $REALTIME_USEABLE = 1 ; then
    AC_CHECK_HEADER(asterisk/buildopts.h,
      AC_MSG_CHECKING([checking version in asterisk/buildopts.h])
      AC_EGREP_HEADER([define DEBUG_CHANNEL_LOCKS], asterisk/buildopts.h,
        AC_DEFINE(CS_AST_DEBUG_CHANNEL_LOCKS,1,[Found 'DEBUG_CHANNEL_LOCKS' in asterisk/buildopts.h])
      )
      AC_EGREP_HEADER([define DEBUG_THREADS], asterisk/buildopts.h,
        AC_DEFINE(CS_AST_DEBUG_THREADS,1,[Found 'DEBUG_THREADS' in asterisk/buildopts.h])
      )
      AC_MSG_RESULT(['OK'])
    )
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
      AC_DEFINE([ast_config_load(x)],[ast_load(x)],[Not Found 'ast_config_load' in asterisk/config.h])
      AC_DEFINE([ast_config_destroy(x)],[ast_destroy(x)],[Not Found 'ast_config_load' in asterisk/config.h])
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
  dnl Check Asterisk Jitterbuffer
  AC_MSG_CHECKING([asterisk jitterbuffer for 'target_extra'])
  if grep -rq "target_extra" $PBX_INCLUDE/abstract_jb.h; then
    AC_DEFINE([CS_AST_JB_TARGET_EXTRA],1,[Found 'target_extra' in asterisk/abstract_jb.h])
    AC_MSG_RESULT([Found])
  else
    AC_MSG_RESULT([Not Found])
  fi
])
