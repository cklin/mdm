// Time-stamp: <2009-02-06 23:43:03 cklin>

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

typedef struct {
  int   issue_fd;
  pid_t pid;
} slave;

static slave slaves[MAX_SLAVES];
static int   sc;

#define GOOD_SLAVE(x) (0 <= (x) && (x) < sc)

void slave_init(int fd, pid_t pid)
{
  assert(sc < MAX_SLAVES);
  slaves[sc].issue_fd = fd;
  slaves[sc++].pid = pid;
  FD_SET(fd, &openfds);
  if (fd > maxfd)  maxfd = fd;
}

void slave_exit(int slave)
{
  assert(GOOD_SLAVE(slave));
  FD_CLR(slaves[slave].issue_fd, &openfds);
  slaves[slave] = slaves[--sc];
}

static bool wind_down;
static int  fetch_fd;
static FILE *log;

void issue(int slave)
{
  int opcode, run_fd, index;
  int slave_fd = slaves[slave].issue_fd;
  job job;

  if (!wind_down) {
    run_fd = serv_accept(fetch_fd);
    readn(run_fd, &opcode, sizeof (int));
    if (opcode == 0) {
      close(run_fd);
      wind_down = true;
    }
  }
  if (wind_down) {
    write_int(slave_fd, 0);
    close(slave_fd);
    slave_exit(slave);
    return;
  }

  read_job(run_fd, &job);
  write_int(run_fd, 0);
  close(run_fd);

  write_int(slave_fd, 1);
  write_job(slave_fd, &job);

  fprintf(log, "[%5d]", slaves[slave].pid);
  for (index=0; job.cmd.svec[index]; index++)
    fprintf(log, " %s", job.cmd.svec[index]);
  fprintf(log, "\n");

  release_job(&job);
}

void get_status(int slave)
{
  int status;
  if (readn(slaves[slave].issue_fd, &status, sizeof (int))) {
    fprintf(log, "[%5d] done, status %d\n",
            slaves[slave].pid, status);
    issue(slave);
  }
  else {
    fprintf(log, "[%5d] lost connection\n",
            slaves[slave].pid);
    slave_exit(slave);
  }
}

int main(int argc, char *argv[])
{
  char      *fetch_addr;
  pid_t     pid;
  int       listenfd;
  char      *sockdir;
  int       slave;

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
  while (sc > 0 || !wind_down) {
    fd_set readfds = openfds;
    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(4, "select");

    for (slave=sc-1; slave>=0; slave--)
      if (FD_ISSET(slaves[slave].issue_fd, &readfds))
        get_status(slave);

    if (FD_ISSET(listenfd, &readfds)) {
      int new_fd;
      new_fd = serv_accept(listenfd);
      if (sc == MAX_SLAVES)
        close(new_fd);
      else {
        pid_t slave_pid;
        readn(new_fd, &slave_pid, sizeof (pid_t));
        slave_init(new_fd, slave_pid);
        fprintf(log, "[%5d] online!\n", pid);
        issue(sc-1);
      }
    }
  }

  return 0;
}
