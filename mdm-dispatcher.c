#include <sys/socket.h>
#include <err.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
  int    index, base, len, agent;
  int    commfd[4], maxfd;
  bool   ready[4];
  pid_t  pid;
  char   file[200];
  char   buffer[4096];
  fd_set readfds;

  if (argc != 2)
    errx(1, "need server socket pathname argument");
  listenfd = serv_listen(argv[1]);

  for (agent=0, maxfd=0; agent<4; agent++) {
    printf("%s waiting for agent %d... ", show_time(), agent);
    commfd[agent] = serv_accept(listenfd);
    read(commfd[agent], &pid, sizeof (pid_t));
    printf("online! pid=%d\n", pid);
    if (commfd[agent] > maxfd)  maxfd = commfd[agent];
    ready[agent] = true;
  }
  close(listenfd);

  char  *exec_argv[] =
    { "/usr/bin/lame", "--resample", "22.05",
      "-m", "m", "-V", "6", NULL };

  for (index=0, base=0; exec_argv[index]; index++) {
    strcpy(buffer+base, exec_argv[index]);
    base += strlen(exec_argv[index])+1;
  }

  for ( ; ; ) {
    for (agent=0; agent<4; agent++) {
      if (!ready[agent])  continue;
      if (!gets(file))  continue;
      printf("%s %s => agent %d\n",
             show_time(), file, agent);
      len = base+strlen(file)+1;
      write(commfd[agent], &len, sizeof (int));
      write(commfd[agent], buffer, base);
      write(commfd[agent], file, strlen(file)+1);
      ready[agent] = false;
    }

    for (agent=0; ready[agent] && agent<4; agent++);
    if (agent == 4)  break;

    FD_ZERO(&readfds);
    for (agent=0; agent<4; agent++)
      FD_SET(commfd[agent], &readfds);
    retval = select(maxfd+1, &readfds, NULL, NULL, NULL);
    if (retval < 0)  err(2, "select");

    for (agent=0; agent<4; agent++) {
      if (!FD_ISSET(commfd[agent], &readfds))
        continue;
      if (ready[agent])
        errx(3, "agent %d writes data on standby", agent);
      read(commfd[agent], &status, sizeof (int));
      printf("%s agent %d done, status %d\n",
             show_time(), agent, status);
      ready[agent] = true;
    }
  }

  len = 0;
  for (agent=0; agent<4; agent++)
    write(commfd[agent], &len, sizeof (int));

  return 0;
}
