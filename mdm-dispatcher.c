// Time-stamp: <2008-12-31 14:50:11 cklin>

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

char *show_time(void)
{
  time_t      t;
  struct tm   tm;
  static char buffer[9];

  time(&t);
  localtime_r(&t, &tm);
  strftime(buffer, 9, "%T", &tm);
  return buffer;
}

int main(int argc, char *argv[])
{
  int    listenfd;
  int    retval, status;
  int    agent;
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

  for (agent=0, maxfd=0; agent<MAX_AGENTS; agent++) {
    fprintf(log, "%s [%d] waiting... ", show_time(), agent);
    commfd[agent] = serv_accept(listenfd);
    read(commfd[agent], &pid, sizeof (pid_t));
    fprintf(log, "online! pid=%d\n", pid);
    if (commfd[agent] > maxfd)  maxfd = commfd[agent];
    busy[agent] = false;
  }
  close(listenfd);
  unlink(argv[1]);

  char  *exec_argv[] =
    { "/usr/bin/lame", "--resample", "22.05",
      "-m", "m", "-V", "6", file, NULL };

  for ( ; ; ) {
    for (agent=0; agent<MAX_AGENTS; agent++) {
      if (busy[agent])  continue;
      if (!fgets(file, MAX_ARG_SIZE, stdin))  continue;
      if (file[strlen(file)-1] == '\n')
        file[strlen(file)-1] = '\0';
      fprintf(log, "%s [%d] %s\n", show_time(), agent, file);
      write_cmd(commfd[agent], exec_argv);
      busy[agent] = true;
    }

    for (agent=0; agent<MAX_AGENTS; agent++)
      if (busy[agent])  break;
    if (agent == MAX_AGENTS)  break;

    FD_ZERO(&readfds);
    for (agent=0; agent<MAX_AGENTS; agent++)
      FD_SET(commfd[agent], &readfds);
    retval = select(maxfd+1, &readfds, NULL, NULL, NULL);
    if (retval < 0)  err(4, "select");

    for (agent=0; agent<MAX_AGENTS; agent++)
      if (FD_ISSET(commfd[agent], &readfds)) {
        if (!busy[agent])
          errx(5, "agent %d protocol error", agent);
        read(commfd[agent], &status, sizeof (int));
        fprintf(log, "%s [%d] done, status %d\n",
                show_time(), agent, status);
        busy[agent] = false;
      }
  }

  status = 0;
  for (agent=0; agent<MAX_AGENTS; agent++)
    write(commfd[agent], &status, sizeof (int));

  return 0;
}
