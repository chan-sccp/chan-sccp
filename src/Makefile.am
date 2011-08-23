# FILE: AutoMake Makefile for chan-sccp-b
# COPYRIGHT: chan-sccp-b.sourceforge.net group 2011
# CREATED BY: Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
#          See the LICENSE file at the top of the source tree.
# DATE:     $Date: 2011-01-22 17:57:33 +0100 (Sa, 22 Jan 2011) $
# REVISION: $Revision: 2278 $
# NOTE: Process this file with automake to produce a makefile.in script.

AUTOMAKE_OPTS		= gnu silent-rules

# Chan_SCCP.so
csmod_LTLIBRARIES	= chan_sccp.la

chan_sccp_la_SOURCES	=	chan_sccp.h		sccp_cli.h		sccp_event.h					\
                                sccp_labels.h		sccp_utils.h		sccp_adv_features.h	sccp_channel.h  	\
                                sccp_softkeys.h		sccp_conference.h	sccp_featureButton.h	sccp_line.h		\
                                sccp_actions.h		sccp_config.h		sccp_features.h		sccp_pbx.h		\
                                pbx_impl/pbx_impl.h	sccp_device.h		sccp_hint.h		sccp_lock.h		\
                                sccp_protocol.h 	sccp_dllists.h		sccp_indicate.h		sccp_management.h	\
                                sccp_socket.h   	sccp_mwi.h		common.h		sccp_rtp.h 		\
                                sccp_appfunctions.h

chan_sccp_la_SOURCES	+=	chan_sccp.c		sccp_channel.c		sccp_device.c		sccp_indicate.c 	\
                                sccp_pbx.c 		pbx_impl/pbx_impl.c	sccp_softkeys.c		sccp_featureButton.c	\
                                sccp_line.c		sccp_utils.c		sccp_actions.c		sccp_cli.c		\
                                sccp_features.c 	sccp_socket.c		sccp_config.c		sccp_hint.c		\
                                sccp_lock.c		sccp_event.c		sccp_adv_features.c	sccp_management.c	\
                                sccp_mwi.c		sccp_conference.c	sccp_rtp.c		sccp_appfunctions.c	\
                                sccp_protocol.c
                                
if BUILD_AST			# New conditional build using EXTRA_ and _DEPENDENCIES 
EXTRA_chan_sccp_la_SOURCES = pbx_impl/ast/ast.h pbx_impl/ast/ast.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast102.h pbx_impl/ast/ast102.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast1020x.h pbx_impl/ast/ast1020x.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast104.h pbx_impl/ast/ast104.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast1040x.h pbx_impl/ast/ast1040x.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast106.h pbx_impl/ast/ast106.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast1060x.h pbx_impl/ast/ast1060x.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast108.h pbx_impl/ast/ast108.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast1080x.h pbx_impl/ast/ast1080x.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast110.h pbx_impl/ast/ast110.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/ast1100x.h pbx_impl/ast/ast1100x.c
EXTRA_chan_sccp_la_SOURCES += pbx_impl/ast/astTrunk.h pbx_impl/ast/astTrunk.c
endif

chan_sccp_la_CPPFLAGS	= $(PBX_CPPFLAGS)
chan_sccp_la_LIBADD	= $(PTHREAD_CFLAGS) $(GCLIBS) $(PBX_GENERAL) $(PBX_MAJOR) $(PBX_MINOR)
chan_sccp_la_DEPENDENCIES = $(PBX_GENERAL) $(PBX_MAJOR) $(PBX_MINOR)
chan_sccp_la_CFLAGS	= $(AM_CFLAGS) $(PBX_CFLAGS) $(GDB_FLAGS) $(PTHREAD_CFLAGS) $(GCLIBS)
chan_sccp_la_CFLAGS	+= -D_REENTRANT -D_GNU_SOURCE -DCRYPTO -fPIC -pipe -Wall $(GCFLAGS)
chan_sccp_la_LDFLAGS	= -avoid-version -module  $(PBX_LDFLAGS) $(PTHREAD_LIBS) $(GCLIBS)

install-csmodLTLIBRARIES:
	@$(NORMAL_INSTALL)
	@test -z "$(DESTDIR)$(csmoddir)" || $(mkdir_p) "$(DESTDIR)$(csmoddir)"
	@if test "x$(strip_binaries)"=="xyes" ; then \
	        $(INSTALL) $(INSTALL_STRIP_FLAG) .libs/chan_sccp.so "$(DESTDIR)$(csmoddir)"; \
	else \
	        $(INSTALL) .libs/chan_sccp.so "$(DESTDIR)$(csmoddir)"; \
	fi
	@if test -d "$(DESTDIR)$(csmoddir)"; then                                          \
		echo    "=============================================================="; \
		echo    "Installed chan_sccp.so to '$(DESTDIR)$(csmoddir)'...";           \
		echo    "=============================================================="; \
		echo    "Please restart asterisk to use the new version of chan_sccp.so"; \
		echo    "=============================================================="; \
	fi
