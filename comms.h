// Time-stamp: <2008-12-23 09:50:50 cklin>

#ifndef __COMMS_H__
#define __COMMS_H__

#include <unistd.h>

ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t readn(int fd, void *vptr, size_t n);

int serv_listen(const char *name);
int cli_conn(const char *name);
int serv_accept(int listenfd);

#endif
