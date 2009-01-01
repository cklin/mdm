// Time-stamp: <2008-12-31 17:36:14 cklin>

#include <sys/types.h>
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "bounds.h"
#include "comms.h"

int main(int argc, char *argv[])
{
  char  cmd_buf[MAX_ARG_SIZE];
  char  env_buf[MAX_ARG_SIZE];
  char  *exec_cmd[MAX_ARG_COUNT];
  char  *exec_env[MAX_ARG_COUNT];
  int   cmdlen, envlen;
  int   commfd;
  int   status;
  char  *sockdir;
  char  cmdaddr[MAX_PATH_SIZE];
  pid_t pid;

  if (argc != 2)
    errx(1, "Need socket directory argument");
  sockdir = argv[1];

  check_sockdir(sockdir);
  strncpy(cmdaddr, sockdir, sizeof (cmdaddr));
  strncat(cmdaddr, CMD_SOCK, sizeof (cmdaddr));
  commfd = cli_conn(cmdaddr);

  pid = getpid();
  write(commfd, &pid, sizeof (pid_t));

  for ( ; ; ) {
    cmdlen = read_cmd(commfd, cmd_buf);
    if (cmdlen == 0)  break;
    cmd_pointers(cmd_buf, cmdlen, exec_cmd);
    envlen = read_cmd(commfd, env_buf);
    cmd_pointers(env_buf, envlen, exec_env);

    pid = fork();
    if (pid == 0) {
      close(commfd);
      execve(exec_cmd[0], exec_cmd, exec_env);
    }
    wait(&status);
    write(commfd, &status, sizeof (int));
  }
  return 0;
}
