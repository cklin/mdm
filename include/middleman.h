// Time-stamp: <2009-03-11 22:01:39 cklin>

/*
   middleman.h - Middleman System Header File

   Copyright 2009 Chuan-kai Lin

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef __COMMS_H__
#define __COMMS_H__

#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#define MAX_SLAVES    16

#define MAX_HISTORY   60

#define CMD_SOCK_VAR  "MDM_CMD_SOCK"
#define FETCH_SOCK    "fetch"
#define ISSUE_SOCK    "issue"
#define MON_SOCK      "monitor"
#define LOG_FILE      "messages"

#define TOP_OP_EXIT     0
#define TOP_OP_FETCH    1
#define TOP_OP_ISSUE    2
#define TOP_OP_DONE     3
#define TOP_OP_ATTN     4
#define TOP_OP_ONLINE   10
#define TOP_OP_OFFLINE  11

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

int send_fd(int sockfd, int fd);
int recv_fd(int sockfd);

void check_sockdir(const char *path);

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);
ssize_t write_int(int fd, int v);
ssize_t write_pid(int fd, pid_t v);
ssize_t write_time(int fd, time_t v);

void read_job(int fd, job *job);
void write_job(int fd, const job *job);

int read_sv(int fd, sv *sv);
int write_sv(int fd, char *const svec[]);

void *xmalloc(size_t size);
char *path_join(const char *path, const char *name);
char *xstrdup(const char *s);
void release_sv(sv *sv);
void release_job(job *job);
void flatten_sv(sv *sv);

void init_iospec(const char *config_name);
bool validate_job(sv *cmd);
void register_job(sv *cmd);
void unregister_job(sv *cmd);

typedef struct {
  char          state;
  pid_t         ppid;
  unsigned long utime;
  time_t        start_time;
} proc;

bool proc_stat(pid_t pid, proc *pptr);
char *time_string(unsigned long seconds);

#endif
