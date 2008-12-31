// Time-stamp: <2008-12-31 15:40:15 cklin>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <err.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "bounds.h"
#include "comms.h"

int main(int argc, char *argv[])
{
  int    listenfd;
  int    retval, status;
  int    agent, nagents;
  int    commfd[MAX_AGENTS], maxfd;
  bool   busy[MAX_AGENTS];
  pid_t  pid;
  char   file[MAX_ARG_SIZE];
  char   *sockdir;
  char   cmdaddr[MAX_PATH_SIZE];
  char   logaddr[MAX_PATH_SIZE];
  FILE   *log;
  fd_set readfds;

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

  char  *exec_argv[] =
    { "/usr/bin/lame", "--resample", "22.05",
      "-m", "m", "-V", "6", file, NULL };

  nagents = 0;
  for ( ; ; ) {
    for (agent=0; agent<nagents; agent++) {
      if (busy[agent])  continue;
      if (!fgets(file, MAX_ARG_SIZE, stdin))  continue;
      if (file[strlen(file)-1] == '\n')
        file[strlen(file)-1] = '\0';
      fprintf(log, "[%d] %s\n", agent, file);
      write_cmd(commfd[agent], exec_argv);
      busy[agent] = true;
    }

    for (agent=0; agent<nagents; agent++)
      if (busy[agent])  break;
    if (nagents > 0 && agent == nagents)  break;

    FD_ZERO(&readfds);
    FD_SET(listenfd, &readfds);
    for (agent=0, maxfd=listenfd; agent<nagents; agent++) {
      FD_SET(commfd[agent], &readfds);
      if (commfd[agent] > maxfd)  maxfd = commfd[agent];
    }
    retval = select(maxfd+1, &readfds, NULL, NULL, NULL);
    if (retval < 0)  err(4, "select");

    for (agent=0; agent<nagents; agent++)
      if (FD_ISSET(commfd[agent], &readfds)) {
        if (!busy[agent])
          errx(5, "agent %d protocol error", agent);
        read(commfd[agent], &status, sizeof (int));
        fprintf(log, "[%d] done, status %d\n", agent, status);
        busy[agent] = false;
      }

    if (FD_ISSET(listenfd, &readfds)) {
      int comm;

      comm = serv_accept(listenfd);
      if (nagents == MAX_AGENTS) {
        close(comm);
        continue;
      }
      commfd[nagents] = comm;
      read(commfd[nagents], &pid, sizeof (pid_t));
      fprintf(log, "[%d] online! pid=%d\n", nagents, pid);
      busy[nagents] = false;
      nagents++;
    }
  }

  status = 0;
  for (agent=0; agent<nagents; agent++)
    write(commfd[agent], &status, sizeof (int));

  return 0;
}
