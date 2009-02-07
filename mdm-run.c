// Time-stamp: <2009-02-06 22:57:55 cklin>

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
  char        *core_addr;
  int         core_fd, status;
  job         job;

  if (argc < 2)
    errx(1, "Please supply command as arguments");

  core_addr = getenv(CMD_SOCK_VAR);
  if (!core_addr)
    if (execvp(*argv, ++argv) < 0)
      errx(8, "execve: %s", *argv);

  if (lstat(core_addr, &sock_stat) < 0)
    err(4, "%s: Cannot stat core socket", core_addr);
  if (!S_ISSOCK(sock_stat.st_mode))
    errx(5, "%s: Not a socket", core_addr);
  if (sock_stat.st_uid != geteuid())
    errx(6, "%s: Belongs to someone else", core_addr);

  core_fd = cli_conn(core_addr);
  if (core_fd < 0)
    errx(7, "%s: cli_conn error", core_addr);

  write_int(core_fd, 1);
  job.cwd = get_current_dir_name();
  job.cmd.svec = ++argv;
  job.env.svec = environ;
  write_job(core_fd, &job);
  readn(core_fd, &status, sizeof (int));
  close(core_fd);

  return 0;
}
