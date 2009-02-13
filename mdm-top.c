// Time-stamp: <2009-02-12 21:03:58 cklin>

#include <err.h>
#include <stdlib.h>
#include <ncurses.h>
#include "middleman.h"

static int hookup(const char *sockdir)
{
  char *master_addr;
  int  master_fd;

  check_sockdir(sockdir);
  master_addr = path_join(sockdir, MON_SOCK);
  master_fd = cli_conn(master_addr);
  if (master_fd < 0)
    errx(2, "Cannot connect to mdm-master");
  free(master_addr);
  return master_fd;
}

int main(int argc, char *argv[])
{
  int   master_fd, op;
  pid_t slv_pid;
  sv    *cmd;

  if (argc != 2)
    errx(1, "Need socket directory argument");

  master_fd = hookup(argv[1]);

  for ( ; ; ) {
    readn(master_fd, &op, sizeof (int));
    if (op == 0)  break;

    readn(master_fd, &slv_pid, sizeof (pid_t));
    if (op == 1)
      printf("[%05d] running\n", slv_pid);
    else if (op == 2)
      printf("[%05d] idle\n", slv_pid);
    else if (op == 3)
      printf("[%05d] online\n", slv_pid);
    else if (op == 4)
      printf("[%05d] exit\n", slv_pid);
  }
  return 0;
}

