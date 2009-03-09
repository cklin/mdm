# Time-stamp: <2009-03-09 00:35:55 cklin>

CC := $(shell which mdm-run > /dev/null && echo mdm-run) $(CC)
CFLAGS := -Wall -D_GNU_SOURCE -Iinclude

SED := /bin/sed
INSTALL := /usr/bin/install
LN := /bin/ln
GZIP := /bin/gzip

LIB := library/buffer.o library/comms.o library/socket.o
PROG := $(patsubst programs/%.c,%,$(wildcard programs/*.c))

PREFIX ?= /usr/local
BIN_DIR := $(PREFIX)/bin
LIB_DIR := $(PREFIX)/lib/mdm
MAN_DIR := $(PREFIX)/man/man1

all : $(PROG)

mdm-master : library/hazard.o
mdm-top : library/procfs.o
mdm-top : LDFLAGS=-lcurses

mdm-% : programs/mdm-%.c $(LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+

LIB += library/hazard.o library/procfs.o
$(LIB) : include/middleman.h
$(PROG) : include/middleman.h

install : install-bin install-docs

install-bin : all 
	$(INSTALL) -d $(BIN_DIR) $(LIB_DIR)
	$(INSTALL) scripts/mdm.screen $(BIN_DIR)
	$(INSTALL) -s mdm-run $(BIN_DIR)
	$(LN) -f -s mdm-run $(BIN_DIR)/mdm-sync
	$(INSTALL) -s mdm-master $(LIB_DIR)
	$(INSTALL) -s mdm-slave $(LIB_DIR)
	$(INSTALL) -s mdm-top $(LIB_DIR)
	$(SED) -i -e "s:MDM_LIB:$(LIB_DIR):" $(BIN_DIR)/mdm.screen

install-docs :
	$(INSTALL) -d $(MAN_DIR)
	$(INSTALL) -m 644 documents/mdm.screen.1 $(MAN_DIR)
	$(INSTALL) -m 644 documents/mdm-run.1 $(MAN_DIR)
	$(GZIP) -f -9 $(MAN_DIR)/mdm.screen.1
	$(GZIP) -f -9 $(MAN_DIR)/mdm-run.1
	$(LN) -f -s mdm-run.1.gz $(MAN_DIR)/mdm-sync.1.gz

clean :
	$(RM) library/*.o

dist-clean : clean
	$(RM) mdm-*

.PHONY : all install install-bin install-docs clean dist-clean
