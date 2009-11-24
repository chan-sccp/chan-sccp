# (SCCP*)
#
# An implementation of Skinny Client Control Protocol (SCCP)
#
# Sergio Chersovani (mlists@c-net.it)
#
# Reworked, but based on chan_sccp code.
# The original chan_sccp driver that was made by Zozo which 
# itself was derived from the chan_skinny driver.
# Modified by Jan Czmok and Julien Goodwin
#
# This program is free software and may be modified and
# distributed under the terms of the GNU Public License.

OSNAME=${shell uname}

INSTALL_PREFIX=
ASTERISK_HEADER_DIR=$(INSTALL_PREFIX)/usr/include

ifeq (${OSNAME},FreeBSD)
ASTERISK_HEADER_DIR=$(INSTALL_PREFIX)/usr/local/include
endif

ifeq (${OSNAME},NetBSD)
ASTERISK_HEADER_DIR=$(INSTALL_PREFIX)/usr/pkg/include
endif

AST_MODULES_DIR=$(INSTALL_PREFIX)/usr/lib/asterisk/modules/
# Location asterisk modules install to
ifeq (${OSNAME},SunOS)
AST_MODULES_DIR=$(INSTALL_PREFIX)/opt/asterisk/lib/modules/
ASTERISK_HEADER_DIR=$(INSTALL_PREFIX)/opt/asterisk/usr/include/asterisk
endif

#PROC=native
DEBUG=-ggdb #-include $(ASTERISK_HEADER_DIR)/asterisk/astmm.h

CFLAGS:=-O2 $(CFLAGS) -I$(ASTERISK_HEADER_DIR)
#CFLAGS+= -march=$(PROC)
CFLAGS+= -Iinclude -D_REENTRANT -D_GNU_SOURCE -DCRYPTO -fPIC -pipe -Wall 
#CFLAGS+= -Wextra 
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wlong-long
#CFLAGS+= -D'SCCP_BRANCH="v3 - shared lines"' -D'SCCP_VERSION="$(shell svnversion -n .)"' -D'BUILD_USER="$(shell whoami)"' -D'BUILD_DATE="$(shell date)"'
CFLAGS+= -D'SCCP_BRANCH="v3 - shared lines"' -D'SCCP_VERSION="$(shell svnversion -n .)"' -D'BUILD_USER="$(shell whoami)"' -D'BUILD_DATE="$(shell date)"'
CFLAGS+= $(DEBUG)

LINTOPTS=-linelen 1000
LINTOPTS+=+matchanyintegral
LINTOPTS+=-retvalother
LINTOPTS+=-type

FLS+=chan_sccp
FLS+=sccp_hint
FLS+=sccp_lock
FLS+=sccp_actions
FLS+=sccp_channel
FLS+=sccp_device
FLS+=sccp_line
FLS+=sccp_utils
FLS+=sccp_pbx
FLS+=sccp_cli
FLS+=sccp_softkeys
FLS+=sccp_socket
FLS+=sccp_indicate
FLS+=sccp_features
FLS+=sccp_adv_features
FLS+=sccp_config
FLS+=sccp_management
FLS+=sccp_mwi
FLS+=sccp_featureButton
FLS+=sccp_event


HEADERS=$(shell for i in $(FLS) ; do echo $$i.h ; done)
OBJECTS=$(shell for i in $(FLS) ; do echo .tmp/$$i.o ; done)
SOURCES=$(shell for i in $(FLS) ; do echo $$i.c ; done)
LINK_OPT=-shared -Xlinker -x
ifeq (${OSNAME},Darwin)
LINK_OPT=-dynamic -bundle -undefined suppress -force_flat_namespace
endif

chan_sccp.so: .config .tmp $(OBJECTS) $(HEADERS) $(SOURCES)
	@printf "   [LD] chan_sccp.so\n"
	@$(CC) $(LINK_OPT) -o chan_sccp.so $(OBJECTS)

.tmp/%.o: $(HEADERS)
	@printf "   [CC] %-40s\t%5s lines\n" "$*.c -> $*.o" "`wc -l <$*.c`"
	@$(CC) $(CFLAGS) -c $*.c -o .tmp/$*.o

all: chan_sccp.so

install: chan_sccp.so
#	@echo
#	@echo "============================"
#	@echo "|                          |"
#	@echo "|       |          |       |"
#	@echo "|      :|:        :|:      |"
#	@echo "|     :|||:      :|||:     |"
#	@echo "|  .:|||||||:..:|||||||:.  |"
#	@echo "|       CHAN_SCCP_v3       |"
#	@echo "============================"	
	@echo "Installing: chan_sccp.so"
	@install -m 755 chan_sccp.so $(AST_MODULES_DIR);
	@if ! [ -f $(INSTALL_PREFIX)/etc/asterisk/sccp.conf ]; then \
		echo "Installing: $(INSTALL_PREFIX)/etc/asterisk/sccp.conf"; \
		mkdir -p $(INSTALL_PREFIX)/etc/asterisk; \
		cp conf/sccp.conf $(INSTALL_PREFIX)/etc/asterisk/; \
	fi
	@echo "Done."
	@echo
	@echo "If not already done, please remember to disable"
	@echo "chan_skinny from asterisk modules adding the"
	@echo "following line to /etc/asterisk/modules.conf:"
	@echo
	@echo "noload => chan_skinny.so"
	@echo

clean:	
#	@rm -rf config.h chan_sccp.so .tmp
	@rm -rf chan_sccp.so .tmp
	@echo "Make clean successfully"

lint:
	@splint -weak -warnposix -varuse -fullinitblock -unrecog $(LINTOPTS) $(FLS) | tee splintlog

Makefile:

MEEP=chan_sccp.o
MOO=chan_sccp.c	

.config:
	@if ! [ -f ./config.h ]; then \
	sh ./create_config.sh "$(ASTERISK_HEADER_DIR)"; \
	fi
	
.tmp/chan_sccp.o:		chan_sccp.c
.tmp/sccp_event.o:		sccp_event.c
.tmp/sccp_lock.o:       sccp_lock.c
.tmp/sccp_actions.o:	sccp_actions.c
.tmp/sccp_channel.o:	sccp_channel.c
.tmp/sccp_device.o:		sccp_device.c
.tmp/sccp_utils.o:		sccp_utils.c
.tmp/sccp_pbx.o:		sccp_pbx.c
.tmp/sccp_cli.o:		sccp_cli.c
.tmp/sccp_line.o:		sccp_line.c
.tmp/sccp_softkeys.o:	sccp_softkeys.c
.tmp/sccp_socket.o:		sccp_socket.c
.tmp/sccp_indicate.o:	sccp_indicate.c
.tmp/sccp_features.o:	sccp_features.c
.tmp/sccp_adv_features.o: sccp_adv_features.c
.tmp/sccp_config.o: 	sccp_config.c
.tmp/sccp_featureButton.o: 	sccp_featureButton.c

.tmp:
	@mkdir -p .tmp

.PHONY: lint clean

