// Time-stamp: <2009-02-06 22:10:00 cklin>

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
#include "middleman.h"

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

void issue(int widx, int fetch_fd)
{
  int       opcode, run_fd, index;
  int       worker_fd = workers[widx].fd;

  struct argv cmd, env;
  char        *cwd;

  if (!wind_down) {
    run_fd = serv_accept(fetch_fd);
    readn(run_fd, &opcode, sizeof (int));
    if (opcode == 0) {
      close(run_fd);
      wind_down = true;
    }
  }
  if (wind_down) {
    write_int(worker_fd, 0);
    close(worker_fd);
    worker_exit(widx);
    return;
  }

  read_block(run_fd, &cwd);
  read_args(run_fd, &cmd);
  read_args(run_fd, &env);
  write_int(run_fd, 0);
  close(run_fd);

  write_int(worker_fd, 1);
  write_string(worker_fd, cwd);
  write_args(worker_fd, (const char **) cmd.args);
  write_args(worker_fd, (const char **) env.args);

  fprintf(log, "[%5d]", workers[widx].pid);
  for (index=0; cmd.args[index]; index++)
    fprintf(log, " %s", cmd.args[index]);
  fprintf(log, "\n");

  free(cwd);
  release_argv(&cmd);
  release_argv(&env);
}

void get_status(int widx, int fetch_fd)
{
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

int main(int argc, char *argv[])
{
  char      *fetch_addr;
  pid_t     pid;
  int       listenfd, fetch_fd;
  char      *sockdir;
  int       widx;

  if (argc < 3)
    errx(1, "Need comms directory and command");
  sockdir = *(++argv);

  {
    check_sockdir(sockdir);
    fetch_addr = path_join(sockdir, FETCH_SOCK);
    fetch_fd = serv_listen(fetch_addr);
  }

  {
    char *slave_addr = path_join(sockdir, ISSUE_SOCK);
    listenfd = serv_listen(slave_addr);
    FD_SET(listenfd, &openfds);
    maxfd = listenfd;
    free(slave_addr);
  }

  {
    char *log_file = path_join(sockdir, LOG_FILE);
    log = fopen(log_file, "w+");
    if (log == NULL)  err(3, "Log file %s", log_file);
    setvbuf(log, NULL, _IONBF, 0);
    free(log_file);
  }

  pid = fork();
  if (pid == 0) {
    int core_fd, status = 0;
    pid = fork();
    if (pid == 0) {
      setenv(CMD_SOCK_VAR, fetch_addr, 1);
      if (execvp(*argv, ++argv) < 0)
        err(9, "execvp: %s", *argv);
    }
    wait(&status);
    core_fd = cli_conn(fetch_addr);
    write_int(core_fd, 0);
    exit(status);
  }

  wind_down = false;
  while (ready > 0 || !wind_down) {
    fd_set readfds = openfds;
    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(4, "select");

    for (widx=ready-1; widx>=0; widx--)
      if (FD_ISSET(workers[widx].fd, &readfds))
        get_status(widx, fetch_fd);

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
