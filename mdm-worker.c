// Time-stamp: <2009-02-06 22:25:43 cklin>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "middleman.h"

extern char **environ;

int hookup(const char *sockdir)
{
  char *master_addr;
  int status;

  check_sockdir(sockdir);
  master_addr = path_join(sockdir, ISSUE_SOCK);
  status = cli_conn(master_addr);
  free(master_addr);
  return status;
}

int main(int argc, char *argv[])
{
  sv    cmd, env;
  char  *cwd;
  int   op;
  int   comm_fd, initwd_fd;
  int   status;
  pid_t pid;

  if (argc != 2)
    errx(1, "Need socket directory argument");

  initwd_fd = open(".", O_RDONLY);
  comm_fd = hookup(argv[1]);
  pid = getpid();
  writen(comm_fd, &pid, sizeof (pid_t));

  for ( ; ; ) {
    readn(comm_fd, &op, sizeof (int));
    if (op == 0)  break;

    pid = fork();
    if (pid == 0) {
      read_block(comm_fd, &cwd);
      read_sv(comm_fd, &cmd);
      read_sv(comm_fd, &env);
      close(comm_fd);

      chdir(cwd);
      environ = env.svec;
      execvp(cmd.svec[0], cmd.svec);
    }
    wait(&status);
    write_int(comm_fd, status);
  }

  fchdir(initwd_fd);
  return 0;
}
