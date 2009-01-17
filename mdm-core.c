// Time-stamp: <2009-01-16 20:45:03 cklin>

#include <assert.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <err.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bounds.h"
#include "comms.h"
#include "cmdline.h"

extern char **environ;

static fd_set openfds;
static int    maxfd;

struct worker {
  int   fd;
  pid_t pid;
};

static struct worker workers[MAX_WORKERS];
static int ready;

#define IS_READY(x) (0 <= (x) && (x) < ready)

void worker_init(int fd, pid_t pid)
{
  assert(ready < MAX_WORKERS);
  workers[ready].fd = fd;
  workers[ready++].pid = pid;
  FD_SET(fd, &openfds);
  if (fd > maxfd)  maxfd = fd;
}

void worker_exit(int widx)
{
  assert(IS_READY(widx));
  FD_CLR(workers[widx].fd, &openfds);
  workers[widx] = workers[--ready];
}

static bool wind_down;
static FILE *log;

void issue(int widx, int issue_fd)
{
  const int zero = 0, one = 1;
  int       opcode, run_fd, index;
  int       worker_fd = workers[widx].fd;

  struct argv cmd, env;
  char        cwd[MAX_ARG_SIZE];

  if (!wind_down) {
    run_fd = serv_accept(issue_fd);
    readn(run_fd, &opcode, sizeof (int));
    if (opcode == 0) {
      close(run_fd);
      wind_down = true;
    }
  }
  if (wind_down) {
    writen(worker_fd, &zero, sizeof (int));
    close(worker_fd);
    worker_exit(widx);
    return;
  }

  read_block(run_fd, cwd);
  read_args(run_fd, &cmd);
  read_args(run_fd, &env);
  close(run_fd);

  writen(worker_fd, &one, sizeof (int));
  write_string(worker_fd, cwd);
  write_args(worker_fd, (const char **) cmd.args);
  write_args(worker_fd, (const char **) env.args);

  fprintf(log, "[%5d]", workers[widx].pid);
  for (index=0; cmd.args[index]; index++)
    fprintf(log, " %s", cmd.args[index]);
  fprintf(log, "\n");
}

int main(int argc, char *argv[])
{
  char      fetchaddr[MAX_PATH_SIZE];
  pid_t     pid;
  int       listenfd, fetch_fd;
  char      *sockdir;
  int       widx;

  printf("argc = %d\n", argc);
  if (argc < 3)
    errx(1, "Need comms directory and command");
  sockdir = *(++argv);

  {
    check_sockdir(sockdir);
    strncpy(fetchaddr, sockdir, sizeof (fetchaddr));
    strncat(fetchaddr, FETCH_SOCK, sizeof (fetchaddr));
    fetch_fd = serv_listen(fetchaddr);
  }

  {
    char cmdaddr[MAX_PATH_SIZE];
    check_sockdir(sockdir);
    strncpy(cmdaddr, sockdir, sizeof (cmdaddr));
    strncat(cmdaddr, ISSUE_SOCK, sizeof (cmdaddr));
    listenfd = serv_listen(cmdaddr);
    FD_SET(listenfd, &openfds);
    maxfd = listenfd;
  }

  {
    char logaddr[MAX_PATH_SIZE];
    strncpy(logaddr, sockdir, sizeof (logaddr));
    strncat(logaddr, LOG_FILE, sizeof (logaddr));
    log = fopen(logaddr, "w+");
    if (log == NULL)  err(3, "Log file %s", logaddr);
    setvbuf(log, NULL, _IONBF, 0);
  }

  pid = fork();
  if (pid == 0) {
    const int zero = 0;
    int core_fd, status = 0;
    pid = fork();
    if (pid == 0) {
      setenv("MDM_CMD_SOCK", fetchaddr, 1);
      if (execve(*argv, ++argv, environ) < 0)
        err(8, "execve: %s", *argv);
    }
    sleep(10);
    wait(&status);
    core_fd = cli_conn(fetchaddr);
    writen(core_fd, &zero, sizeof (int));
    exit(status);
  }

  wind_down = false;
  while (ready > 0 || !wind_down) {
    fd_set readfds = openfds;
    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(4, "select");

    for (widx=ready-1; widx>=0; widx--)
      if (FD_ISSET(workers[widx].fd, &readfds)) {
        int status;
        if (readn(workers[widx].fd, &status, sizeof (int))) {
          fprintf(log, "[%5d] done, status %d\n",
                  workers[widx].pid, status);
          issue(widx, fetch_fd);
        }
        else {
          fprintf(log, "[%5d] lost connection\n",
                  workers[widx].pid);
          worker_exit(widx);
        }
      }

    if (FD_ISSET(listenfd, &readfds)) {
      int new_fd;
      new_fd = serv_accept(listenfd);
      if (ready == MAX_WORKERS)
        close(new_fd);
      else {
        pid_t worker_pid;
        readn(new_fd, &worker_pid, sizeof (pid_t));
        worker_init(new_fd, worker_pid);
        fprintf(log, "[%5d] online!\n", pid);
        issue(ready-1, fetch_fd);
      }
    }
  }

  return 0;
}
