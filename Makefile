# Time-stamp: <2009-02-07 21:17:51 cklin>

CFLAGS = -Wall -D_GNU_SOURCE
OBJS = comms.o buffer.o socket.o

all:		mdm-slave mdm-master mdm-run
mdm-slave:	mdm-slave.c $(OBJS)
mdm-master:	mdm-master.c $(OBJS)
mdm-run:	mdm-run.c $(OBJS)
.PHONY:		all
