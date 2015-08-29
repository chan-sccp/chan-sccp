# FILE: AutoMake Makefile for chan-sccp-b
# COPYRIGHT: chan-sccp-b.sourceforge.net group 2009
# CREATED BY: Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License.
#          See the LICENSE file at the top of the source tree.
# DATE:     $Date$
# REVISION: $Revision$
# NOTE: Process this file with automake to produce a makefile.in script.
include $(top_srcdir)/amdoxygen.am

#AUTOMAKE_OPTIONS		= gnu silent-rules
AUTOMAKE_OPTIONS		= gnu
EXTRA_DIST 			= .version tools conf contrib/gen_sccpconf .revision
SUBDIRS 			= src doc
ACLOCAL_AMFLAGS			= -I autoconf
AM_CPPFLAGS			= $(LTDLINCL)
MOSTLYCLEANFILES		= coverage

distdir = $(PACKAGE)-$(SCCP_VERSION)_$(SCCP_BRANCH)_r$(SCCP_REVISION)
	
# Update libtool if necessary	
libtool: @LIBTOOL_DEPS@
	cd $(srcdir) && \
	$(SHELL) ./config.status --recheck
	
FORCE:

# Update ChangeLog file from SVN Log
ChangeLog:	FORCE
if ENABLE_SVN2CL
	@if svnrev=`cat .revision |grep VCS_SHORT_HASH|awk -F= '{print "r" $2}'|tr -d '"'`; then \
	  if [ -z "`cat ChangeLog 2>/dev/null| grep "\[${svnrev}\]"`" ];then \
            echo "Generating ChangeLog from SVN $(CHANGELOG_OLDEST)->HEAD"; \
            $(SVN2CL) --include-actions -i -a --group-by-day --separate-daylogs \
            	--include-rev \
             	--revision-link='https://svn.code.sf.net/p/chan-sccp-b/code/trunk/' \
              	--ticket-link='http://sourceforge.net/tracker/?func=detail&group_id=186378&atid=917045&aid=' \
              	--ignore-message-starting='#' \
              	--revision HEAD:$(CHANGELOG_OLDEST); \
	  fi; \
	fi
else
	svn log -r$(CHANGELOG_OLDEST):HEAD > ChangeLog
endif

# Update doc/html/ChangeLog.html from SVN Log
ChangeLog-HTML:	FORCE
if ENABLE_SVN2CL
	@if svnrev=`cat .revision |grep VCS_SHORT_HASH|awk -F= '{print "r" $2}'|tr -d '"'`; then \
	  if [ -z "`cat doc/html/ChangeLog_SVN2CL.html 2>/dev/null| grep "\[r${svnrev}\]"`" ];then \
            echo "Generating doc/html/ChangeLog_SVN2CL.html from SVN $(CHANGELOG_OLDEST)->HEAD"; \
            if [ ! -d doc/html ]; then mkdir doc/html;fi; \
            $(SVN2CL) --include-actions -i -a --group-by-day --title "Chan_SCCP-b ChangeLog" --separate-daylogs --include-rev --ignore-message-starting='#' --revision HEAD:$(CHANGELOG_OLDEST) --html -o doc/html/ChangeLog_SVN2CL.html \
            --revision-link 'http://chan-sccp-b.svn.sourceforge.net/viewvc/chan-sccp-b?view=rev&revision=' \
            --ticket-link='http://sourceforge.net/tracker/?func=detail&group_id=186378&atid=917045&aid='; \
            cp autoconf/svn2html.css doc/html/; \
	  fi; \
	fi
endif

# Update ChangeLog Files and Generate Doxygen Documentation
doxygen: ChangeLog ChangeLog-HTML doxygen-doc

deb: 
	dpkg-buildpackage -rfakeroot -us -uc

rpm:    release
	@if test ! -z "$(RPMBUILD)" ; then \
	  cp $(distdir).tar.gz /usr/src/packages/SOURCES/; \
	  cp tools/chan-sccp-b.spec /usr/src/packages/SOURCES/; \
	  export DISTDIR=${distdir}; \
	  if [ -f /etc/SuSE-release ]; then \
                  if [ "$(ASTERISK_VER)" == "1.4" ]; then \
                          export ASTERISKVER=""; \
                  else \
                          export ASTERISKVER="`echo $(ASTERISK_VER)|sed 's/\.//g'`"; \
                  fi; \
	  fi; \
          export RPMVERSION="$(SCCP_VERSION)"; \
          export RPMBRANCH="$(SCCP_BRANCH)"; \
          export RPMREVISION="$(SCCP_REVISION"; \
          cd /usr/src/packages/SOURCES/ && rpmbuild -bb chan-sccp-b.spec; \
        else \
	  echo ""; \
	  echo "RPM Build on your platform not (yet) supported !"; \
	  echo "If you like you could send us the required package build file for your platform"; \
	  echo ""; \
	fi

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
	rm -rf coverage;

coverage:
	mkdir coverage; \
	lcov --directory src/.libs -b src -c --o coverage/chan-sccp-b.info; \
	genhtml -o coverage -f -t chan-sccp-b coverage/chan-sccp-b.info
	
coverage-zero: rmcoverage
	lcov --directory src/.libs -b src -z;

test:
	@echo "Success"
        
#install-exec-hook:
#	@if test "$(DEBUG)" == "1" && test -n "$(GDB)" && test tools/chan-sccp-b_backtrace -nt /usr/bin/chan-sccp-b_backtrace; then	\
#		$(INSTALL) -C tools/chan-sccp-b_backtrace /usr/bin/chan-sccp-b_backtrace;						\
#	fi
