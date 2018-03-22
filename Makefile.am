# FILE:			AutoMake Makefile for chan-sccp
# COPYRIGHT:		chan-sccp-b.sourceforge.net group 2009
# CREATED BY:		Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE:		This program is free software and may be modified and distributed under the terms of the GNU Public License.
# 			See the LICENSE file at the top of the source tree.
# NOTE:			Process this file with automake to produce a makefile.in script.
include 		$(top_srcdir)/amdoxygen.am

AUTOMAKE_OPTIONS	= gnu
EXTRA_DIST 		= .version tools conf contrib/gen_sccpconf .revision
SUBDIRS 		= src doc
ACLOCAL_AMFLAGS		= -I autoconf
AM_CPPFLAGS		= $(LTDLINCL)

distdir 		= $(PACKAGE)-$(SCCP_VERSION)_$(SCCP_BRANCH)_r$(SCCP_REVISION)
	
# Update libtool if necessary	
libtool: 		@LIBTOOL_DEPS@
			cd $(srcdir) && \
			$(SHELL) ./config.status --recheck
	
FORCE:

# Update ChangeLog file from SVN Log
ChangeLog:		FORCE
	@$(GIT) log v4.2.0...HEAD --pretty=medium >ChangeLog

# Update ChangeLog Files and Generate Doxygen Documentation
doxygen: ChangeLog doxygen-doc

deb: 
	dpkg-buildpackage -rfakeroot -us -uc

rpm:    release
if ENABLE_RPMBUILD
	@if [ ! -d ~/rpmbuild/BUILD ]; then mkdir -p ~/rpmbuild/BUILD;fi
	@if [ ! -d ~/rpmbuild/SOURCES ]; then mkdir -p ~/rpmbuild/SOURCES;fi
	@if [ ! -d ~/rpmbuild/SPECS ]; then mkdir -p ~/rpmbuild/SPECS;fi
	@if [ ! -d ~/rpmbuild/RPMS ]; then mkdir -p ~/rpmbuild/RPMS;fi
	@if [ ! -d ~/rpmbuild/SRPMS ]; then mkdir -p ~/rpmbuild/SRPMS;fi
	@cp $(distdir).tar.gz ~/rpmbuild/SOURCES/;
	@cp tools/chan-sccp.spec ~/rpmbuild/SOURCES/;
	@if [ -f /etc/SuSE-release ]; then \
		export ASTERISKVER="`echo $(ASTERISK_VER)|sed 's/\.//g'`"; \
	fi;
	@export RPMVERSION=$(SCCP_VERSION);
	@export RPMBRANCH=$(SCCP_BRANCH);
	@export RPMREVISION=$(SCCP_REVISION);
	@cd /usr/src/packages/SOURCES/ && SOURCEDIR="Chan_SCCP-4.3.0_TRUNK_r6452" rpmbuild -bb chan-sccp.spec --define="_usr ~/rpmbuild/";
else
	@echo ""; \
	echo "RPM Build on your platform not (yet) supported !"; \
	echo "If you like you could send us the required package build file for your platform"; \
	echo "";
endif

release:
	@rm -f ChangeLog
	@$(MAKE) ChangeLog
	@if [ "`grep -i $(SCCP_VERSION)_$(SCCP_BRANCH) NEWS`" == "" ]; then \
	  echo >>NEWS;\
	  echo "Version: $(SCCP_VERSION)_$(SCCP_BRANCH)" >> NEWS; \
	  echo "Please edit the NEWS file now..."; \
	  /usr/bin/joe NEWS; \
	  /usr/bin/debchange; \
	  tools/autorevision -o .revision -t m4 2>&1 >/dev/null; \
	else \
	  echo "Could not find $(SCCP_VERSION)_$(SCCP_BRANCH) in NEWS file"; \
	  exit; \
	fi 
	@$(MAKE) dist-gzip

gen_sccpconf:
	cd contrib/gen_sccpconf && $(MAKE) $(AM_MAKEFLAGS) all

gen_sccpconf_install:
	cd contrib/gen_sccpconf && $(MAKE) $(AM_MAKEFLAGS) install

isntall: install

rmcoverage:
	[ -d coverage ] && rm -rf coverage;

coverage: src/.libs/chan_sccp.so
	mkdir coverage; \
	lcov --directory src/.libs -b src -c --o coverage/chan-sccp.info ; \
	genhtml -o coverage -f -t chan-sccp coverage/chan-sccp.info
	
coverage-zero: rmcoverage
	lcov --directory src/.libs -b src -z;

test:
	@echo "Success"

install-data-local: chan_sccp-en_US.xml
	@test -d $(DESTDIR)$(PBX_ETC) || $(mkinstalldirs) $(DESTDIR)$(PBX_ETC);
	@if [ ! -f $(DESTDIR)$(PBX_ETC)/sccp.conf ]; then										\
		echo "Installing $(DESTDIR)$(PBX_ETC)/sccp.conf...";									\
		$(INSTALL) -m 644 $(top_srcdir)/conf/sccp.conf $(DESTDIR)$(PBX_ETC);							\
	fi;
	@echo "Installing chan_sccp-en_US.xml to $(DESTDIR)$(PBX_DATADIR)/documentation/thirdparty/..."					
	@test -d $(DESTDIR)$(PBX_DATADIR)/documentation/thirdparty || $(mkinstalldirs) $(DESTDIR)$(PBX_DATADIR)/documentation/thirdparty;
	@$(INSTALL) -m 644 chan_sccp-en_US.xml "$(DESTDIR)$(PBX_DATADIR)/documentation/thirdparty";

XML_chan_sccp_en_US=$(top_srcdir)/src/sccp_appfunctions.c
chan_sccp-en_US.xml: $(top_builddir)/src/stamp-h1 $(XML_chan_sccp_en_US)
	@printf "Building Documentation..."
	@echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > $@
	@echo "<!DOCTYPE docs SYSTEM \"appdocsxml.dtd\">" >> $@
	@echo "<?xml-stylesheet type=\"text/xsl\" href=\"appdocsxml.xslt\"?>" >> $@
	@echo "<docs xmlns:xi=\"http://www.w3.org/2001/XInclude\">" >> $@
	@for x in $(XML_chan_sccp_en_US);do 												\
		$(AWK) -f $(top_srcdir)/tools/get_documentation $$x >> $@;									\
	done;
	@echo
	@echo "</docs>" >> $@

unload:	
	cd src;make unload
	
load:	
	cd src;make load
	
reload:
	cd src;make reload

#install-exec-hook:
#	@if test "$(DEBUG)" == "1" && test -n "$(GDB)" && test tools/chan-sccp_backtrace -nt /usr/bin/chan-sccp_backtrace; then	\
#		$(INSTALL) -C tools/chan-sccp_backtrace /usr/bin/chan-sccp_backtrace;						\
#	fi
