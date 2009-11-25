# Copyright (C) 2009 Diederik de Groot <dkgroot@talon.nl>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AUTOMAKE_OPTS			= gnu dist-bzip2 dist-tarZ silent-rules
ACLOCAL_AMFLAGS			= -I m4
#DEFS += -include $(top_builddir)/include/global.h


csmod_LTLIBRARIES		= chan_sccp.la
chan_sccp_la_SOURCES		= chan_sccp.c          sccp_channel.c  sccp_device.c    sccp_indicate.c  sccp_pbx.c         sccp_utils.c \
				  sccp_actions.c       sccp_cli.c      sccp_features.c  sccp_line.c      sccp_socket.c \
				  sccp_adv_features.c  sccp_config.c   sccp_hint.c      sccp_lock.c      sccp_softkeys.c \
				  sccp_event.c		   sccp_management.c	sccp_mwi.c  sccp_featureButton.c

chan_sccp_la_CFLAGS		= $(AM_CFLAGS) #-I/usr/include -Iinclude
chan_sccp_la_CFLAGS             += -D_REENTRANT -D_GNU_SOURCE -DCRYPTO -fPIC -pipe -Wall
chan_sccp_la_CFLAGS             += -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wlong-long
chan_sccp_la_LDFLAGS            = -avoid-version -module

#chan_sccp_la_LIBADD		= @ASTERISK_LIB@
#chan_sccp_la_LDFLAGS            = -module -avoid-version @NO_UNDEFINED@

#INCLUDES = -I$(top_builddir)/include -I${top_srcdir}/corelib -I$(top_srcdir)/include
