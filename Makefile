# (SCCP*)
#
# An implementation of Skinny Client Control Protocol (SCCP)
#
# Sergio Chersovani (mlists@c-net.it)
#
# Reworked, but based on chan_sccp code.
# The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
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

PROC?=athlon

DEBUG=-ggdb #-include $(ASTERISK_HEADER_DIR)/asterisk/astmm.h

CFLAGS+=-Iinclude -D_REENTRANT -D_GNU_SOURCE -O -DCRYPTO -fPIC
CFLAG= -pipe -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
CFLAG+=$(shell if $(CC) -march=$(PROC) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo " -march=$(PROC)"; fi)
CFLAG+=$(shell if echo athlon | grep -q ppc; then echo " -fsigned-char"; fi)
#CFLAG+= -pedantic
#CFLAG+= -W
#CFLAG+= -Wpointer-arith
#CFLAG+= -Wcast-qual
#CFLAG+= -Wwrite-strings
#CFLAG+= -Wconversion
#CFLAG+= -Wredundant-decls # Useless as too many false positives from asterisk source
CFLAG+= -Wnested-externs 
CFLAG+= -Wlong-long
CFLAG+= $(DEBUG)

LINTOPTS=-linelen 1000
LINTOPTS+=+matchanyintegral
LINTOPTS+=-retvalother
LINTOPTS+=-type

FLS+=chan_sccp
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

CFLAGS+=$(CFLAG) -I$(ASTERISK_HEADER_DIR)
HEADERS=$(shell for i in $(FLS) ; do echo $$i.h ; done)
OBJECTS=$(shell for i in $(FLS) ; do echo .tmp/$$i.o ; done)
SOURCES=$(shell for i in $(FLS) ; do echo $$i.c ; done)
LINK_OPT=-shared -Xlinker -x
ifeq (${OSNAME},Darwin)
LINK_OPT=-dynamic -bundle -undefined suppress -force_flat_namespace
endif

chan_sccp.so: .config .tmp $(OBJECTS) $(HEADERS) $(SOURCES)
	@echo "Linking chan_sccp.so"
	@$(CC) $(LINK_OPT) -o chan_sccp.so $(OBJECTS)

.tmp/%.o: $(HEADERS)
	@printf "Now compiling .... %-15s\t%s lines \n" $*.c "`wc -l <$*.c`"
	@$(CC) $(CFLAGS) -c $*.c -o .tmp/$*.o

all: chan_sccp.so

install: chan_sccp.so
	@echo "Now Installing chan_sccp.so"
	@install -m 755 chan_sccp.so $(AST_MODULES_DIR);
	@if ! [ -f $(INSTALL_PREFIX)/etc/asterisk/sccp.conf ]; then \
		echo "Installing config file $(INSTALL_PREFIX)/etc/asterisk/sccp.conf"; \
		mkdir -p $(INSTALL_PREFIX)/etc/asterisk; \
		cp conf/sccp.conf $(INSTALL_PREFIX)/etc/asterisk/; \
	fi
	@echo "Chan_sccp is now installed"
	@echo "Remember to disable chan_skinny by adding the following"
	@echo "line to /etc/asterisk/modules.conf:"
	@echo "noload => chan_skinny.so"

clean:
	rm -rf config.h chan_sccp.so .tmp

lint:
	@splint -weak -warnposix -varuse -fullinitblock -unrecog $(LINTOPTS) $(FLS) | tee splintlog

Makefile:

MEEP=chan_sccp.o
MOO=chan_sccp.c	

.config:
	sh ./create_config.sh "$(ASTERISK_HEADER_DIR)"
	
.tmp/chan_sccp.o:		chan_sccp.c
.tmp/sccp_actions.o:	sccp_actions.c
.tmp/sccp_channel.o:	sccp_channel.c
.tmp/sccp_device.o:	sccp_device.c
.tmp/sccp_utils.o:	sccp_utils.c
.tmp/sccp_pbx.o:		sccp_pbx.c
.tmp/sccp_cli.o:		sccp_cli.c
.tmp/sccp_line.o:		sccp_line.c
.tmp/sccp_softkeys.o:	sccp_softkeys.c
.tmp/sccp_socket.o:	sccp_socket.c
.tmp/sccp_indicate.o:	sccp_indicate.c

.tmp:
	@mkdir -p .tmp

.PHONY: lint clean

