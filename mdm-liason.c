// Time-stamp: <2009-01-01 13:39:50 cklin>

#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "cmdline.h"

extern char **environ;

int main(int argc, char *argv[])
{
  char        *cwd;
  int         cmd_fd;
  struct stat dump_stat;

  if (argc < 2)
    errx(1, "Need dump file argument");

  lstat(*(++argv), &dump_stat);
  if (! S_ISREG(dump_stat.st_mode))
    errx(2, "%s: Not a regular file", *argv);
  if (dump_stat.st_size != 0)
    errx(3, "%s: File not empty", *argv);
  if (dump_stat.st_uid != geteuid())
    errx(4, "%s: Belongs to someone else", *argv);

  cmd_fd = open(*argv, O_WRONLY);
  if (cmd_fd < 0)
    err(5, "Cannot open %s", *argv);

  cwd = (char *) get_current_dir_name();
  write_string(cmd_fd, cwd);
  write_args(cmd_fd, (const char **) ++argv);
  write_args(cmd_fd, (const char **) environ);
  close(cmd_fd);

  return 0;
}
