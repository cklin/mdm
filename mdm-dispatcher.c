// Time-stamp: <2008-12-23 21:39:56 cklin>

#include <sys/socket.h>
#include <err.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "bounds.h"

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
  int    commfd[AGENTS_COUNT], maxfd;
  bool   busy[AGENTS_COUNT];
  pid_t  pid;
  char   file[MAX_ARG_SIZE];
  fd_set readfds;

  if (argc != 2)
    errx(1, "need server socket pathname argument");
  listenfd = serv_listen(argv[1]);

  setvbuf(stdout, NULL, _IONBF, 0);

  for (agent=0, maxfd=0; agent<AGENTS_COUNT; agent++) {
    printf("%s [%d] waiting... ", show_time(), agent);
    commfd[agent] = serv_accept(listenfd);
    read(commfd[agent], &pid, sizeof (pid_t));
    printf("online! pid=%d\n", pid);
    if (commfd[agent] > maxfd)  maxfd = commfd[agent];
    busy[agent] = false;
  }
  close(listenfd);
  unlink(argv[1]);

  char  *exec_argv[] =
    { "/usr/bin/lame", "--resample", "22.05",
      "-m", "m", "-V", "6", file, NULL };

  for ( ; ; ) {
    for (agent=0; agent<AGENTS_COUNT; agent++) {
      if (busy[agent])  continue;
      if (!fgets(file, MAX_ARG_SIZE, stdin))  continue;
      if (file[strlen(file)-1] == '\n')
        file[strlen(file)-1] = '\0';
      printf("%s [%d] %s\n", show_time(), agent, file);
      write_cmd(commfd[agent], exec_argv);
      busy[agent] = true;
    }

    for (agent=0; agent<AGENTS_COUNT; agent++)
      if (busy[agent])  break;
    if (agent == AGENTS_COUNT)  break;

    FD_ZERO(&readfds);
    for (agent=0; agent<AGENTS_COUNT; agent++)
      FD_SET(commfd[agent], &readfds);
    retval = select(maxfd+1, &readfds, NULL, NULL, NULL);
    if (retval < 0)  err(2, "select");

    for (agent=0; agent<AGENTS_COUNT; agent++)
      if (FD_ISSET(commfd[agent], &readfds)) {
        if (!busy[agent])
          errx(3, "agent %d protocol error", agent);
        read(commfd[agent], &status, sizeof (int));
        printf("%s [%d] done, status %d\n",
               show_time(), agent, status);
        busy[agent] = false;
      }
  }

  status = 0;
  for (agent=0; agent<AGENTS_COUNT; agent++)
    write(commfd[agent], &status, sizeof (int));

  return 0;
}
