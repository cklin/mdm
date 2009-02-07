// Time-stamp: <2009-02-07 00:03:53 cklin>

#include <assert.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
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

void issue(int slave)
{
  int opcode, run_fd;
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

  warnx("[%5d] %s", slaves[slave].pid, job.cmd.svec[0]);
  release_job(&job);
}

void get_status(int slave)
{
  int status;

  if (readn(slaves[slave].issue_fd, &status, sizeof (int))) {
    warnx("[%5d] done, status %d\n", slaves[slave].pid, status);
    issue(slave);
  }
  else {
    warnx("[%5d] lost connection\n", slaves[slave].pid);
    slave_exit(slave);
  }
}

int main(int argc, char *argv[])
{
  char      *sockdir, *fetch_addr;
  pid_t     pid;
  int       listenfd;
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
    char *msgs_file = path_join(sockdir, LOG_FILE);
    int msgs_fd = open(msgs_file, O_WRONLY | O_CREAT);
    if (msgs_fd == -1)  err(2, "Log file %s", msgs_file);
    dup2(msgs_fd, STDERR_FILENO);
    close(msgs_fd);
    free(msgs_file);
  }

  pid = fork();
  if (pid == 0) {
    int core_fd, status = 0;
    pid = fork();
    if (pid == 0) {
      setenv(CMD_SOCK_VAR, fetch_addr, 1);
      if (execvp(*argv, ++argv) < 0)
        err(3, "execvp: %s", *argv);
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
      int new_fd = serv_accept(listenfd);
      if (sc == MAX_SLAVES)
        close(new_fd);
      else {
        pid_t slave_pid;
        readn(new_fd, &slave_pid, sizeof (pid_t));
        slave_init(new_fd, slave_pid);
        warnx("[%5d] online!\n", pid);
        issue(sc-1);
      }
    }
  }
  return 0;
}
