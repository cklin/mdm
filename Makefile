# Time-stamp: <2009-01-01 13:11:32 cklin>

CFLAGS = -Wall -D_GNU_SOURCE

all:		mdm-agent mdm-dispatcher mdm-liason
mdm-agent:	mdm-agent.c comms.o cmdline.o
mdm-dispatcher:	mdm-dispatcher.c comms.o cmdline.o
mdm-liason:	mdm-liason.c comms.o cmdline.o
.PHONY:		all
