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
			for x in "1.2" "1.4" "1.6" "1.8" "1.10" "10" "11" "12"; do
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
                                                        112) AC_DEFINE([ASTERISK_CONF_1_12], [1], [Defined ASTERISK_CONF_1_12]);;
                                                        113) AC_DEFINE([ASTERISK_CONF_1_13], [1], [Defined ASTERISK_CONF_1_13])
	                                                        ASTERISK_INCOMPATIBLE=yes;;
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
                                                        echo "This version of chan-sccp-b only has support for Asterisk 1.12.x and below."
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
                                AC_MSG_RESULT([found])
                                AC_CHECK_HEADER([asterisk/format_cache.h],
                                [
					ASTERISK_VER_GROUP=113
					ASTERISK_VERSION_NUMBER=11300
					ASTERISK_REPOS_LOCATION=TRUNK

					AC_DEFINE([ASTERISK_CONF_1_13], [1], [Defined ASTERISK_CONF_1_13])
					AC_DEFINE([ASTERISK_VERSION_NUMBER], [11300], [ASTERISK Version Number])
					AC_DEFINE([ASTERISK_VERSION_GROUP], [113], [ASTERISK Version Group])
					AC_DEFINE([ASTERISK_REPOS_LOCATION], ["TRUNK"],[ASTERISK Source Location])
					
					version_found=1
					AC_MSG_RESULT([done])
					dnl AC_MSG_RESULT([WARNING: Found 'Asterisk Version ${ASTERISK_VERSION_NUMBER}'. Experimental at the moment. Anything might break.])
					AC_MSG_RESULT([WARNING: Found 'Asterisk Version ${ASTERISK_VERSION_NUMBER}'.])
                                ],
				[
					AC_CHECK_HEADER(
						[asterisk/uuid.h],
						[
							ASTERISK_VER_GROUP=112
							ASTERISK_VERSION_NUMBER=11200
							ASTERISK_REPOS_LOCATION=TRUNK

							AC_DEFINE([ASTERISK_CONF_1_12], [1], [Defined ASTERISK_CONF_1_12])
							AC_DEFINE([ASTERISK_VERSION_NUMBER], [11200], [ASTERISK Version Number])
							AC_DEFINE([ASTERISK_VERSION_GROUP], [112], [ASTERISK Version Group])
							AC_DEFINE([ASTERISK_REPOS_LOCATION], ["TRUNK"],[ASTERISK Source Location])
							
							version_found=1
							AC_MSG_RESULT([done])
						],[
							ASTERISK_VER_GROUP=111
							ASTERISK_VERSION_NUMBER=11100
							ASTERISK_REPOS_LOCATION=BRANCH

							AC_DEFINE([ASTERISK_CONF_1_11], [1], [Defined ASTERISK_CONF_1_11])
							AC_DEFINE([ASTERISK_VERSION_NUMBER], [11100], [ASTERISK Version Number])
							AC_DEFINE([ASTERISK_VERSION_GROUP], [111], [ASTERISK Version Group])
							AC_DEFINE([ASTERISK_REPOS_LOCATION], ["BRANCH"],[ASTERISK Source Location])
							
							version_found=1
							AC_MSG_RESULT([done])
							AC_MSG_RESULT([Found 'Asterisk Version 11'])
						]
					)
                                ])
			],[
                                AC_MSG_RESULT([not found])
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

  CFLAGS_backup={$CFLAGS}
  CFLAGS="${CFLAGS_saved}"
  
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
                        AC_MSG_CHECKING([ - if asterisk was compiled with the 'LOW_MEMORY' buildoption...])                              
                        AC_EGREP_HEADER([yes], [
				#include <asterisk/buildopts.h>
				#ifdef LOW_MEMORY
					yes
				#endif
			],[
                                AC_DEFINE([CS_LOW_MEMORY],1,[asterisk compiled with 'LOW_MEMORY'])
                                AC_MSG_RESULT(yes)
                        ],[
                                AC_MSG_RESULT(no)
                        ])             

			AC_MSG_CHECKING([ - if asterisk was compiled with the 'TEST_FRAMEWORK' buildoption...])
                        AC_EGREP_HEADER([yes], [
	                        #include <asterisk/buildopts.h>
       	        		#ifdef TEST_FRAMEWORK
       	                 		yes
       		                 #endif
       		        ], [
                                AC_DEFINE([CS_TEST_FRAMEWORK],1,[asterisk compiled with 'TEST_FRAMEWORK'])
                                TEST_FRAMEWORK=yes
                                AC_MSG_RESULT(yes)
                        ],[
                                AC_MSG_RESULT(no)
                                TEST_FRAMEWORK=no
                        ])
                        AC_SUBST([TEST_FRAMEWORK])
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

			AC_MSG_CHECKING([ - availability 'ast_channel_bridge_peer' in asterisk/channel.h...])
			AC_EGREP_HEADER([ast_channel_bridge_peer], [asterisk/channel.h],
			[
                                AC_DEFINE([CS_AST_HAS_CHANNEL_BRIDGE_PEER],1,['ast_channel_bridge_peer' available])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])

			AC_MSG_CHECKING([ - availability 'ast_channel_get_bridge_channel' in asterisk/channel.h...])
			AC_EGREP_HEADER([ast_channel_get_bridge_channel], [asterisk/channel.h],
			[
                                AC_DEFINE([CS_AST_HAS_CHANNEL_GET_BRIDGE_CHANNEL], 1 ,['ast_channel_get_bridge_channel' available])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])
			
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

			CS_CV_TRY_COMPILE_IFELSE([ - availability 'ast_namedgroups'...], [ac_cv_ast_namedgroups], [
				$HEADER_INCLUDE
					#include <asterisk/channel.h>
				], [
					struct ast_namedgroup *test = ast_get_namedgroups("test");
				],
				[AC_DEFINE([CS_AST_HAS_NAMEDGROUP],1,[Found 'ast_namedgroups' in asterisk/channel.h])],
			)
		],,[
			$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/bridge.h],
    		[
			AC_MSG_CHECKING([ - availability 'ast_bridge_base_new' in asterisk/bridge.h...])
			AC_EGREP_HEADER([ast_bridge_base_new], [asterisk/bridge.h],
			[
				AC_DEFINE(CS_BRIDGE_BASE_NEW,1,[Found 'ast_bridge_base_new' in asterisk/bridge.h])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])
			AC_MSG_CHECKING([ - availability 'AST_BRIDGE_JOIN_PASS_REFERENCE' in ast_bridge_join...])
			AC_EGREP_HEADER([AST_BRIDGE_JOIN_PASS_REFERENCE], [asterisk/bridge.h],
			[
				AC_DEFINE(CS_BRIDGE_JOIN_PASSREFERENCE,1,[Found 'AST_BRIDGE_JOIN_PASS_REFERENCE' in definition of ast_bridge_join' in asterisk/bridge.h])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])
			CS_CV_TRY_COMPILE_IFELSE([ - ast_bridge_depart with only one parameter...], [ac_cv_ast_bridge_depart], [
		               	$HEADER_INCLUDE
                                #include <asterisk/channel.h>
                                #include <asterisk/linkedlists.h>
                                #include <asterisk/astobj2.h>
				#include <asterisk/bridge.h>
				], [
				        struct ast_channel *chan = {0};
					struct ast_bridge *test_bridge = ast_bridge_depart(chan);
				], 
				[AC_DEFINE([CS_BRIDGE_DEPART_ONLY_CHANNEL],1,['ast_bridge_depart' only needs channel reference in asterisk/bridge.h])],
			)
		],[
		    AC_CHECK_HEADER([asterisk/bridging.h],
				[
					AC_CHECK_HEADER([asterisk/bridging_roles.h], [
						AC_DEFINE(HAVE_PBX_BRIDGING_ROLES_H,1,[Found 'asterisk/bridging_roles.h'])
						],,[
						$HEADER_INCLUDE
						#include <asterisk/channel.h>
						#include <asterisk/linkedlists.h>
						#include <asterisk/astobj2.h>
						#include <asterisk/bridging.h>
					])
					AC_MSG_CHECKING([ - availability 'ast_bridge_base_new' in asterisk/bridging.h...])
					AC_EGREP_HEADER([ast_bridge_base_new], [asterisk/bridging.h],
					[
						AC_DEFINE(CS_BRIDGE_BASE_NEW,1,[Found 'ast_bridge_base_new' in asterisk/bridging.h])
						AC_MSG_RESULT(yes)
					],[
						AC_MSG_RESULT(no)
					])
					AC_MSG_CHECKING([ - availability 'AST_BRIDGE_CAPABILITY_MULTITHREADED' in asterisk/bridging.h...])
					AC_EGREP_HEADER([AST_BRIDGE_CAPABILITY_MULTITHREADED], [asterisk/bridging.h],
					[
						AC_DEFINE(CS_BRIDGE_CAPABILITY_MULTITHREADED,1,[Found 'AST_BRIDGE_CAPABILITY_MULTITHREADED' in asterisk/bridging.h])
						AC_MSG_RESULT(yes)
					],[
						AC_MSG_RESULT(no)
					])
					AC_MSG_CHECKING([ - availability 'pass_reference' in ast_bridge_join...])
					AC_EGREP_HEADER([int pass_reference], [asterisk/bridging.h],
					[
						AC_DEFINE(CS_BRIDGE_JOIN_PASSREFERENCE,1,[Found 'pass_reference' in definition of ast_bridge_join' in asterisk/bridging.h])
						AC_MSG_RESULT(yes)
					],[
						AC_MSG_RESULT(no)
					])

					CS_CV_TRY_COMPILE_IFELSE([ - ast_bridge_depart with only one parameter...], [ac_cv_ast_bridge_depart], [
						$HEADER_INCLUDE
						#include <asterisk/channel.h>
						#include <asterisk/linkedlists.h>
						#include <asterisk/astobj2.h>
						#include <asterisk/bridging.h>
						], [
							struct ast_channel *chan = {0};
							struct ast_bridge *test_bridge = ast_bridge_depart(chan);
						], 
						[AC_DEFINE([CS_BRIDGE_DEPART_ONLY_CHANNEL],1,['ast_bridge_depart' only needs channel reference in asterisk/bridging.h])],
					)
				],,[ 
					$HEADER_INCLUDE
					#include <asterisk/channel.h>
					#include <asterisk/linkedlists.h>
					#include <asterisk/astobj2.h>
		    ])
		],[
			$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/channel_pvt.h],
    		[
    			AC_DEFINE(HAVE_PBX_CHANNEL_pvt_H,1,[Found 'asterisk/channel_pvt.h'])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_DEFINE(PBX_VARIABLE_TYPE,[struct ast_variable],[Defined PBX_VARIABLE as 'struct ast_variable'])
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
    AC_CHECK_HEADER([asterisk/stasis.h],	
    		[
    			AC_DEFINE(HAVE_PBX_STASIS_H,1,[Found 'asterisk/stasis.h'])
    			CS_CV_TRY_COMPILE_DEFINE([ - availability 'stasis_subscribe'...], [ac_cv_ast_stasis_subscribe], [
			               	$HEADER_INCLUDE
					#include <asterisk/stasis.h>
				], [
                                        struct stasis_topic *stasis_topic;
					void *data;
                                        struct stasis_subscription *stasis_sub = stasis_subscribe(stasis_topic, data, data);
				], [CS_AST_HAS_STASIS],['stasis_subscribe' available]
			)
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/parking.h],
    [
		AC_DEFINE(HAVE_PBX_FEATURES_H,1,[Found 'asterisk/parking.h'])
    ],,[ 
              	$HEADER_INCLUDE
    ])
    ast_pickup_h=0
    AC_CHECK_HEADER([asterisk/pickup.h],
    [
		AC_DEFINE(HAVE_PBX_FEATURES_H,1,[Found 'asterisk/pickup.h'])
		AC_DEFINE(CS_AST_DO_PICKUP,1,[Found 'ast_do_pickup' in asterisk/pickup.h])
		ast_pickup_h=1
    ],,[ 
              	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/features.h],
    [
   			AC_DEFINE(HAVE_PBX_FEATURES_H,1,[Found 'asterisk/features.h'])
   			
			AS_IF([test "${ast_pickup_h}" == 0], [
				AC_MSG_CHECKING([ - availability 'ast_do_pickup'...])
				AC_EGREP_HEADER([ast_do_pickup], [asterisk/features.h],
				[
					AC_DEFINE(CS_AST_DO_PICKUP,1,[Found 'ast_do_pickup' in asterisk/features.h])
					AC_MSG_RESULT(yes)
				],[
					AC_MSG_RESULT(no)
				])
			]);
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

			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_incomplete'...], [ac_cv_ast_control_incomplete], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>
				], [
					int test_control_incomplete = (int)AST_CONTROL_INCOMPLETE;
				], [CS_AST_CONTROL_INCOMPLETE], ['AST_CONTROL_INCOMPLETE' available]
			)
			
			CS_CV_TRY_COMPILE_DEFINE([ - availability 'ast_control_redirecting'...], [ac_cv_ast_control_redirecting], [
			               	$HEADER_INCLUDE
					#include <asterisk/frame.h>p
				], [
					int test_control_redirecting = (int)AST_CONTROL_REDIRECTING;
				], [CS_AST_CONTROL_REDIRECTING], ['AST_CONTROL_REDIRECTING' available]
			)

		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/manager.h],	
    		[
    			AC_DEFINE(HAVE_PBX_MANAGER_H,1,[Found 'asterisk/manager.h'])
    			CS_CV_TRY_COMPILE_DEFINE([ - availability 'manager_custom_hook' is available...], [ac_cv_manager_custom_hook], [
				$HEADER_INCLUDE
        			#include <asterisk/stringfields.h>
				#include <asterisk/manager.h>
				], [
					struct manager_custom_hook sccp_manager_hook;
				], [HAVE_PBX_MANAGER_HOOK_H], ['struct manager_custom_hook' available]
			)
    		],,[
	               	$HEADER_INCLUDE
			#include <asterisk/stringfields.h>
		]
    )
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
			AC_MSG_CHECKING([ - availability 'ast_rtp_instance_bridge'...])
			AC_EGREP_HEADER([ast_rtp_instance_bridge], [asterisk/rtp_engine.h],
			[
				AC_DEFINE(CS_AST_RTP_INSTANCE_BRIDGE,1,[Found 'ast_rtp_instance_bridge' in asterisk/bridging.h])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])
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
   			AC_DEFINE([HAVE_PBX_STRINGS_H],1,[Found 'asterisk/strings.h'])
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

			AC_MSG_CHECKING([ - availability 'ast_cel_linkedid_ref'...])
			AC_EGREP_HEADER([ast_cel_linkedid_ref], [asterisk/cel.h],
			[
				AC_DEFINE(CS_AST_CEL_LINKEDID_REF,1,[Found 'ast_cel_linkedid_ref' in asterisk/cel.h])
				AC_MSG_RESULT(yes)
			],[
				AC_MSG_RESULT(no)
			])
		],,[ 
	               	$HEADER_INCLUDE
    ])
    AC_CHECK_HEADER([asterisk/message.h],
    		[
    			AC_DEFINE(HAVE_PBX_MESSAGE_H,1,[Found 'asterisk/message.h'])
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
    AC_CHECK_HEADER([asterisk/http.h],
    		[
    			AC_DEFINE(HAVE_PBX_HTTP_H,1,[Found 'asterisk/http.h'])
    			HAVE_PBX_HTTP=yes
    			AC_SUBST([HAVE_PBX_HTTP])
		],,[ 
	               	$HEADER_INCLUDE
    ])
  CFLAGS={$CFLAGS_backup}
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

