
	dnl FILE: asterisk.m4
dnl COPYRIGHT: chan-sccp-b.sourceforge.net group 2009
dnl CREATED BY: Created by Diederik de Groot
dnl LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
dnl          See the LICENSE file at the top of the source tree.
dnl DATE: $Date: $
dnl REVISION: $Revision: $

AC_DEFUN([AST_GET_VERSION], [
	CONFIGURE_PART([Checking Asterisk Version:])
	REALTIME_USEABLE=1
	version_found=0
	
	AC_CHECK_HEADER([asterisk/version.h],[
		AC_MSG_CHECKING([version in asterisk/version.h])
		AC_TRY_COMPILE([
			#include <asterisk/version.h>
		],[
			const char *test_src = ASTERISK_VERSION;
		], [
			AC_MSG_RESULT(processing...)
			pbx_ver=$(eval "$ac_cpp conftest.$ac_ext" 2>/dev/null | $EGREP test_src |$EGREP -o '\".*\"')

			# process tgz, branch, trunk
			if echo $pbx_ver|grep -q "\"SVN-branch"; then
				ASTERISK_REPOS_LOCATION=BRANCH
				pbx_ver=${pbx_ver:12}						# remove head
			elif echo $pbx_ver|grep -q "\"SVN-trunk"; then
				ASTERISK_REPOS_LOCATION=TRUNK
				pbx_ver=${pbx_ver:11}						# remove head
				if test "${pbx_ver:0:1}" == "r" ; then
					pbx_ver="1.10.1"					# default trunk without number
				fi
			else
				ASTERISK_REPOS_LOCATION=TGZ
			fi

			# remove from pbx_ver
			# remove from pbx_ver
			pbx_ver=${pbx_ver%%-*}							# remove tail
			#pbx_ver=${pbx_ver//\"/}						# remove '"'
			pbx_ver=`echo ${pbx_ver} | sed 's/"//g'`

			# process version number
			for x in "1.2" "1.4" "1.6" "1.8" "1.10" "10" "11"; do
				if test $version_found == 0; then
                                        if echo $pbx_ver|grep -q "$x"; then
                                                if test ${#x} -gt 3; then		# 1.10
                                                        ASTERISK_VER_GROUP="`echo $x|sed 's/\.//g'`"
                                                elif test ${#x} -lt 3; then		# 1.10
                                                        ASTERISK_VER_GROUP="110"
                                                else
                                                        ASTERISK_VER_GROUP="`echo $x|sed 's/\./0/g'`"
                                                fi
                                                AC_SUBST([ASTERISK_VER_GROUP])
                                                
                                                if test "$ASTERISK_VER_GROUP" == "102"; then						# switch off realtime for 1.2
                                                        REALTIME_USEABLE=0
                                                fi
                                                
                                                #ASTERISK_MINOR_VER=${pbx_ver/$x\./}							# remove leading '1.x.'
                                                ASTERISK_MINOR_VER=`echo $pbx_ver|sed "s/^$x.\([0-9]*\)\(.*\)/\1/g"`			# remove leading and trailing .*
                                                #ASTERISK_MINOR_VER1=${ASTERISK_MINOR_VER%%.*}						# remove trailing '.*'
                                                #if test ${#ASTERISK_MINOR_VER1} -gt 1; then
                                                        #ASTERISK_VERSION_NUMBER="${ASTERISK_VER_GROUP}${ASTERISK_MINOR_VER1}"		# add only third version part
                                                #else
                                                        #ASTERISK_VERSION_NUMBER="${ASTERISK_VER_GROUP}0${ASTERISK_MINOR_VER1}"		# add only third version part
                                                #fi
                                                #ASTERISK_STR_VER="${x}.${ASTERISK_MINOR_VER1}"

                                                #ASTERISK_MINOR_VER=${pbx_ver/$x\./}							# remove leading '1.x.'
                                                ASTERISK_MINOR_VER=`echo $pbx_ver|sed "s/^$x.\([0-9]*\)\(.*\)/\1/g"`			# remove leading and trailing .*
                                                ASTERISK_MINOR_VER1=${ASTERISK_MINOR_VER%%.*}						# remove trailing '.*'
                                                #if test ${#ASTERISK_MINOR_VER1} -gt 1; then
                                                        #ASTERISK_VERSION_NUMBER="${ASTERISK_VER_GROUP}${ASTERISK_MINOR_VER1}"		# add only third version part
                                                #else
                                                        ASTERISK_VERSION_NUMBER="${ASTERISK_VER_GROUP}0${ASTERISK_MINOR_VER1}"		# add only third version part
                                                #fi

                                                #ASTERISK_STR_VER="${x}.${ASTERISK_MINOR_VER1}"
                                                
                                                version_found=1
                                                AC_MSG_RESULT([Found 'Asterisk Version ${ASTERISK_VERSION_NUMBER} ($x)'])

                                                AC_DEFINE_UNQUOTED([ASTERISK_VERSION_NUMBER],`echo ${ASTERISK_VERSION_NUMBER}`,[ASTERISK Version Number])
                                                AC_SUBST([ASTERISK_VERSION_NUMBER])
                                                AC_DEFINE_UNQUOTED([ASTERISK_VERSION_GROUP],`echo ${ASTERISK_VER_GROUP}`,[ASTERISK Version Group])
                                                AC_SUBST([ASTERISK_VER_GROUP])
                                                AC_DEFINE_UNQUOTED([ASTERISK_REPOS_LOCATION],`echo ${ASTERISK_REPOS_LOCATION}`,[ASTERISK Source Location])
                                                AC_SUBST([ASTERISK_REPOS_LOCATION])

                                                case "${ASTERISK_VER_GROUP}" in
                                                        102) AC_DEFINE([ASTERISK_CONF_1_2], [1], [Defined ASTERISK_CONF_1_2]);;
                                                        104) AC_DEFINE([ASTERISK_CONF_1_4], [1], [Defined ASTERISK_CONF_1_4]);;
                                                        106) AC_DEFINE([ASTERISK_CONF_1_6], [1], [Defined ASTERISK_CONF_1_6]);;
                                                        108) AC_DEFINE([ASTERISK_CONF_1_8], [1], [Defined ASTERISK_CONF_1_8]);;
                                                        110) AC_DEFINE([ASTERISK_CONF_1_10], [1], [Defined ASTERISK_CONF_1_10]);;
                                                        111) AC_DEFINE([ASTERISK_CONF_1_11], [1], [Defined ASTERISK_CONF_1_11]);;
                                                        *) AC_DEFINE([ASTERISK_CONF], [0], [NOT Defined ASTERISK_CONF !!]);;
                                                esac 

                                                if [ test ${ASTERISK_VER_GROUP} -lt ${MIN_ASTERISK_VERSION} ]; then
                                                        echo ""
                                                        CONFIGURE_PART([Asterisk Version ${ASTERISK_VER} Not Supported])
                                                        echo ""
                                                        echo "This version of chan-sccp-b only has support for Asterisk 1.6.x and above."
                                                        echo ""
                                                        echo "Please install a higher version of asterisk"
                                                        echo ""
                                                        echo ""
                                                        exit 255
                                                fi
                                                if [ test ${ASTERISK_VER_GROUP} -gt ${MAX_ASTERISK_VERSION} ]; then
                                                        echo ""
                                                        CONFIGURE_PART([Asterisk Version ${ASTERISK_VER} Not Supported])
                                                        echo ""
                                                        echo "This version of chan-sccp-b only has support for Asterisk 1.11.x and below."
                                                        echo ""
                                                        echo "Please install a lower version of asterisk"
                                                        echo ""
                                                        echo ""
                                                        exit 255
                                                fi
                                        fi
				fi 
			done
			if test $version_found == 0; then
				echo ""
				echo ""
				echo "PBX branch version could not be determined"
				echo "==================================="
				echo "Either install asterisk and asterisk-devel packages"
				echo "Or specify the location where asterisk can be found, using ./configure --with-asterisk=[path]"
				exit
			fi
			AC_MSG_RESULT(done)
		],[
			AC_MSG_RESULT('ASTERISK_VERSION could not be established)
		])
	], [
		AC_CHECK_HEADER(
			[asterisk/ast_version.h],
			[
				AC_MSG_CHECKING([version in asterisk/ast_version.h])
				ASTERISK_VER_GROUP=112
				ASTERISK_VERSION_NUMBER=11200
				ASTERISK_REPOS_LOCATION=TRUNK

				AC_DEFINE([ASTERISK_CONF_1_12], [1], [Defined ASTERISK_CONF_1_12])
				AC_DEFINE([ASTERISK_VERSION_NUMBER], [11200], [ASTERISK Version Number])
				AC_DEFINE([ASTERISK_VERSION_GROUP], [112], [ASTERISK Version Group])
				AC_DEFINE([ASTERISK_REPOS_LOCATION], ["TRUNK"],[ASTERISK Source Location])
				
				version_found=1
				AC_MSG_RESULT(done)
	                        AC_MSG_RESULT([WARNING: Experimental at the moment. Anything might break.])
			],[
				AC_MSG_RESULT(['ASTERISK_VERSION could not be established'])
			]
		)
	])
	if test $version_found == 0; then
		echo ""
		echo ""
		echo "PBX branch version could not be determined"
		echo "==================================="
		echo "Either install asterisk and asterisk-devel packages"
		echo "Or specify the location where asterisk can be found, using ./configure --with-asterisk=[path]"
		exit
	fi
	
])

dnl Find Asterisk Header Files
AC_DEFUN([AST_CHECK_HEADERS],[
  CONFIGURE_PART([Checking Asterisk Headers:])
  
  HEADER_INCLUDE="
	#undef PACKAGE
	#undef PACKAGE_BUGREPORT
	#undef PACKAGE_NAME
	#undef PACKAGE_STRING
	#undef PACKAGE_TARNAME
	#undef PACKAGE_VERSION
	#undef VERSION
  	#if ASTERISK_VERSION_NUMBER >= 10400
	#  include <asterisk.h>
	#endif
	#include <asterisk/autoconfig.h>
	#include <asterisk/buildopts.h>
  "
    
  AC_CHECK_HEADER([asterisk/lock.h],
                [
                        lock_compiled=yes
                        AC_DEFINE(HAVE_PBX_LOCK_H,1,[Found 'asterisk/lock.h'])
			CS_CHECK_AST_TYPEDEF([struct ast_lock_track],[asterisk/lock.h],AC_DEFINE([CS_AST_LOCK_TRACK],1,[Asterisk Lock Track Support]))
                ],[
                	lock_compiled=no
                ],[	
                	$HEADER_INCLUDE
                ]
  )

  AS_IF([test "${lock_compiled}" != "no"], [
    AC_DEFINE([PBX_CHANNEL_TYPE],[struct ast_channel],[Define PBX_CHANNEL_TYPE as 'struct ast_channel'])
    AC_CHECK_HEADER([asterisk/acl.h],
		[
	                AC_DEFINE(HAVE_PBX_ACL_H,1,[Found 'asterisk/acl.h'])
			CS_CHECK_AST_TYPEDEF([struct ast_ha],[asterisk/acl.h],AC_DEFINE([CS_AST_HA],1,['struct ast_ha' available]))
	    	],,[
                	$HEADER_INCLUDE
	    	]
    )
    AC_CHECK_HEADER([asterisk/buildopts.h], 
		[
			AC_DEFINE(HAVE_PBX_BUILDOPTS_H,1,[Found 'asterisk/buildopts.h'])
	                CS_CV_TRY_COMPILE_DEFINE([ - availability 'debug_channel_locks'...],[ac_cv_ast_debug_channel_locks],[
		                	$HEADER_INCLUDE
				], [
                                	int test_format = (int)DEBUG_CHANNEL_LOCKS;
                                ], [CS_AST_DEBUG_CHANNEL_LOCKS],
                                ['DEBUG_CHANNEL_LOCKS' available]
                        )

	                CS_CV_TRY_COMPILE_DEFINE([ - availability 'debug_threads'...],[ac_cv_ast_debug_threads],[
		                	$HEADER_INCLUDE
                                ], [
                                	int test_format = (int)DEBUG_THREADS;
                                ], [CS_AST_DEBUG_THREADS],
                                ['DEBUG_THREADS' available]
                        )
			dnl Trick to compile against asterisk 1.6.2 to fix AST_MUTEX_DEFINE_STATIC issue, cause by #include order
                        if [ test ${ASTERISK_VER_GROUP} == 106 ]; then
                                AC_DEFINE(DEBUG_THREADS,1,[Fake DEBUG_THREADS to please 'asterisk/lock.h' when compiling against asterisk 1.6.2])
                        fi
                        AC_MSG_CHECKING([ - if asterisk was compiled with the 'LOW_MEMORY' buildoption...])                              
                        AC_EGREP_HEADER([yes], [
                        #include <asterisk/buildopts.h>
                        #ifdef LOW_MEMORY
                        	yes
                        #endif],                            
                        [                                                                                         
                                AC_DEFINE([CS_LOW_MEMORY],1,[asterisk compiled with 'LOW_MEMORY'])                  
                                AC_MSG_RESULT(yes)                                                                
                        ],[                                                                                       
                                AC_MSG_RESULT(no)                                                                 
                        ])                
		],,[
	               	$HEADER_INCLUDE
		]
    )
    AC_CHECK_HEADER([asterisk/abstract_jb.h],
    		[
	    		AC_DEFINE(HAVE_PBX_ABSTRACT_JB_H,1,[Found 'asterisk/abstract_jb.h'])
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_jb.target_extra'...],[ac_cv_ast_jb_target_extra],[
		                	$HEADER_INCLUDE
					#include <asterisk/abstract_jb.h>
				], [
					struct ast_jb_conf test_jbconf;
					test_jbconf.target_extra = (long)1;
				], 
				[CS_AST_JB_TARGETEXTRA],['CS_AST_JB_TARGETEXTRA' available]
			)						
		],,[ 
	               	$HEADER_INCLUDE
	           ]
    )
    AC_CHECK_HEADER([asterisk/linkedlists.h],	AC_DEFINE(HAVE_PBX_LINKEDLISTS_H,1,[Found 'asterisk/linkedlists.h']),,[ 
               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/app.h],
		[	
			AC_DEFINE(HAVE_PBX_APP_H,1,[Found 'asterisk/app.h'])
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_app_has_voicemail'...],[ac_cv_ast_app_has_voicemail],[
		                	$HEADER_INCLUDE
					#ifdef HAVE_PBX_LINKEDLISTS_H
					#include <asterisk/linkedlists.h>
					#endif		
					#include <asterisk/app.h>
				], [
					int test_vm = ast_app_has_voicemail(NULL, NULL);
				], [CS_AST_HAS_NEW_VOICEMAIL],['ast_app_has_voicemail' available]
			)

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_app_separate_args'...],[ac_cv_ast_app_seperate_args], [
		                	$HEADER_INCLUDE
					#ifdef HAVE_PBX_LINKEDLISTS_H
					#include <asterisk/linkedlists.h>
					#endif		
					#include <asterisk/app.h>
				], [
					unsigned int test_args = ast_app_separate_args(NULL, NULL, NULL, 0);
				], [CS_AST_HAS_APP_SEPARATE_ARGS],['ast_app_separate_args' available]
			)
			if test $ac_cv_ast_app_seperate_args = yes; then
			        AC_DEFINE(sccp_app_separate_args(x,y,z,w),ast_app_separate_args(x,y,z,w),[Found 'ast_app_separate_args' in asterisk/app.h])
		        fi
		],,[
                	$HEADER_INCLUDE
			#ifdef HAVE_PBX_LINKEDLISTS_H
			#include <asterisk/linkedlists.h>
			#endif		
    ])
    AC_CHECK_HEADER([asterisk/astdb.h],		
    		[
    			AC_DEFINE(HAVE_PBX_ASTDB_H,1,[Found 'asterisk/astdb.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/network.h],
    		[
    			AC_DEFINE(HAVE_PBX_NETWORK_H,1,[Found 'asterisk/astdb.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/callerid.h],
    		[
    			AC_DEFINE(HAVE_PBX_CALLERID_H,1,[Found 'asterisk/callerid.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/causes.h],
    		[
    			AC_DEFINE(HAVE_PBX_CAUSES_H,1,[Found 'asterisk/causes.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/channel.h],
		[
			AC_DEFINE(HAVE_PBX_CHANNEL_H,1,[Found 'asterisk/channel.h'])

			CS_CHECK_AST_TYPEDEF([struct ast_channel_tech],[asterisk/channel.h],AC_DEFINE([CS_AST_HAS_TECH_PVT],1,['struct ast_channel_tech' available]))

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_bridged_channel'...],[ac_cv_ast_bridged_channel],[
		               	$HEADER_INCLUDE
				#include <asterisk/channel.h>
				], [
					struct ast_channel *test_bridged_channel = ast_bridged_channel(NULL);
				], [CS_AST_HAS_BRIDGED_CHANNEL],['ast_bridged_channel' available]
			)
			
			CS_CHECK_AST_TYPEDEF([struct ast_callerid],[asterisk/channel.h],AC_DEFINE([CS_AST_CHANNEL_HAS_CID],1,['struct ast_callerid' available]))

			CS_CV_TRY_COMPILE_IFELSE([ - availability 'max_musicclass'...],[ac_cv_ast_max_musicclass], [
		               	$HEADER_INCLUDE
					#include <asterisk/channel.h>
				], [
					int test_musicclass = (int)MAX_MUSICCLASS;
				], [
				], [AC_DEFINE([MAX_MUSICCLASS],[MAX_LANGUAGE],['MAX_MUSICCLASS' replacement])]				
			)
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_flag_moh'...],[ac_cv_ast_flag_moh], [
		               	$HEADER_INCLUDE
				#include <asterisk/channel.h>
				], [
					int test_moh = (int)AST_FLAG_MOH;
				], [CS_AST_HAS_FLAG_MOH],['AST_FLAG_MOH' available]
			)

			CS_CV_TRY_COMPILE_IFELSE([ - availability 'ast_max_context'...], [ac_cv_ast_max_context], [
		               	$HEADER_INCLUDE
					#include <asterisk/channel.h>
				], [
					int test_context = (int)AST_MAX_CONTEXT;
				], [
				], [AC_DEFINE([AST_MAX_CONTEXT],[AST_MAX_EXTENSION],['AST_MAX_CONTEXT' replacement])
			])

			CS_CV_TRY_COMPILE_IFELSE([ - availability 'ast_max_account_code'...], [ac_cv_ast_max_account_code], [
		               	$HEADER_INCLUDE
					#include <asterisk/channel.h>
				], [
					int test_account_code = (int)AST_MAX_ACCOUNT_CODE;
				], 
				[AC_DEFINE([SCCP_MAX_ACCOUNT_CODE],[AST_MAX_ACCOUNT_CODE],[Found 'AST_MAX_ACCOUNT_CODE' in asterisk/channel.h])],
				[AC_DEFINE([SCCP_MAX_ACCOUNT_CODE],[MAX_LANGUAGE],[Not Found 'AST_MAX_ACCOUNT_CODE' in asterisk/channel.h])]
			)
			
			CS_CHECK_AST_TYPEDEF([ast_group_t],[asterisk/channel.h],AC_DEFINE([CS_AST_HAS_AST_GROUP_T],1,['ast_group_t' available]))
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/channel_pvt.h],
    		[
    			AC_DEFINE(HAVE_PBX_CHANNEL_pvt_H,1,[Found 'asterisk/channel_pvt.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/cli.h],
		[
			AC_DEFINE(HAVE_PBX_CLI_H,1,[Found 'asterisk/cli.h'])

			AC_MSG_CHECKING([ - availability 'ast_cli_generator'...])
			AC_EGREP_HEADER([ast_cli_generator\(const], [asterisk/cli.h],
			[
				AC_DEFINE(CS_NEW_AST_CLI,1,[Found 'new ast_cli_generator definition' in asterisk/cli.h])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])

			AC_MSG_CHECKING([ - availability 'new ast cli using const.'...])
			AC_EGREP_HEADER([const char \* const \*argv], [asterisk/cli.h],
			[
				AC_DEFINE(CS_NEW_AST_CLI_CONST,1,[Found 'const char * const *argv' in asterisk/cli.h])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])		
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/config.h],
    		[
    			AC_DEFINE(HAVE_PBX_CONFIG_H,1,[Found 'asterisk/config.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/extconf.h],
    		[
    			AC_DEFINE(HAVE_PBX_EXTCONF_H,1,[Found 'asterisk/extconf.h'])
    			AC_DEFINE(PBX_VARIABLE_TYPE,[struct ast_variable],[Defined PBX_VARIABLE as 'struct ast_variable'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/devicestate.h],
		[
			AC_DEFINE(HAVE_PBX_DEVICESTATE_H,1,[Found 'asterisk/devicestate.h'])			
			DEVICESTATE_H=yes
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_devstate_changed'...], [ac_cv_ast_devstate_changed], [
			               	$HEADER_INCLUDE
					#include <asterisk/devicestate.h>
				], [
					ast_devstate_changed(AST_DEVICE_UNKNOWN, "SCCP/%s", "test");
				], [CS_DEVICESTATE],['ast_devstate_changed' available]
			)
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_devstate_changed (with cache option)'...], [ac_cv_ast_devstate_changed_with_cache], [
			               	$HEADER_INCLUDE
					#include <asterisk/devicestate.h>
				], [
					int test_devicestate_changed = ast_devstate_changed(AST_DEVICE_UNKNOWN, AST_DEVSTATE_CACHABLE, "SCCP/%s", "test");
				], [CS_NEW_DEVICESTATE_WITH_CACHE],['ast_devstate_changed' available]
			)

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'cacheable ast_devstate_changed'...], [ac_cv_cacheable_ast_devstate_changed], [
			               	$HEADER_INCLUDE
					#include <asterisk/devicestate.h>
				], [
					ast_devstate_changed(AST_DEVICE_UNKNOWN, AST_DEVSTATE_CACHABLE, "SCCP/%s", "test");
				], [CS_CACHEABLE_DEVICESTATE],['cacheable ast_devstate_changed' available]
			)
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_device_ringing'...], [ac_cv_ast_device_ringing], [
			               	$HEADER_INCLUDE
					#include <asterisk/devicestate.h>
				], [
					int test_device_ringing = (int)AST_DEVICE_RINGING;
				], [CS_AST_DEVICE_RINGING],['AST_DEVICE_RINGING' available]
			)
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_device_ringinuse'...], [ac_cv_ast_device_ringinuse], [
			               	$HEADER_INCLUDE
					#include <asterisk/devicestate.h>
				], [
					int test_device_ringinuse = (int)AST_DEVICE_RINGINUSE;
				], [CS_AST_DEVICE_RINGINUSE],['AST_DEVICE_RINGINUSE' available]
			)

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_device_onhold'...], [ac_cv_ast_device_onhold], [
			               	$HEADER_INCLUDE
					#include <asterisk/devicestate.h>
				], [
					int test_device_onhold = (int)AST_DEVICE_ONHOLD;
				], [CS_AST_DEVICE_ONHOLD],['AST_DEVICE_ONHOLD' available]
			)

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_device_total'...], [ac_cv_ast_device_total], [
			               	$HEADER_INCLUDE
					#include <asterisk/devicestate.h>
				], [
					int test_device_total = (int)AST_DEVICE_TOTAL;
				], [CS_AST_DEVICE_TOTAL],['AST_DEVICE_TOTAL' available]
			)
dnl			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_enable_distributed_devstate'...], [ac_cv_ast_enable_distributed_devstate], [
dnl			               	$HEADER_INCLUDE
dnl					#include <asterisk/devicestate.h>
dnl				], [
dnl					int test_device_total = (int)ast_enable_distributed_devstate();
dnl				], [CS_AST_ENABLE_DISTRIBUTED_DEVSTATE],['CS_AST_ENABLE_DISTRIBUTED_DEVSTATE' available]
dnl			)
			AC_MSG_CHECKING([ - availability 'ast_enable_distributed_devstate'...])
			AC_EGREP_HEADER([ast_enable_distributed_devstate], [asterisk/devicestate.h],
			[
				AC_DEFINE(CS_AST_ENABLE_DISTRIBUTED_DEVSTATE,1,[Found 'ast_enable_distributed_devstate' in asterisk/devicestate.h])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/endian.h],	
    		[
			AC_DEFINE([CS_AST_HAS_ENDIAN],1,[Found 'asterisk/endian.h'])
			AC_DEFINE([HAVE_PBX_ENDIAN_H],1,[Found 'asterisk/endian.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/event.h],	
    		[
    			AC_DEFINE(HAVE_PBX_EVENT_H,1,[Found 'asterisk/event.h'])
    			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_event_subscribe'...], [ac_cv_ast_event_subscribe], [
			               	$HEADER_INCLUDE
					#include <asterisk/event.h>
				], [
					ast_event_cb_t test_cb;
					void *data;
					struct ast_event_sub *test_event_sub = ast_event_subscribe(AST_EVENT_MWI, test_cb, "mailbox subscription", data, AST_EVENT_IE_MAILBOX, AST_EVENT_IE_PLTYPE_STR, NULL, AST_EVENT_IE_CONTEXT, AST_EVENT_IE_PLTYPE_STR, "default", AST_EVENT_IE_END);
				], [CS_AST_HAS_EVENT],['ast_event_subscribe' available]
			)

		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/event_defs.h],	
    		[
    			AC_DEFINE(HAVE_PBX_EVENT_DEFS_CIDNAME_H,1,[Found 'asterisk/event_defs.h'])
    			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ac_cv_ast_event_defs_cidname'...], [ac_cv_ast_event_defs_cidname], [
			               	$HEADER_INCLUDE
					#include <asterisk/event_defs.h>
				], [
					int data = AST_EVENT_IE_CEL_CIDNAME;				
				], [CS_AST_HAS_EVENT_CIDNAME],['ac_cv_ast_event_defs_cidname' available]
			)

		],,[ 
	               	$HEADER_INCLUDE
    ])
    
    
    AC_CHECK_HEADER([asterisk/features.h],
    		[
    			AC_DEFINE(HAVE_PBX_FEATURES_H,1,[Found 'asterisk/features.h'])

			AC_MSG_CHECKING([ - availability 'ast_do_pickup'...])
			AC_EGREP_HEADER([ast_do_pickup], [asterisk/features.h],
			[
				AC_DEFINE(CS_AST_DO_PICKUP,1,[Found 'ast_do_pickup' in asterisk/features.h])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/frame.h],
		[
			AC_DEFINE(HAVE_PBX_FRAME_H,1,[Found 'asterisk/frame.h'])			
			AC_DEFINE(PBX_FRAME_TYPE,[struct ast_frame],[Define PBX_FRAME as 'struct ast_frame'])
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_frame.data.ptr'...], [ac_cv_ast_frame_data_ptr], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					struct ast_frame test_frame = { AST_FRAME_DTMF, };
					test_frame.data.ptr = NULL;
				], [CS_AST_NEW_FRAME_STRUCT], [new frame data.ptr]
			)

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_vidupdate' is available...], [ac_cv_ast_control_vidupdate], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_vidupdate = (int)AST_CONTROL_VIDUPDATE;
				], [CS_AST_CONTROL_VIDUPDATE], ['AST_CONTROL_VIDUPDATE' available]
			)
			
			CS_CV_TRY_COMPILE_IFELSE([ - availability 'ast_control_t38_parameters'...], [ac_cv_ast_control_t38_parameters], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_T38_param = (int)AST_CONTROL_T38_PARAMETERS;
				], [AC_DEFINE(CS_AST_CONTROL_T38_PARAMETERS,1,['AST_CONTROL_T38_PARAMETERS' available])
				], [
				CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_t38'...], [ac_cv_ast_control_t38], [
				               	$HEADER_INCLUDE
						#include <asterisk/frame.h>
					], [
						int test_control_T38 = (int)AST_CONTROL_T38;
					], [CS_AST_CONTROL_T38], ['AST_CONTROL_T38_PARAMETERS' available]
				)
			])

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_srcupdate'...], [ac_cv_ast_control_srcupdate], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_srcupdate = (int)AST_CONTROL_SRCUPDATE;
				], [CS_AST_CONTROL_SRCUPDATE], ['AST_CONTROL_SRCUPDATE' available]
			)

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_incomplete'...], [ac_cv_ast_control_incomplete], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_incomplete = (int)AST_CONTROL_INCOMPLETE;
				], [CS_AST_CONTROL_INCOMPLETE], ['AST_CONTROL_INCOMPLETE' available]
			)
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_hold'...], [ac_cv_ast_control_hold], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_hold = (int)AST_CONTROL_HOLD;
				], [CS_AST_CONTROL_HOLD], ['AST_CONTROL_HOLD' available]
			)
						
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_connected_line'...], [ac_cv_ast_control_connected_line], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_connected_line = (int)AST_CONTROL_CONNECTED_LINE;
				], [CS_AST_CONTROL_CONNECTED_LINE], ['AST_CONTROL_CONNECTED_LINE' available]
			)
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_format_siren7'...], [ac_cv_ast_format_siren7], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_format_siren7 = (int)AST_FORMAT_SIREN7;
				], [CS_AST_FORMAT_SIREN7], [SIREN7 frmt available]
			)
			
			if test x${ac_cv_ast_format_siren7} = xyes; then
			        AC_DEFINE([CS_CODEC_G722_1_24K],[1],[Found 'AST_FORMAT_SIREN7' in asterisk/frame.h])
		        fi

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_srcchange'...], [ac_cv_ast_control_srcchange], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_srcchange = (int)AST_CONTROL_SRCCHANGE;
				], [CS_AST_CONTROL_SRCCHANGE], ['AST_CONTROL_SRCCHANGE' available]
			)

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_redirecting'...], [ac_cv_ast_control_redirecting], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_redirecting = (int)AST_CONTROL_REDIRECTING;
				], [CS_AST_CONTROL_REDIRECTING], ['AST_CONTROL_REDIRECTING' available]
			)

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_transfer'...], [ac_cv_ast_control_transfer], [
		               	$HEADER_INCLUDE
				#include <asterisk/frame.h>
				], [
					int test_control_transfer = (int)AST_CONTROL_TRANSFER;
				], [CS_AST_CONTROL_TRANSFER], ['AST_CONTROL_TRANSFER' available]
			)
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/logger.h],
    		[
    			AC_DEFINE(HAVE_PBX_LOGGER_H,1,[Found 'asterisk/logger.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/manager.h],	
    		[
    			AC_DEFINE(HAVE_PBX_MANAGER_H,1,[Found 'asterisk/manager.h'])
    		],,[
	               	$HEADER_INCLUDE
			#ifdef HAVE_PBX_STRINGFIELDS_H
			#include <asterisk/stringfields.h>
			#endif
		]
    )
    AC_CHECK_HEADER([asterisk/module.h],
		[
			AC_DEFINE(HAVE_PBX_MODULE_H,1,[Found 'asterisk/module.h'])

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_module_ref'...], [ac_cv_ast_module_ref], [
		               	$HEADER_INCLUDE
				#include <asterisk/module.h>
				],[
					struct ast_module *test_module_ref = ast_module_ref(NULL);
				],[CS_AST_MODULE_REF],['ast_module_ref' available]
			)
		],,[ 
	               	$HEADER_INCLUDE
		]
    )
    AC_CHECK_HEADER([asterisk/musiconhold.h],
    		[
    			AC_DEFINE(HAVE_PBX_MUSICONHOLD_H,1,[Found 'asterisk/musiconhold.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/options.h],
    		[
    			AC_DEFINE(HAVE_PBX_OPTIONS_H,1,[Found 'asterisk/options.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/pbx.h],
		[
			AC_DEFINE(HAVE_PBX_PBX_H,1,[Found 'asterisk/pbx.h'])
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_get_hint'...], [ac_cv_ast_get_hint], [
		               	$HEADER_INCLUDE
				#ifdef HAVE_PBX_CHANNEL_H
				#include <asterisk/channel.h>
				#endif
				#include <asterisk/pbx.h>
				],[
					int test_get_hint = ast_get_hint("", 0, "", 0, NULL, "", "");
				],[CS_AST_HAS_NEW_HINT],['ast_get_hint' available]
			)
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_extension_onhold'...], [ac_cv_ast_extension_onhold], [
		               	$HEADER_INCLUDE
				#include <asterisk/pbx.h>
				], [
					int test_ext_onhold = (int)AST_EXTENSION_ONHOLD;
				], [CS_AST_HAS_EXTENSION_ONHOLD],['AST_EXTENSION_ONHOLD' available]
			)
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_extension_ringing'...], [ac_cv_ast_extension_ringing], [
		               	$HEADER_INCLUDE
				#include <asterisk/pbx.h>
				], [
					int test_ext_ringing = (int)AST_EXTENSION_RINGING;
				], [CS_AST_HAS_EXTENSION_RINGING],['AST_EXTENSION_RINGING' available]
			)
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/rtp_engine.h],
		[
			AC_DEFINE(HAVE_PBX_RTP_H,1,[Found 'asterisk/rtp_engine.h'])
			AC_DEFINE(HAVE_PBX_RTP_ENGINE_H,1,[Found 'asterisk/rtp_engine.h'])
			AC_DEFINE(PBX_RTP_TYPE,[struct ast_rtp_instance],[Defined PBX_RTP_TYPE as 'struct ast_rtp_instance'])
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_rtp_instance_new'...], [ac_cv_ast_rtp_instance_new], [
			               	$HEADER_INCLUDE
					#include <asterisk/rtp_engine.h>
				], [
					struct sched_context *test_sched=NULL;
					const struct ast_sockaddr *test_sin=NULL;
					struct ast_rtp_instance *test_instance = NULL;
					test_instance = ast_rtp_instance_new(NULL, test_sched, &test_sin, NULL);
				], [CS_AST_RTP_INSTANCE_NEW],[Found 'void ast_rtp_instance_new' in asterisk/rtp_engine.h]
			)
		],
		[
                	AC_CHECK_HEADER([asterisk/rtp.h],
                                [
                                        AC_DEFINE(HAVE_PBX_RTP_H,1,[Found 'asterisk/rtp.h'])
					AC_DEFINE(PBX_RTP_TYPE,[struct ast_rtp],[Defined PBX_RTP_TYPE as 'struct ast_rtp'])
                                        CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_rtp_new_source'...], [ac_cv_ast_rtp_new_source], [
					               	$HEADER_INCLUDE
       		                                        #include <asterisk/rtp.h>
                                                ], [
                                                        struct ast_rtp *test_rtp=NULL;
                                                        ast_rtp_new_source(test_rtp);
                                                ], [CS_AST_RTP_NEW_SOURCE],['ast_rtp_new_source' available]
                                        )

                                        CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_rtp_change_source'...], [ac_cv_ast_rtp_change_source], [
					               	$HEADER_INCLUDE
               		                                #include <asterisk/rtp.h>
               	                                ], [
                                                        struct ast_rtp *test_rtp=NULL;
                                                        ast_rtp_change_source(test_rtp);
                                                ], [CS_AST_RTP_CHANGE_SOURCE],['ast_rtp_change_source' available]
                                        )
                                ],,[ 
                                #if ASTERISK_VERSION_NUMBER >= 10400
                                #include <asterisk.h>
                                #endif
                	])
		],[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/sched.h],
		[
			AC_DEFINE(HAVE_PBX_SCHED_H,1,[Found 'asterisk/sched.h'])
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_sched_del'...], [ac_cv_ast_sched_del], [
					#include <unistd.h>				
			               	$HEADER_INCLUDE
					#ifdef HAVE_PBX_OPTIONS_H
					#include <asterisk/options.h>
					#endif
					#ifdef HAVE_PBX_LOGGER_H
					#include <asterisk/logger.h>
					#endif
					#include <asterisk/sched.h>
				],[
					struct sched_context *test_con;
					int test_id = 0;
					int test_sched_del = AST_SCHED_DEL(test_con, test_id);
				],[CS_AST_SCHED_DEL],['AST_SCHED_DEL' available]
			)
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/strings.h],
		[
 			AC_DEFINE([CS_AST_HAS_STRINGS],1,[Found 'asterisk/strings.h'])
    			AC_DEFINE([HAVE_PBX_STRINGS_H],1,[Found 'asterisk/strings.h'])
			dnl Checking for ast_copy_string
			AC_MSG_CHECKING([ - availability 'ast_copy_string'...])
			AC_TRY_COMPILE([
		               	$HEADER_INCLUDE
				#include <asterisk/strings.h>
				], [
					const char *test_src = "SRC";
					char *test_dst = "DST";
					ast_copy_string(test_dst, test_src, (size_t)3);
				], [
					AC_MSG_RESULT(yes)
					AC_DEFINE([sccp_copy_string(x,y,z)],[ast_copy_string(x,y,z)],['ast_copy_string' available])
				], [
					AC_MSG_RESULT(no)
					AC_DEFINE([sccp_copy_string(x,y,z)],[strncpy(x,y,z - 1)],['ast_copy_string' replacement])			
				])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/stringfields.h],	
    		[
    			AC_DEFINE(HAVE_PBX_STRINGFIELDS_H,1,[Found 'asterisk/stringfields.h'])
			AC_DEFINE(CS_AST_HAS_AST_STRING_FIELD,1,[Found 'ast_string_field_' in asterisk])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/cel.h],
    		[
    			AC_DEFINE(HAVE_PBX_CEL_H,1,[Found 'asterisk/stringfields.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/message.h],
    		[
    			AC_DEFINE(HAVE_PBX_MESSAGE_H,1,[Found 'asterisk/message.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/translate.h],
    		[
    			AC_DEFINE(HAVE_PBX_TRANSLATE_H,1,[Found 'asterisk/translate.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/utils.h],
		[
			AC_DEFINE(HAVE_PBX_UTILS_H,1,[Found 'asterisk/utils.h'])

			AC_MSG_CHECKING([ - availability 'ast_free'...])
			AC_TRY_COMPILE([
		               	$HEADER_INCLUDE
				#include <asterisk/utils.h>
				], [
					ast_free(NULL);
				], [
				
				AC_MSG_RESULT(yes)
			], [
				AC_DEFINE([ast_free],[free],['ast_free' replacement])
				AC_MSG_RESULT(no)
			])

			AC_MSG_CHECKING([ - availability 'ast_random'...])
			AC_TRY_COMPILE([
		               	$HEADER_INCLUDE
				#include <asterisk/utils.h>
				], [
					unsigned int test_random = ast_random();
				], [
				
				AC_MSG_RESULT(yes)
			], [
				AC_DEFINE([ast_random],[random],['ast_random' replacement])
				AC_MSG_RESULT(no)
			])		
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/astobj.h],
    		[
    			AC_DEFINE(HAVE_PBX_ASTOBJ2_H,1,[Found 'asterisk/astobj.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/astobj2.h],
    		[
    			AC_DEFINE(HAVE_PBX_ASTOBJ2_H,1,[Found 'asterisk/astobj2.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    
    if test "${ASTERISK_VER_GROUP}" == "1.8"; then
	if test "${ac_cv_have_variable_fdset}x" = "0x"; then
		AC_RUN_IFELSE(
			[AC_LANG_PROGRAM([
			#include <unistd.h>
			#include <sys/types.h>
			#include <stdlib.h>
			], 
			[
			if (getuid() != 0) { exit(1); }
			])],
				AC_DEFINE([CONFIGURE_RAN_AS_ROOT], 1, [Some configure tests will unexpectedly fail if configure is run by a non-root user.  These may be able to be tested at runtime.]))
			fi

	AC_MSG_CHECKING(if we can increase the maximum select-able file descriptor)
	AC_RUN_IFELSE(
		[AC_LANG_PROGRAM([
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
			])],
			dnl if ok then yes
			AC_MSG_RESULT(yes)
			AC_DEFINE([HAVE_VARIABLE_FDSET], 1, [Define to 1 if your system can support larger than default select bitmasks.]),
			dnl if nok else no
			AC_MSG_RESULT(no),
			dnl print cross-compile
			AC_MSG_RESULT(cross-compile)
	)

#	AC_CHECK_SIZEOF([int])
#	AC_CHECK_SIZEOF([long])
#	AC_CHECK_SIZEOF([long long])
#	AC_CHECK_SIZEOF([char *])
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
  ],
  [
  	echo "Couldn't find asterisk/lock.h. No need to go any further. All will fail."
  	echo "Asterisk version.h: ${pbx_ver/\"/}"
  	echo ""
  	echo "Exiting"
  	exit 255
  ]
  )
])

