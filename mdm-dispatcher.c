// Time-stamp: <2009-01-06 20:29:06 cklin>

#include <assert.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
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

static int workers[MAX_WORKERS];
static int busy, ready;

#define IS_BUSY(x)  (0 <= (x) && (x) < busy)
#define IS_READY(x) (busy <= (x) && (x) < ready)

void worker_swap(int w1, int w2)
{
  int temp;

  temp = workers[w1];
  workers[w1] = workers[w2];
  workers[w2] = temp;
}

void worker_init(int fd)
{
  assert(ready < MAX_WORKERS);
  workers[ready++] = fd;
}

void worker_exit(int w)
{
  assert(IS_READY(w));
  worker_swap(w, --ready);
}

void worker_busy(int w)
{
  assert(IS_READY(w));
  worker_swap(w, busy++);
}

void worker_idle(int w)
{
  assert(IS_BUSY(w));
  worker_swap(w, --busy);
}

void issue(int worker_fd, char *file)
{
  const int one = 1;
  int       last;
  char      *cwd;
  char      *cmd[] =
    { "/usr/bin/lame", "--resample", "22.05",
      "-m", "m", "-V", "6", file, NULL };

  last = strlen(file)-1;
  if (file[last] == '\n')  file[last] = '\0';

  cwd = get_current_dir_name();
  writen(worker_fd, &one, sizeof (int));
  write_string(worker_fd, cwd);
  write_args(worker_fd, (const char **) cmd);
  write_args(worker_fd, (const char **) environ);
  free(cwd);
}

int main(int argc, char *argv[])
{
  const int zero = 0;
  int       listenfd;
  int       worker;
  int       maxfd;
  char      file[MAX_ARG_SIZE];
  char      *sockdir;
  FILE      *log;
  fd_set    readfds;
  bool      wind_down;

  if (argc != 2)
    errx(1, "Need socket directory argument");
  sockdir = argv[1];

  {
    char cmdaddr[MAX_PATH_SIZE];
    check_sockdir(sockdir);
    strncpy(cmdaddr, sockdir, sizeof (cmdaddr));
    strncat(cmdaddr, CMD_SOCK, sizeof (cmdaddr));
    listenfd = serv_listen(cmdaddr);
  }

  {
    char logaddr[MAX_PATH_SIZE];
    strncpy(logaddr, sockdir, sizeof (logaddr));
    strncat(logaddr, LOG_FILE, sizeof (logaddr));
    log = fopen(logaddr, "w+");
    if (log == NULL)  err(3, "Log file %s", logaddr);
    setvbuf(log, NULL, _IONBF, 0);
  }

  wind_down = false;
  while (ready > 0 || !wind_down) {

    assert(ready == busy);

    FD_ZERO(&readfds);
    FD_SET(listenfd, &readfds);
    for (worker=0, maxfd=listenfd; worker<ready; worker++) {
      FD_SET(workers[worker], &readfds);
      if (workers[worker] > maxfd)  maxfd = workers[worker];
    }
    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(4, "select");

    for (worker=busy-1; worker>=0; worker--)
      if (FD_ISSET(workers[worker], &readfds))
        worker_idle(worker);

    for (worker=ready-1; worker>=busy; worker--) {
      int status;
      if (readn(workers[worker], &status, sizeof (int)))
        fprintf(log, "[%d] done, status %d\n", worker, status);
      else {
        fprintf(log, "[%d] lost connection\n", worker);
        worker_exit(worker);
      }
    }

    if (FD_ISSET(listenfd, &readfds)) {
      int new_worker;
      new_worker = serv_accept(listenfd);
      if (ready == MAX_WORKERS)
        close(new_worker);
      else {
        pid_t pid;
        worker_init(new_worker);
        read(new_worker, &pid, sizeof (pid_t));
        fprintf(log, "[%d] online! pid=%d\n", ready, pid);
      }
    }

    for (worker=busy; worker<ready; worker++)
      if (wind_down || !fgets(file, MAX_ARG_SIZE, stdin)) {
        write(workers[worker], &zero, sizeof (int));
        close(workers[worker]);
        worker_exit(worker);
        wind_down = true;
      }
      else {
        issue(workers[worker], file);
        fprintf(log, "[%d] %s\n", worker, file);
        worker_busy(worker);
      }
  }

  return 0;
}
