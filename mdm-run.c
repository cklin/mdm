// Time-stamp: <2009-02-08 10:02:32 cklin>

#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "middleman.h"

extern char **environ;

int main(int argc, char *argv[])
{
  struct stat sock_stat;
  char        *master_addr;
  int         master_fd, status;
  job         job;

  if (argc < 2)
    errx(1, "Please supply command as arguments");

  master_addr = getenv(CMD_SOCK_VAR);
  if (!master_addr)
    if (execvp(*argv, ++argv) < 0)
      errx(2, "execve: %s", *argv);

  if (lstat(master_addr, &sock_stat) < 0)
    err(3, "%s: Cannot stat master socket", master_addr);
  if (!S_ISSOCK(sock_stat.st_mode))
    errx(4, "%s: Not a socket", master_addr);
  if (sock_stat.st_uid != geteuid())
    errx(5, "%s: Belongs to someone else", master_addr);

  master_fd = cli_conn(master_addr);
  if (master_fd < 0)
    errx(6, "%s: cli_conn error", master_addr);

  write_int(master_fd, 1);
  job.cwd = get_current_dir_name();
  job.cmd.svec = ++argv;
  job.env.svec = environ;
  write_job(master_fd, &job);
  readn(master_fd, &status, sizeof (int));
  close(master_fd);

  return status;
}
