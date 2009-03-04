# Time-stamp: <2009-03-04 14:33:15 cklin>

CC := $(shell which mdm-run > /dev/null && echo mdm-run) $(CC)
CFLAGS := -Wall -D_GNU_SOURCE -Iinclude

SED := /bin/sed
INSTALL := /usr/bin/install
LN := /bin/ln

LIB := $(patsubst %.c,%.o,$(wildcard library/*.c))
PROG := $(patsubst programs/%.c,%,$(wildcard programs/*.c))

PREFIX ?= /usr/local
BIN_DIR := $(PREFIX)/bin
LIB_DIR := $(PREFIX)/lib/mdm

all : $(PROG)

$(LIB) : include/middleman.h
mdm-top : LDFLAGS=-lcurses

mdm-% : programs/mdm-%.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

install : all
	$(INSTALL) -d $(BIN_DIR) $(LIB_DIR)
	$(INSTALL) scripts/mdm.screen $(BIN_DIR)
	$(INSTALL) -s mdm-run $(BIN_DIR)
	$(LN) -f -s mdm-run $(BIN_DIR)/mdm-sync
	$(INSTALL) -s mdm-master $(LIB_DIR)
	$(INSTALL) -s mdm-slave $(LIB_DIR)
	$(INSTALL) -s mdm-top $(LIB_DIR)
	$(SED) -i -e "s:MDM_LIB:$(LIB_DIR):" $(BIN_DIR)/mdm.screen

clean :
	$(RM) library/*.o

dist-clean : clean
	$(RM) mdm-*

.PHONY : all install clean dist-clean
