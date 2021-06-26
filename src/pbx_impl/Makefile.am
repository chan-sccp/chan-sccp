# FILE:			AutoMake Makefile for chan-sccp
# COPYRIGHT:		http://chan-sccp.github.io/chan-sccp/ group 2011
# CREATED BY:		Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: 		This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
# 			See the LICENSE file at the top of the source tree.
# NOTE:			Process this file with automake to produce a makefile.in script.

include 		$(top_srcdir)/src/Makefile.inc.am
#CC = G++
noinst_LTLIBRARIES	= libpbximpl.la
noinst_HEADERS	 	= pbx_impl.h
libpbximpl_la_SOURCES	= pbx_impl.c

libpbximpl_la_CXXFLAGS	= $(AM_CXXFLAGS) #-Wc++-compat
libpbximpl_la_LDFLAGS	= $(AM_LDFLAGS)
