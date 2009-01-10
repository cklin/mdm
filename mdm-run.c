// Time-stamp: <2009-01-09 21:29:29 cklin>

#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "comms.h"
#include "cmdline.h"

extern char **environ;

void write_cmd(char *argv[])
{
  char        *cwd, *core_addr;
  int         core_fd;
  struct stat sock_stat, exe_stat;

  if (stat(*argv, &exe_stat) < 0)
    err(2, "Cannot stat executable %s", *argv);
  if (!S_ISREG(exe_stat.st_mode))
    errx(3, "%s: Not a regular file", *argv);

  core_addr = getenv("MDM_CMD_SOCK");
  if (!core_addr)  return;

  if (lstat(core_addr, &sock_stat) < 0)
    err(4, "%s: Cannot stat core socket", core_addr);
  if (!S_ISSOCK(sock_stat.st_mode))
    errx(5, "%s: Not a socket", core_addr);
  if (sock_stat.st_uid != geteuid())
    errx(6, "%s: Belongs to someone else", core_addr);

  core_fd = cli_conn(core_addr);
  if (core_fd < 0)
    errx(7, "%s: cli_conn error", core_addr);

  cwd = get_current_dir_name();
  write_string(core_fd, cwd);
  write_args(core_fd, (const char **) argv);
  write_args(core_fd, (const char **) environ);
  close(core_fd);

  exit(0);
}

int main(int argc, char *argv[])
{
  if (argc < 2)
    errx(1, "Please supply command as arguments");

  write_cmd(++argv);
  if (execve(*argv, argv, environ) < 0)
    errx(8, "execve: %s", *argv);

  return 0;
}
