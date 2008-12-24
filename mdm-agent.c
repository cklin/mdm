// Time-stamp: <2008-12-23 18:12:57 cklin>

#include <sys/types.h>
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include "bounds.h"
#include "comms.h"

extern char **environ;

int main(int argc, char *argv[])
{
  char  buffer[MAX_ARG_SIZE];
  char  *exec_argv[MAX_ARG_COUNT];
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
    cmdlen = read_cmd(commfd, buffer);
    cmd_pointers(buffer, cmdlen, exec_argv);
    if (cmdlen == 0)  break;

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
