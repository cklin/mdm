// Time-stamp: <2008-12-31 13:59:15 cklin>

#ifndef __COMMS_H__
#define __COMMS_H__

#define CMD_SOCK  "/cmd"
#define LOG_FILE  "/log"

#include <unistd.h>

ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t readn(int fd, void *vptr, size_t n);

int serv_listen(const char *name);
int cli_conn(const char *name);
int serv_accept(int listenfd);

void check_sockdir(const char *path);

#endif
