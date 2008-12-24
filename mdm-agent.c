// Time-stamp: <2008-12-23 15:53:34 cklin>

#include <sys/types.h>
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include "comms.h"

extern char **environ;

int main(int argc, char *argv[])
{
  char  buffer[4096];
  int   cmdlen;
  int   commfd;
  int   status;
  pid_t pid;

  if (argc != 2)
    errx(1, "need server socket pathname argument");
  commfd = cli_conn(argv[1]);

  pid = getpid();
  write(commfd, &pid, sizeof (pid_t));

  for ( ; ; ) {
    readn(commfd, &cmdlen, sizeof (int));
    readn(commfd, buffer, cmdlen);
    if (cmdlen == 0)  break;

    char *exec_argv[64];
    int  aidx=0, bidx=0;

    exec_argv[aidx++] = buffer;
    while (bidx < cmdlen) {
      if (buffer[bidx++])  continue;
      exec_argv[aidx++] = buffer+bidx;
    }
    exec_argv[--aidx] = NULL;

    printf("received command:");
    for (aidx=0; exec_argv[aidx]; aidx++)
      printf(" %s", exec_argv[aidx]);
    printf("\n");

    pid = fork();
    if (pid == 0) {
      close(commfd);
      execve(buffer, exec_argv, environ);
    }

    wait(&status);
    write(commfd, &status, sizeof (int));
  }

  return 0;
}
