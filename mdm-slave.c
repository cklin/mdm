// Time-stamp: <2009-02-22 09:37:38 cklin>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "middleman.h"

extern char **environ;

static int hookup(const char *sockdir)
{
  char *master_addr;
  int  master_fd;

  check_sockdir(sockdir);
  master_addr = path_join(sockdir, ISSUE_SOCK);
  master_fd = cli_conn(master_addr);
  if (master_fd < 0)
    errx(2, "Cannot connect to mdm-master");
  free(master_addr);
  return master_fd;
}

int main(int argc, char *argv[])
{
  job   job;
  int   master_fd, op, status;
  pid_t pid;

  if (argc != 2)
    errx(1, "Need socket directory argument");

  master_fd = hookup(argv[1]);
  write_pid(master_fd, getpid());
  if (isatty(STDIN_FILENO))
    printf("\ek%s\e\\\n", ttyname(STDIN_FILENO));

  for ( ; ; ) {
    readn(master_fd, &op, sizeof (int));
    if (op == 0)  break;

    pid = fork();
    if (pid == 0) {
      read_job(master_fd, &job);
      close(master_fd);

      chdir(job.cwd);
      environ = job.env.svec;
      execvp(job.cmd.svec[0], job.cmd.svec);
    }
    wait(&status);
    write_int(master_fd, status);
  }
  return 0;
}
