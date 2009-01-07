// Time-stamp: <2009-01-06 19:24:11 cklin>

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

#define FD_WORKER(fd) ((fd) != -1)

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
  int       retval, status;
  int       worker, nworkers;
  int       commfd[MAX_WORKERS], maxfd;
  bool      busy[MAX_WORKERS];
  pid_t     pid;
  char      file[MAX_ARG_SIZE];
  char      *sockdir;
  char      cmdaddr[MAX_PATH_SIZE];
  char      logaddr[MAX_PATH_SIZE];
  FILE      *log;
  fd_set    readfds;
  bool      wind_down;

  if (argc != 2)
    errx(1, "Need socket directory argument");
  sockdir = argv[1];

  check_sockdir(sockdir);
  strncpy(cmdaddr, sockdir, sizeof (cmdaddr));
  strncat(cmdaddr, CMD_SOCK, sizeof (cmdaddr));
  listenfd = serv_listen(cmdaddr);

  strncpy(logaddr, sockdir, sizeof (logaddr));
  strncat(logaddr, LOG_FILE, sizeof (logaddr));
  log = fopen(logaddr, "w+");
  if (log == NULL)  err(3, "Log file %s", logaddr);
  setvbuf(log, NULL, _IONBF, 0);

  nworkers = 0;
  for (worker=0; worker<MAX_WORKERS; worker++)
    commfd[worker] = -1;

  wind_down = false;
  for ( ; ; ) {
    if (! wind_down)
      for (worker=0; worker<MAX_WORKERS; worker++) {
        if (! FD_WORKER(commfd[worker]))  continue;
        if (busy[worker])  continue;
        if (!fgets(file, MAX_ARG_SIZE, stdin)) {
          wind_down = true;
          break;
        }
        issue(commfd[worker], file);
        fprintf(log, "[%d] %s\n", worker, file);
        busy[worker] = true;
      }

    for (worker=0; worker<MAX_WORKERS; worker++) {
      if (! FD_WORKER(commfd[worker]))  continue;
      if (busy[worker])  continue;
      write(commfd[worker], &zero, sizeof (int));
      close(commfd[worker]);
      commfd[worker] = -1;
      nworkers--;
    }
    if (wind_down && nworkers == 0)  break;

    FD_ZERO(&readfds);
    FD_SET(listenfd, &readfds);
    for (worker=0, maxfd=listenfd; worker<MAX_WORKERS; worker++) {
      if (! FD_WORKER(commfd[worker]))  continue;
      FD_SET(commfd[worker], &readfds);
      if (commfd[worker] > maxfd)  maxfd = commfd[worker];
    }
    retval = select(maxfd+1, &readfds, NULL, NULL, NULL);
    if (retval < 0)  err(4, "select");

    for (worker=0; worker<MAX_WORKERS; worker++) {
      if (! FD_WORKER(commfd[worker]))  continue;
      if (FD_ISSET(commfd[worker], &readfds)) {
        if (!busy[worker])
          errx(5, "worker %d protocol error", worker);
        retval = read(commfd[worker], &status, sizeof (int));
        if (retval == 0) {
          fprintf(log, "[%d] lost connection\n", worker);
          commfd[worker] = -1;
          nworkers--;
          continue;
        }
        fprintf(log, "[%d] done, status %d\n", worker, status);
        busy[worker] = false;
      }
    }

    if (FD_ISSET(listenfd, &readfds)) {
      int comm;

      comm = serv_accept(listenfd);
      if (nworkers == MAX_WORKERS) {
        close(comm);
        continue;
      }
      for (worker=0; FD_WORKER(commfd[worker]); worker++);
      commfd[worker] = comm;
      read(commfd[worker], &pid, sizeof (pid_t));
      fprintf(log, "[%d] online! pid=%d\n", worker, pid);
      busy[worker] = false;
      nworkers++;
    }
  }

  return 0;
}
