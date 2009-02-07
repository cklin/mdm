# Time-stamp: <2009-02-06 23:44:19 cklin>

CFLAGS = -Wall -D_GNU_SOURCE

all:		mdm-slave mdm-master mdm-run
mdm-slave:	mdm-slave.c comms.o buffer.o
mdm-master:	mdm-master.c comms.o buffer.o
mdm-run:	mdm-run.c comms.o buffer.o
.PHONY:		all
