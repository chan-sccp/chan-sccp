# FILE:			AutoMake Makefile for chan-sccp
# COPYRIGHT:		chan-sccp-b.sourceforge.net group 2011
# CREATED BY:		Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: 		This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
# 			See the LICENSE file at the top of the source tree.
# NOTE:			Process this file with automake to produce a makefile.in script.

include 		$(top_srcdir)/src/Makefile.inc.am

noinst_LTLIBRARIES	= libast.la
noinst_HEADERS		= define.h ast.h include_asterisk_autoconfig.h

libast_la_SOURCES	= ast.c
libast_la_CFLAGS	= $(AM_CFLAGS)
libast_la_LDFLAGS	= $(AM_LDFLAGS)
