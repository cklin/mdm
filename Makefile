# Time-stamp: <2008-12-23 09:52:02 cklin>

all:		mdm-agent mdm-dispatcher
mdm-agent:	mdm-agent.c comms.c comms.h
mdm-dispatcher:	mdm-dispatcher.c comms.c comms.h
.PHONY:		all
