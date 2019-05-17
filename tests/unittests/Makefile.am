# FILE:				AutoMake Makefile for chan-sccp-b
# COPYRIGHT:			chan-sccp-b.sourceforge.net group 2011
# CREATED BY:			Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: 			This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
# 				See the LICENSE file at the top of the source tree.
# NOTE:				Process this file with automake to produce a makefile.in script.

include 			$(top_srcdir)/src/Makefile.inc.am

cstest_LTLIBRARIES	= chan_sccp_test.la
#noinst_HEADERS	 		= sccp_test_rtp_ack.h
chan_sccp_test_la_SOURCES	= sccp_test_rtp_ack.c

#chan_sccp_test_la_LIBADD	= $(top_builddir)/src/libsccp.la $(top_builddir)/src/pbx_impl/libpbximpl.la $(top_builddir)/src/$(PBX_COND_LIBADD) $(top_builddir)/src/$(PBXVER_COND_LIBADD) $(top_builddir)/src/$(PBXVER_COND_ANNOUNCE_LIBADD) $(top_builddir)/src/chan_sccp.la 
chan_sccp_test_la_LIBADD	= $(top_builddir)/src/libsccp.la $(top_builddir)/src/pbx_impl/libpbximpl.la $(top_builddir)/src/$(PBX_COND_LIBADD) $(top_builddir)/src/$(PBXVER_COND_LIBADD) $(top_builddir)/src/$(PBXVER_COND_ANNOUNCE_LIBADD) $(top_builddir)/src/.libs/chan_sccp.so
chan_sccp_test_la_DEPENDENCIES  = $(chan_sccp_test_la_LIBADD) $(chan_sccp_la_LIBADD)
chan_sccp_test_la_CFLAGS        = $(AM_CFLAGS)
chan_sccp_test_la_LDFLAGS	= $(AM_LDFLAGS) -Wl,--whole-archive $(PBX_LDFLAGS) $(PTHREAD_LIBS) $(EVENT_LIBS) $(LIBEXSLT_LIBS) $(LIBCURL_LIBS) $(EVENT_LIBS) $(LIBBFD) $(LIBEXECINFO) $(LTLIBICONV)
chan_sccp_test_la_LDFLAGS	+= -avoid-version -module -lm -s -rdynamic -Wl,--no-whole-archive
chan_sccp_test_la_CXXFLAGS	= $(AM_CXXFLAGS)

install-cstestLTLIBRARIES:
	@$(NORMAL_INSTALL)
	@test -d "$(DESTDIR)$(cstestdir)" || $(MKDIR_P) "$(DESTDIR)$(cstestdir)"
	@if test "x$(strip_binaries)" == "xyes" ; then 												\
		test -d "$(DESTDIR)$(PBX_DEBUGMODDIR)" || $(MKDIR_P) "$(DESTDIR)$(PBX_DEBUGMODDIR)";						\
		$(OBJCOPY) --only-keep-debug ".libs/chan_sccp_test.so" "$(DESTDIR)$(PBX_DEBUGMODDIR)/chan_sccp_test.so";			\
		$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(INSTALL) -s ".libs/chan_sccp_test.so" "$(DESTDIR)$(cstestdir)";	\
		$(OBJCOPY) --add-gnu-debuglink="$(DESTDIR)$(PBX_DEBUGMODDIR)/chan_sccp_test.so" "$(DESTDIR)$(cstestdir)/chan_sccp_test.so";	\
		echo    "==============================================================================================="; 			\
		echo    "Installed Stripped chan_sccp.so to '$(DESTDIR)$(cstestdir)'...";           						\
		echo    "===============================================================================================";			\
	else 																	\
		$(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(INSTALL) $(INSTALL_STRIP_FLAG) ".libs/chan_sccp_test.so" "$(DESTDIR)$(cstestdir)"; \
 		echo    "==============================================================================================="; 			\
		echo    "Installed chan_sccp.so to '$(DESTDIR)$(cstestdir)'...";           							\
		echo    "===============================================================================================";			\
	fi
	@if test -d "$(DESTDIR)$(cstestdir)"; then                                          							\
		echo    "Please restart asterisk or reload the module to use the new version of chan_sccp_test.so"; 				\
		echo    "==============================================================================================="; 			\
	fi
