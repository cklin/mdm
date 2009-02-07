// Time-stamp: <2009-02-07 09:15:54 cklin>

#include <assert.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "middleman.h"

typedef struct {
  int   issue_fd;
  pid_t pid;
} slave;

#define GOOD_SLAVE(x) (0 <= (x) && (x) < sc)

static slave  slaves[MAX_SLAVES];
static fd_set openfds;
static int    maxfd, sc;

static void slave_init(int fd, pid_t pid)
{
  assert(sc < MAX_SLAVES);
  slaves[sc].issue_fd = fd;
  slaves[sc++].pid = pid;
  FD_SET(fd, &openfds);
  if (fd > maxfd)  maxfd = fd;
}

static void slave_exit(int slave)
{
  int slave_fd;

  assert(GOOD_SLAVE(slave));
  slave_fd = slaves[slave].issue_fd;
  slaves[slave] = slaves[--sc];

  FD_CLR(slave_fd, &openfds);
  write_int(slave_fd, 0);
  close(slave_fd);
}

static int fetch_fd;

static char *init_fetch(const char sockdir[])
{
  char *fetch_addr;

  check_sockdir(sockdir);
  fetch_addr = path_join(sockdir, FETCH_SOCK);
  fetch_fd = serv_listen(fetch_addr);
  return fetch_addr;
}

static int init_issue(const char sockdir[])
{
  char *issue_addr = path_join(sockdir, ISSUE_SOCK);
  int  issue_fd = serv_listen(issue_addr);

  FD_SET(issue_fd, &openfds);
  maxfd = issue_fd;
  free(issue_addr);
  return issue_fd;
}

static void init_mesg(const char sockdir[])
{
  char *mesg_file = path_join(sockdir, LOG_FILE);
  int mesg_fd = open(mesg_file, O_WRONLY | O_CREAT);

  if (mesg_fd == -1)  err(2, "Log file %s", mesg_file);
  dup2(mesg_fd, STDERR_FILENO);
  close(mesg_fd);
  free(mesg_file);
}

static void run_main(const char *addr, char *const argv[])
{
  int   master_fd, status;

  if (fork() == 0) {
    close(fetch_fd);
    setenv(CMD_SOCK_VAR, addr, 1);
    if (fork() == 0)
      if (execvp(*argv, argv) < 0)
        err(3, "execvp: %s", *argv);
    wait(&status);
    master_fd = cli_conn(addr);
    write_int(master_fd, 0);
    exit(status);
  }
}

static bool get_status(int slave)
{
  int status;
  int n = readn(slaves[slave].issue_fd, &status, sizeof (int));

  if (n)  warnx("[%5d] done (%d)", slaves[slave].pid, status);
  else  warnx("[%5d] lost", slaves[slave].pid);
  return (n > 0);
}

static bool wind_down;

static int accept_run(void)
{
  int run_fd, opcode;

  run_fd = serv_accept(fetch_fd);
  readn(run_fd, &opcode, sizeof (int));
  if (opcode == 0) {
    close(run_fd);
    wind_down = true;
    return -1;
  }
  return run_fd;
}

static void issue(int slave)
{
  int slave_fd = slaves[slave].issue_fd;
  int run_fd;
  job job;

  if (!wind_down)
    run_fd = accept_run();
  if (wind_down) {
    warnx("[%5d] exit", slaves[slave].pid);
    slave_exit(slave);
    return;
  }

  read_job(run_fd, &job);
  write_int(run_fd, 0);
  close(run_fd);

  write_int(slave_fd, 1);
  write_job(slave_fd, &job);

  warnx("[%5d] > %s", slaves[slave].pid, job.cmd.svec[0]);
  release_job(&job);
}

int main(int argc, char *argv[])
{
  char *sockdir, *fetch_addr;
  int  issue_fd;
  int  slave;

  if (argc < 3)
    errx(1, "Need comms directory and command");
  sockdir = *(++argv);

  fetch_addr = init_fetch(sockdir);
  run_main(fetch_addr, argv+1);
  issue_fd = init_issue(sockdir);
  init_mesg(sockdir);

  wind_down = false;
  while (sc > 0 || !wind_down) {
    fd_set readfds = openfds;
    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(4, "select");

    for (slave=sc-1; slave>=0; slave--)
      if (FD_ISSET(slaves[slave].issue_fd, &readfds)) {
        if (get_status(slave))  issue(slave);
        else  slave_exit(slave);
      }

    if (FD_ISSET(issue_fd, &readfds)) {
      int slave_fd = serv_accept(issue_fd);
      if (sc == MAX_SLAVES)
        close(slave_fd);
      else {
        pid_t slave_pid;
        readn(slave_fd, &slave_pid, sizeof (pid_t));
        slave_init(slave_fd, slave_pid);
        warnx("[%5d] online!", slave_pid);
        issue(sc-1);
      }
    }
  }
  return 0;
}
