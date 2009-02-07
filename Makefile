# Time-stamp: <2009-02-07 08:57:05 cklin>

CFLAGS = -Wall -D_GNU_SOURCE

all:		mdm-slave mdm-master mdm-run
mdm-slave:	mdm-slave.c comms.o buffer.o socket.o
mdm-master:	mdm-master.c comms.o buffer.o socket.o
mdm-run:	mdm-run.c comms.o buffer.o socket.o
.PHONY:		all
