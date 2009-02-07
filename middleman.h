// Time-stamp: <2009-02-07 09:08:10 cklin>

#ifndef __COMMS_H__
#define __COMMS_H__

#include <unistd.h>

#define MAX_SLAVES    4

#define CMD_SOCK_VAR  "MDM_CMD_SOCK"
#define FETCH_SOCK    "fetch"
#define ISSUE_SOCK    "issue"
#define CON_SOCK      "console"
#define LOG_FILE      "messages"

typedef struct {
  char *buffer;
  char **svec;
} sv;

typedef struct {
  sv   cmd, env;
  char *cwd;
} job;

int serv_listen(const char *name);
int serv_accept(int listenfd);
int cli_conn(const char *name);

void check_sockdir(const char *path);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t write_int(int fd, int v);

void read_job(int fd, job *job);
void write_job(int fd, const job *job);

void *xmalloc(size_t size);
char *path_join(const char *path, const char *name);
void release_job(job *job);

#endif
