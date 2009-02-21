# Time-stamp: <2009-02-20 21:19:33 cklin>

CC := $(shell which mdm-run > /dev/null && echo mdm-run) $(CC)
CFLAGS := -Wall -D_GNU_SOURCE
OBJS := comms.o buffer.o socket.o hazard.o

all:		mdm-slave mdm-master mdm-run mdm-top
mdm-slave:	mdm-slave.c $(OBJS)
mdm-master:	mdm-master.c $(OBJS)
mdm-run:	mdm-run.c $(OBJS)
mdm-top:	LDFLAGS = -lcurses
mdm-top:	mdm-top.c $(OBJS)
.PHONY:		all
