// Time-stamp: <2009-01-01 14:44:37 cklin>

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

#define FD_AGENT(fd) ((fd) != -1)

void issue(int agent_fd, char *file)
{
  const int one = 1;
  int       last;
  char      *cwd;
  char      *cmd[] =
    { "/usr/bin/lame", "--resample", "22.05",
      "-m", "m", "-V", "6", file, NULL };

  last = strlen(file)-1;
  if (file[last] == '\n')  file[last] = '\0';

  cwd = (char *) get_current_dir_name();
  writen(agent_fd, &one, sizeof (int));
  write_string(agent_fd, cwd);
  write_args(agent_fd, (const char **) cmd);
  write_args(agent_fd, (const char **) environ);
  free(cwd);
}

int main(int argc, char *argv[])
{
  const int zero = 0;
  int       listenfd;
  int       retval, status;
  int       agent, nagents;
  int       commfd[MAX_AGENTS], maxfd;
  bool      busy[MAX_AGENTS];
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

  nagents = 0;
  for (agent=0; agent<MAX_AGENTS; agent++)
    commfd[agent] = -1;

  wind_down = false;
  for ( ; ; ) {
    if (! wind_down)
      for (agent=0; agent<MAX_AGENTS; agent++) {
        if (! FD_AGENT(commfd[agent]))  continue;
        if (busy[agent])  continue;
        if (!fgets(file, MAX_ARG_SIZE, stdin)) {
          wind_down = true;
          break;
        }
        issue(commfd[agent], file);
        fprintf(log, "[%d] %s\n", agent, file);
        busy[agent] = true;
      }

    for (agent=0; agent<MAX_AGENTS; agent++) {
      if (! FD_AGENT(commfd[agent]))  continue;
      if (busy[agent])  continue;
      write(commfd[agent], &zero, sizeof (int));
      close(commfd[agent]);
      commfd[agent] = -1;
      nagents--;
    }
    if (wind_down && nagents == 0)  break;

    FD_ZERO(&readfds);
    FD_SET(listenfd, &readfds);
    for (agent=0, maxfd=listenfd; agent<MAX_AGENTS; agent++) {
      if (! FD_AGENT(commfd[agent]))  continue;
      FD_SET(commfd[agent], &readfds);
      if (commfd[agent] > maxfd)  maxfd = commfd[agent];
    }
    retval = select(maxfd+1, &readfds, NULL, NULL, NULL);
    if (retval < 0)  err(4, "select");

    for (agent=0; agent<MAX_AGENTS; agent++) {
      if (! FD_AGENT(commfd[agent]))  continue;
      if (FD_ISSET(commfd[agent], &readfds)) {
        if (!busy[agent])
          errx(5, "agent %d protocol error", agent);
        retval = read(commfd[agent], &status, sizeof (int));
        if (retval == 0) {
          fprintf(log, "[%d] lost connection\n", agent);
          commfd[agent] = -1;
          nagents--;
          continue;
        }
        fprintf(log, "[%d] done, status %d\n", agent, status);
        busy[agent] = false;
      }
    }

    if (FD_ISSET(listenfd, &readfds)) {
      int comm;

      comm = serv_accept(listenfd);
      if (nagents == MAX_AGENTS) {
        close(comm);
        continue;
      }
      for (agent=0; FD_AGENT(commfd[agent]); agent++);
      commfd[agent] = comm;
      read(commfd[agent], &pid, sizeof (pid_t));
      fprintf(log, "[%d] online! pid=%d\n", agent, pid);
      busy[agent] = false;
      nagents++;
    }
  }

  return 0;
}
