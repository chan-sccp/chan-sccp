# FILE: AutoMake Makefile for chan-sccp-b
# COPYRIGHT: chan-sccp-b.sourceforge.net group 2011
# CREATED BY: Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
#          See the LICENSE file at the top of the source tree.
# DATE:     $Date$
# REVISION: $Revision$
# NOTE: Process this file with automake to produce a makefile.in script.
AUTOMAKE_OPTS			= gnu silent-rules subdir-objects
csmod_LTLIBRARIES		= chan_sccp.la 
MOSTLYCLEANFILES 		= *.gcda *.gcno *.gcov revision.h

# revision.h
nodist_chan_sccp_la_SOURCES 	= .revision
CLEANFILES			= .revision
#.PHONY: 			.revision sccp_enum
.PHONY: 			.revision gen_enum

EXTRA_DIST = sccp_config_entries.hh chan_sccp_enums.hh sccp_protocol_enums.hh sccp_event_enums.hh sccp_enum_macro.h

chan_sccp_la_SOURCES	=	sccp_enum.h		chan_sccp.h		sccp_cli.h		sccp_threadpool.h	\
				sccp_event.h		sccp_labels.h		sccp_utils.h		sccp_adv_features.h	\
				sccp_channel.h  	sccp_softkeys.h		sccp_conference.h	sccp_featureButton.h	\
				sccp_line.h		sccp_actions.h		sccp_config.h		sccp_features.h		\
				sccp_pbx.h		pbx_impl/pbx_impl.h	sccp_device.h		sccp_hint.h 		\
				sccp_lock.h		sccp_refcount.h		sccp_protocol.h 	sccp_dllists.h		\
				sccp_indicate.h		sccp_management.h	sccp_mwi.h		common.h		\
				sccp_rtp.h 		sccp_appfunctions.h	sccp_socket.h		sccp_cli_table.h	\
				sccp_devstate.h		revision.h
                                                                
chan_sccp_la_SOURCES	+=	sccp_enum.c 		chan_sccp.c		sccp_channel.c		sccp_device.c		\
				sccp_indicate.c 	sccp_pbx.c 		pbx_impl/pbx_impl.c	sccp_softkeys.c		\
				sccp_featureButton.c	sccp_line.c		sccp_utils.c		sccp_actions.c		\
				sccp_cli.c		sccp_features.c 	sccp_config.c		sccp_hint.c 		\
                                sccp_refcount.c		sccp_event.c		sccp_adv_features.c	sccp_management.c	\
                                sccp_mwi.c		sccp_conference.c	sccp_rtp.c		sccp_appfunctions.c	\
                                sccp_protocol.c		sccp_socket.c		sccp_threadpool.c	sccp_devstate.c

#if CS_EXPERIMENTAL_XML
#chan_sccp_la_SOURCES	+=      sccp_webservice.h       sccp_xml.h 		sccp_xml_embedded.h	sccp_webservice.c	\
#                                sccp_xml.c
#endif 
                                
EXTRA_chan_sccp_la_SOURCES = 
if BUILD_AST
  EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/define.h
  EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast.h      pbx_impl/ast/ast.c
  EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast106.h   pbx_impl/ast/ast106.c
  EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast108.h   pbx_impl/ast/ast108.c
  EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast110.h   pbx_impl/ast/ast110.c
  EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast111.h   pbx_impl/ast/ast111.c
  EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast112.h   pbx_impl/ast/ast112.c pbx_impl/ast/ast112_announce.h pbx_impl/ast/ast112_announce.c
  EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast113.h   pbx_impl/ast/ast113.c pbx_impl/ast/ast112_announce.h pbx_impl/ast/ast112_announce.c
endif

chan_sccp_la_LIBADD	  = $(PTHREAD_CFLAGS) $(PBX_GENERAL) $(PBX_MAJOR) $(PBX_MINOR) $(CLANG_BLOCKS_LIBS)
chan_sccp_la_DEPENDENCIES = $(PBX_GENERAL) $(PBX_MAJOR) $(PBX_MINOR) revision.h
chan_sccp_la_CFLAGS       = $(AM_CFLAGS) $(PBX_CFLAGS) $(GDB_FLAGS) $(PTHREAD_CFLAGS) $(COVERAGE_CFLAGS) $(EVENT_CFLAGS) $(LIBEXSLT_CFLAGS) $(LIBCURL_CFLAGS) 
chan_sccp_la_CFLAGS      += -D_REENTRANT -D_GNU_SOURCE -DCRYPTO -fPIC -pipe -Wall 
chan_sccp_la_LDFLAGS      = -avoid-version -module $(PBX_LDFLAGS) $(PTHREAD_LIBS) $(EVENT_LIBS) $(COVERAGE_CFLAGS) $(COVERAGE_LDFLAGS) $(LIBEXSLT_LIBS) $(LIBCURL_LIBS) $(EVENT_LIBS) $(CLANG_BLOCKS_LIBS)

chan_sccp_la.$(OBJEXT): revision.h .revision

revision.h: Makefile .revision
	@cd "@abs_top_srcdir@/src"

.revision: Makefile
	@cd "@abs_top_srcdir@/src";																				\
	if test -f @abs_builddir@/revision.h; then																		\
		if test ! -z "`find @abs_builddir@/revision.h -mmin +60`"; then															\
			if test "`@abs_top_srcdir@/tools/autorevision -t h -o @abs_top_srcdir@/.revision |grep VCS_SHORT_HASH`" != "`grep VCS_SHORT_HASH @abs_builddir@/revision.h`"; then 	\
				@abs_top_srcdir@/tools/autorevision -t h -f -o @abs_top_srcdir@/.revision  >"@abs_builddir@/revision.h" 2>/dev/null;						\
			fi 																					\
		fi 																						\
	else 																							\
		@abs_top_srcdir@/tools/autorevision -t h -f -o @abs_top_srcdir@/.revision  >"@abs_builddir@/revision.h" 2>/dev/null; 								\
	fi

sccp_enum.c: sccp_enum.in @abs_top_srcdir@/tools/gen_sccp_enum.awk
	@test "sccp_enum.in" -nt $@ || $(MAKE) gen_enum
	@test "@abs_top_srcdir@/tools/gen_sccp_enum.awk" -nt $@ || $(MAKE) gen_enum

sccp_enun.h: sccp_enum.in @abs_top_srcdir@/tools/gen_sccp_enum.awk
	@test "sccp_enum.in" -nt $@ || $(MAKE) gen_enum
	@test "@abs_top_srcdir@/tools/gen_sccp_enum.awk" -nt $@ || $(MAKE) gen_enum

gen_enum: sccp_enum.in @abs_top_srcdir@/tools/gen_sccp_enum.awk
	@echo "  GEN      sccp_enum.h"
	@echo "  GEN      sccp_enum.c"
	@$(AWK) -v out_header_file="@abs_builddir@/src/sccp_enum.h" -v out_source_file="@abs_builddir@/src/sccp_enum.c" -f "@abs_top_srcdir@/tools/gen_sccp_enum.awk" @abs_top_srcdir@/src/sccp_enum.in >/dev/null

install-csmodLTLIBRARIES:
	@$(NORMAL_INSTALL)
	@test -z "$(DESTDIR)$(csmoddir)" || $(MKDIR_P) "$(DESTDIR)$(csmoddir)"							
	@if test "x$(strip_binaries)"=="xyes" ; then 										\
	        $(INSTALL) $(INSTALL_STRIP_FLAG) .libs/chan_sccp.so "$(DESTDIR)$(csmoddir)"; 					\
	else 															\
	        $(INSTALL) .libs/chan_sccp.so "$(DESTDIR)$(csmoddir)"; 								\
	fi
	@if test -d "$(DESTDIR)$(csmoddir)"; then                                          					\
		echo    "==============================================================================================="; 	\
		echo    "Installed chan_sccp.so to '$(DESTDIR)$(csmoddir)'...";           					\
		echo    "===============================================================================================";	\
		echo    "Please restart asterisk to use the new version of chan_sccp.so"; 					\
		echo    "==============================================================================================="; 	\
	fi
