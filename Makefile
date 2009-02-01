# Time-stamp: <2009-01-31 22:06:25 cklin>

CFLAGS = -Wall -D_GNU_SOURCE

all:		mdm-worker mdm-core mdm-run
mdm-worker:	mdm-worker.c comms.o
mdm-core:	mdm-core.c comms.o exec.o
mdm-run:	mdm-run.c comms.o exec.o
.PHONY:		all
