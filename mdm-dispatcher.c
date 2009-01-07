// Time-stamp: <2009-01-06 21:10:10 cklin>

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
static int ready;

#define IS_READY(x) (0 <= (x) && (x) < ready)

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

static bool wind_down;
static FILE *log;

void issue(int widx)
{
  const int zero = 0, one = 1;
  char      file[MAX_ARG_SIZE];
  int       last;
  int       worker_fd = workers[widx];
  char      *cwd;
  char      *cmd[] =
    { "/usr/bin/lame", "--resample", "22.05",
      "-m", "m", "-V", "6", file, NULL };

  if (wind_down || !fgets(file, MAX_ARG_SIZE, stdin)) {
    writen(worker_fd, &zero, sizeof (int));
    close(worker_fd);
    worker_exit(widx);
    wind_down = true;
    return;
  }

  last = strlen(file)-1;
  if (file[last] == '\n')  file[last] = '\0';

  cwd = get_current_dir_name();
  writen(worker_fd, &one, sizeof (int));
  write_string(worker_fd, cwd);
  write_args(worker_fd, (const char **) cmd);
  write_args(worker_fd, (const char **) environ);
  free(cwd);

  fprintf(log, "[%d] %s\n", widx, file);
}

int main(int argc, char *argv[])
{
  int       listenfd;
  int       widx;
  int       maxfd;
  char      *sockdir;
  fd_set    readfds;

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
    FD_ZERO(&readfds);
    FD_SET(listenfd, &readfds);
    for (widx=0, maxfd=listenfd; widx<ready; widx++) {
      FD_SET(workers[widx], &readfds);
      if (workers[widx] > maxfd)  maxfd = workers[widx];
    }
    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(4, "select");

    for (widx=ready-1; widx>=0; widx--)
      if (FD_ISSET(workers[widx], &readfds)) {
        int status;
        if (readn(workers[widx], &status, sizeof (int))) {
          fprintf(log, "[%d] done, status %d\n", widx, status);
          issue(widx);
        }
        else {
          fprintf(log, "[%d] lost connection\n", widx);
          worker_exit(widx);
        }
      }

    if (FD_ISSET(listenfd, &readfds)) {
      int new_fd;
      new_fd = serv_accept(listenfd);
      if (ready == MAX_WORKERS)
        close(new_fd);
      else {
        pid_t pid;
        worker_init(new_fd);
        readn(new_fd, &pid, sizeof (pid_t));
        fprintf(log, "[%d] online! pid=%d\n", ready, pid);
        issue(ready-1);
      }
    }
  }

  return 0;
}
