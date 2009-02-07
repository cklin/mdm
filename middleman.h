// Time-stamp: <2009-02-06 22:26:59 cklin>

#ifndef __COMMS_H__
#define __COMMS_H__

#include <unistd.h>

#define MAX_WORKERS   4

#define CMD_SOCK_VAR  "MDM_CMD_SOCK"
#define FETCH_SOCK    "fetch"
#define ISSUE_SOCK    "issue"
#define LOG_FILE      "log"

typedef struct {
  char *buffer;
  char **svec;
} sv;

int read_block(int fd, char **buffer);
int read_sv(int fd, sv *sv);
int unpack_svec(sv *sv, int size);

int write_string(int fd, const char buffer[]);
int write_sv(int fd, const char *svec[]);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t write_int(int fd, int v);

int serv_listen(const char *name);
int serv_accept(int listenfd);
int cli_conn(const char *name);

void check_sockdir(const char *path);

void *xmalloc(size_t size);
char *path_join(const char *path, const char *name);
void release_sv(sv *sv);

#endif
