# Time-stamp: <2009-01-06 19:24:46 cklin>

CFLAGS = -Wall -D_GNU_SOURCE

all:		mdm-worker mdm-dispatcher mdm-liason
mdm-worker:	mdm-worker.c comms.o cmdline.o
mdm-dispatcher:	mdm-dispatcher.c comms.o cmdline.o
mdm-liason:	mdm-liason.c comms.o cmdline.o
.PHONY:		all
