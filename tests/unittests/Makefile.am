# FILE:				AutoMake Makefile for chan-sccp-b
# COPYRIGHT:			chan-sccp-b.sourceforge.net group 2011
# CREATED BY:			Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: 			This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
# 				See the LICENSE file at the top of the source tree.
# NOTE:				Process this file with automake to produce a makefile.in script.

include 			$(top_srcdir)/src/Makefile.inc.am
noinst_LTLIBRARIES		= libsccp_unittests.la
#noinst_HEADERS	 		= sccp_test_rtp_ack.h
libsccp_unittests_la_SOURCES	= sccp_test_rtp_ack.c
libsccp_unittests_la_CFLAGS	= $(AM_CFLAGS) #-Wc++-compat
libsccp_unittests_la_CXXFLAGS	= $(AM_CFLAGS) #-Wc++-compat
libsccp_unittests_la_LDFLAGS	= $(AM_LDFLAGS)
