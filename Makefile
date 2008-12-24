# Time-stamp: <2008-12-23 18:15:59 cklin>

all:		mdm-agent mdm-dispatcher
mdm-agent:	mdm-agent.c comms.o cmdline.o
mdm-dispatcher:	mdm-dispatcher.c comms.o cmdline.o
.PHONY:		all
