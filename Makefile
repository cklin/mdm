# Time-stamp: <2009-01-09 21:00:51 cklin>

CFLAGS = -Wall -D_GNU_SOURCE

all:		mdm-worker mdm-core mdm-run
mdm-worker:	mdm-worker.c comms.o cmdline.o
mdm-core:	mdm-core.c comms.o cmdline.o
mdm-run:	mdm-run.c comms.o cmdline.o
.PHONY:		all
