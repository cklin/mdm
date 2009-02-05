# Time-stamp: <2009-02-05 01:17:09 cklin>

CFLAGS = -Wall -D_GNU_SOURCE

all:		mdm-worker mdm-core mdm-run
mdm-worker:	mdm-worker.c comms.o
mdm-core:	mdm-core.c comms.o
mdm-run:	mdm-run.c comms.o
.PHONY:		all
