# FILE:			AutoMake Makefile for chan-sccp
# COPYRIGHT:		chan-sccp-b.sourceforge.net group 2011
# CREATED BY:		Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: 		This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
# 			See the LICENSE file at the top of the source tree.
# NOTE:			Process this file with automake to produce a makefile.in script.

include 		$(top_srcdir)/src/Makefile.inc.am

SUBDIRS			= pbx_impl $(PBX_COND_SUBDIR) $(PBXVER_COND_SUBDIR) $(PBXVER_COND_ANNOUNCE_SUBDIR) .
DIST_SUBDIRS		= pbx_impl pbx_impl/ast pbx_impl/ast106 pbx_impl/ast108 pbx_impl/ast110 pbx_impl/ast111 pbx_impl/ast112 pbx_impl/ast113 pbx_impl/ast114 pbx_impl/ast115 pbx_impl/ast_announce .
EXTRA_DIST 		= sccp_enum.in sccp_config_entries.hh
BUILT_SOURCES           = revision.h sccp_enum.h sccp_enum.c
CLEANFILES              = revision.h sccp_enum.h sccp_enum.c stamp-g1
.PHONY:			force
# generated files
revision.gen:
	@echo "  REV      revision.h";
	@@abs_top_srcdir@/tools/autorevision -t h -o @abs_top_srcdir@/.revision >@abs_top_builddir@/src/revision.h 2>/dev/null;

revision.force: force
	@if	[ ! -f @abs_top_builddir@/src/revision.h ] ||																	\
	 	[ "x$(GIT)" != "xNo" -a @abs_top_builddir@/src/revision.h -ot @abs_top_srcdir@/.git/COMMIT_EDITMSG -a @abs_top_builddir@/src/revision.h -ot @abs_top_srcdir@/.git/HEAD ] ||	\
		[ "x$(GIT)" == "xNo" -a @abs_top_builddir@/src/revision.h -ot @abs_top_builddir@/.revision ];											\
	then																							\
		$(MAKE) revision.gen;																				\
	fi	

revision.h: revision.force

sccp_enum.h sccp_enum.c:sccp_enum.in @abs_top_srcdir@/tools/gen_sccp_enum.awk
	@if [ sccp_enum.in -nt $@ ] || [ @abs_top_srcdir@/tools/gen_sccp_enum.awk -nt $@ ]; then							\
		$(MAKE) gen_enum;															\
	fi

gen_enum:		sccp_enum.in @abs_top_srcdir@/tools/gen_sccp_enum.awk
			@echo "  GEN      sccp_enum.h";
			@echo "  GEN      sccp_enum.c";
			@$(AWK) -f "@abs_top_srcdir@/tools/gen_sccp_enum.awk" < @abs_top_srcdir@/src/sccp_enum.in >/dev/null;

#@$(AWK) -O -v out_header_file=sccp_enum.h -v out_source_file=sccp_enum.c -f "@abs_top_srcdir@/tools/gen_sccp_enum.awk" < @abs_top_srcdir@/src/sccp_enum.in >/dev/null;

# libsccp.la and chan_sccp.so
noinst_LTLIBRARIES	= libsccp.la
csmod_LTLIBRARIES	= chan_sccp.la

noinst_HEADERS		= chan_sccp.h			sccp_actions.h			sccp_cli.h			sccp_devstate.h			\
			  sccp_featureButton.h		sccp_line.h			sccp_linedevice.h		sccp_refcount.h			\
			  common.h			sccp_appfunctions.h		sccp_cli_table.h		sccp_dllists.h			\
			  sccp_feature.h		sccp_management.h		sccp_rtp.h			sccp_vector.h			\
			  sccp_atomic.h			sccp_conference.h		sccp_threadpool.h		sccp_debug.h			\
			  sccp_hint.h			sccp_mwi.h			sccp_session.h	 		sccp_callinfo.h			\
			  sccp_config.h			sccp_indicate.h			sccp_pbx.h			sccp_softkeys.h 		\
			  revision.h			sccp_channel.h			sccp_device.h			sccp_event.h			\
			  sccp_labels.h			sccp_protocol.h			sccp_enum.h			sccp_codec.h			\
			  define.h			sccp_netsock.h			sccp_xml.h			sccp_webservice.h		\
			  sccp_utils.h			sccp_featureParkingLot.h	sccp_transport.h

libsccp_la_SOURCES	= sccp_callinfo.c 		sccp_channel.c			sccp_device.c			sccp_debug.c			\
			  sccp_indicate.c 		sccp_pbx.c 			sccp_session.c			sccp_threadpool.c		\
			  sccp_softkeys.c		sccp_featureButton.c		sccp_line.c			sccp_linedevice.c		\
			  sccp_actions.c		sccp_cli.c			sccp_feature.c 			sccp_config.c			\
			  sccp_hint.c 			sccp_refcount.c			sccp_management.c		sccp_mwi.c			\
			  sccp_conference.c		sccp_rtp.c			sccp_appfunctions.c		sccp_protocol.c			\
			  sccp_devstate.c		sccp_event.c			sccp_enum.c			sccp_globals.c			\
			  sccp_netsock.c		sccp_codec.c			sccp_labels.c			sccp_xml.c			\
			  sccp_webservice.c 		sccp_utils.c			sccp_featureParkingLot.c	sccp_transport_tcp.c	sccp_transport_tls.c

chan_sccp_la_SOURCES	= chan_sccp.c

libsccp_la_DEPENDENCIES = revision.h sccp_enum.h sccp_enum.c
libsccp_la_CFLAGS       = $(AM_CFLAGS)
libsccp_la_LDFLAGS	= $(AM_LDFLAGS)
libsccp_la_CXXFLAGS	= $(AM_CXXFLAGS)

chan_sccp_la_LIBADD	=  libsccp.la pbx_impl/libpbximpl.la $(PBX_COND_LIBADD) $(PBXVER_COND_LIBADD) $(PBXVER_COND_ANNOUNCE_LIBADD)
chan_sccp_la_DEPENDENCIES = $(chan_sccp_la_LIBADD)
chan_sccp_la_CFLAGS     = $(AM_CFLAGS)
chan_sccp_la_LDFLAGS	= $(LIBS) $(AM_LDFLAGS) $(PBX_LDFLAGS) $(PTHREAD_LIBS) $(EVENT_LIBS) $(LIBEXSLT_LIBS) $(LIBCURL_LIBS) $(EVENT_LIBS) $(LIBBFD) $(LIBEXECINFO) $(LTLIBICONV)
chan_sccp_la_LDFLAGS	+= -avoid-version -module -lm -s -rdynamic
chan_sccp_la_CXXFLAGS	= $(AM_CXXFLAGS)
install-csmodLTLIBRARIES:
	@$(NORMAL_INSTALL)
	@test -d "$(DESTDIR)$(csmoddir)" || $(MKDIR_P) "$(DESTDIR)$(csmoddir)"
	@if test "x$(strip_binaries)" == "xyes" ; then 											\
		test -d "$(DESTDIR)$(PBX_DEBUGMODDIR)" || $(MKDIR_P) "$(DESTDIR)$(PBX_DEBUGMODDIR)";					\
		$(OBJCOPY) --only-keep-debug ".libs/chan_sccp.so" "$(DESTDIR)$(PBX_DEBUGMODDIR)/chan_sccp.so";				\
		$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(INSTALL) -s ".libs/chan_sccp.so" "$(DESTDIR)$(csmoddir)";\
		$(OBJCOPY) --add-gnu-debuglink="$(DESTDIR)$(PBX_DEBUGMODDIR)/chan_sccp.so" "$(DESTDIR)$(csmoddir)/chan_sccp.so";	\
		echo    "==============================================================================================="; 		\
		echo    "Installed Stripped chan_sccp.so to '$(DESTDIR)$(csmoddir)'...";           					\
		echo    "===============================================================================================";		\
	else 																\
		$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(INSTALL) $(INSTALL_STRIP_FLAG) ".libs/chan_sccp.so" "$(DESTDIR)$(csmoddir)"; \
 		echo    "==============================================================================================="; 		\
		echo    "Installed chan_sccp.so to '$(DESTDIR)$(csmoddir)'...";           						\
		echo    "===============================================================================================";		\
	fi
	@if test -d "$(DESTDIR)$(csmoddir)"; then                                          						\
		echo    "Please restart asterisk or reload the module to use the new version of chan_sccp.so"; 				\
		echo    "==============================================================================================="; 		\
	fi

install-data-local:
	@test -d $(DESTDIR)$(PBX_ETC) || $(mkinstalldirs) $(DESTDIR)$(PBX_ETC);								\
	if [ ! -f $(DESTDIR)$(PBX_ETC)/sccp.conf ]; then										\
		echo "Installing $(DESTDIR)$(PBX_ETC)/sccp.conf...";									\
		$(INSTALL) -m 644 $(top_srcdir)/conf/sccp.conf $(DESTDIR)$(PBX_ETC);							\
	fi

unload:
	@if [ -f $(PBX_SBINDIR)/asterisk ] && [ 0`pidof -s $(PBX_SBINDIR)/asterisk` -gt 0 ]; then 					\
		echo "Unloading asterisk module";											\
		$(PBX_SBINDIR)/asterisk -rx "module unload chan_sccp.so";								\
	fi
	
load: install
	@if [ -f $(PBX_SBINDIR)/asterisk ] && [ 0`pidof -s $(PBX_SBINDIR)/asterisk` -gt 0 ]; then 					\
		echo "Loading asterisk module";												\
		$(PBX_SBINDIR)/asterisk -rx "module load chan_sccp.so";									\
	fi

reload: all
	@if [ -f $(PBX_SBINDIR)/asterisk ] && [ 0`pidof -s $(PBX_SBINDIR)/asterisk` -gt 0 ]; then					\
		if [ -n "`$(PBX_SBINDIR)/asterisk -rx 'module show like chan_sccp.so' | grep 'Running'`" ]; then			\
			$(MAKE) unload;													\
		fi;															\
		for counter in 1 2 3 4 5; do												\
			if [ -z "`$(PBX_SBINDIR)/asterisk -rx 'module show like chan_sccp.so' | grep 'Running'`" ]; then		\
				break;													\
			fi;														\
			echo -n "${counter}...";											\
			sleep 1;													\
		done;															\
		$(MAKE) load;														\
	fi

full-en_US.xml:
	@if [ -z "$(PYTHON)" ]; then													\
		echo "--------------------------------------------------------------------------";					\
		echo "---        Please install python to build full documentation           ---";					\
		echo "--------------------------------------------------------------------------";					\
		exit 1;															\
	fi
	@printf "Building Documentation..."
	@echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > $@
	@echo "<!DOCTYPE docs SYSTEM \"appdocsxml.dtd\">" >> $@
	@echo "<?xml-stylesheet type=\"text/xsl\" href=\"appdocsxml.xslt\"?>" >> $@
	@echo "<docs xmlns:xi=\"http://www.w3.org/2001/XInclude\">" >> $@
	@for x in `find -name '*.c'`;do													\
		$(PYTHON) ../tools/get_documentation.py < $$x >> $@ ;									\
	done
	@echo
	@echo "</docs>" >> $@
#	#@$(PYTHON) build_tools/post_process_documentation.py -i $@ -o "doc/core-en_US.xml"
