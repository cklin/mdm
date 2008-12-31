// Time-stamp: <2008-12-31 13:39:23 cklin>

#include <sys/types.h>
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
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
  char  *sockdir;
  char  sockaddr[MAX_PATH_SIZE];
  pid_t pid;

  if (argc != 2)
    errx(1, "Need socket directory argument");
  sockdir = argv[1];

  if (check_sockdir(sockdir) < 0)
    errx(2, "Socket directory failed validation");
  strncpy(sockaddr, sockdir, sizeof (sockaddr));
  strncat(sockaddr, CMD_SOCK, sizeof (sockaddr));
  commfd = cli_conn(sockaddr);

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
