# Copyright (C) 2009 Diederik de Groot <dkgroot@talon.nl>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# Date:     $Date$
# Revision: $Revision$

AUTOMAKE_OPTS		= gnu silent-rules

# Chan_SCCP.so
csmod_LTLIBRARIES	= chan_sccp.la
chan_sccp_la_SOURCES    = chan_sccp.h          sccp_cli.h          sccp_event.h          sccp_labels.h      sccp_mwi.h           \
			  sccp_softkeys.h      sccp_conference.h   sccp_featureButton.h  sccp_line.h        sccp_utils.h	 \
			  sccp_actions.h       sccp_config.h       sccp_features.h       sccp_lists.h       sccp_pbx.h           \
			  sccp_adv_features.h  sccp_device.h       sccp_hint.h           sccp_lock.h        sccp_protocol.h      \
			  sccp_channel.h       sccp_dllists.h      sccp_indicate.h       sccp_management.h  sccp_socket.h        \
			  													 \
			  chan_sccp.c          sccp_channel.c      sccp_device.c         sccp_indicate.c    sccp_pbx.c           \
			  sccp_utils.c         sccp_actions.c      sccp_cli.c            sccp_features.c    sccp_line.c          \
			  sccp_socket.c        sccp_config.c       sccp_hint.c           sccp_lock.c        sccp_softkeys.c      \
			  sccp_event.c         sccp_adv_features.c sccp_mwi.c            sccp_management.c  sccp_featureButton.c \
			  sccp_conference.cpp  #sccp_object2.c

chan_sccp_la_CFLAGS	= $(AM_CFLAGS)
chan_sccp_la_CFLAGS	+= -D_REENTRANT -D_GNU_SOURCE -DCRYPTO -fPIC -pipe -Wall
chan_sccp_la_CFLAGS	+= -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wlong-long
chan_sccp_la_LDFLAGS	= -avoid-version -module

install-csmodLTLIBRARIES:
	@$(NORMAL_INSTALL)
	@if [ -d "$(DESTDIR)$(csmoddir)" ]; then $(MKDIR_P) "$(DESTDIR)$(csmoddir)"; fi
	@$(install_sh_PROGRAM) .libs/chan_sccp.so $(DESTDIR)$(csmoddir)/chan_sccp.so		 
	@if [ -d "$(DESTDIR)$(csmoddir)" ]; then                                                         \
		echo    "=============================================================="; \
		echo    "Installed chan_sccp.so to '$(DESTDIR)$(csmoddir)'...";           \
		echo    "=============================================================="; \
		echo    "Please restart asterisk to use the new version of chan_sccp.so"; \
		echo    "=============================================================="; \
	fi
