# Copyright (C) 2009 Diederik de Groot <dkgroot@talon.nl>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

include $(top_srcdir)/amdoxygen.am
EXTRA_DIST 			= bootstrap.sh configure
SUBDIRS 			= src doc
ACLOCAL_AMFLAGS                 = -I m4

FORCE:

# Update ChangeLog file from SVN Log
ChangeLog:	FORCE
if ENABLE_SVN2CL
	@echo "Generating ChangeLog from SVN $(CHANGELOG_OLDEST)->HEAD"
	@$(SVN2CL) --include-actions -i -a --group-by-day --separate-daylogs --include-rev --ignore-message-starting='#' --revision HEAD:$(CHANGELOG_OLDEST)
endif

# Update doc/html/ChangeLog.html from SVN Log
ChangeLog-HTML:	FORCE
if ENABLE_SVN2CL
	@echo "Generating doc/html/ChangeLog.html from SVN $(CHANGELOG_OLDEST)->HEAD"
	@mkdir doc/html
	@$(SVN2CL) --include-actions -i -a --group-by-day --title "Chan_SCCP-b ChangeLog" --separate-daylogs --include-rev --ignore-message-starting='#' --revision HEAD:$(CHANGELOG_OLDEST) --html -o doc/html/ChangeLog_SVN2CL.html --revision-link 'http://chan-sccp-b.svn.sourceforge.net/viewvc/chan-sccp-b?view=rev&revision='
	@cp build_tools/svn2html.css doc/html/
endif

# Update ChangeLog Files and Generate Doxygen Documentation
doxygen: ChangeLog ChangeLog-HTML doxygen-doc
