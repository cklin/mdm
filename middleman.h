// Time-stamp: <2009-02-06 22:04:03 cklin>

#ifndef __COMMS_H__
#define __COMMS_H__

#include <unistd.h>

#define MAX_WORKERS   4

#define CMD_SOCK_VAR  "MDM_CMD_SOCK"
#define FETCH_SOCK    "fetch"
#define ISSUE_SOCK    "issue"
#define LOG_FILE      "log"

struct argv {
  char *buffer;
  char **args;
};

int write_args(int fd, const char *args[]);
int read_args(int fd, struct argv *args);

int write_string(int fd, const char buffer[]);
int read_block(int fd, char **buffer);
int unpack_args(char buffer[], int size, char *args[]);

ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t readn(int fd, void *vptr, size_t n);
ssize_t write_int(int fd, int v);

int serv_listen(const char *name);
int cli_conn(const char *name);
int serv_accept(int listenfd);

void check_sockdir(const char *path);

void *xmalloc(size_t size);
char *path_join(const char *path, const char *name);

#endif
