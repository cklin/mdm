# Time-stamp: <2009-02-05 01:27:39 cklin>

CFLAGS = -Wall -D_GNU_SOURCE

all:		mdm-worker mdm-core mdm-run
mdm-worker:	mdm-worker.c comms.o buffer.o
mdm-core:	mdm-core.c comms.o buffer.o
mdm-run:	mdm-run.c comms.o
.PHONY:		all
