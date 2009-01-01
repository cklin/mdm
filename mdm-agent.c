// Time-stamp: <2009-01-01 00:13:56 cklin>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include "bounds.h"
#include "comms.h"
#include "cmdline.h"

int hookup(const char path[])
{
  char addr[MAX_PATH_SIZE];

  check_sockdir(path);
  strncpy(addr, path, sizeof (addr));
  strncat(addr, CMD_SOCK, sizeof (addr));
  return cli_conn(addr);
}

int main(int argc, char *argv[])
{
  struct argv cmd, env;
  char        cwd[MAX_ARG_SIZE];
  int         cmd_len;
  int         comm_fd, initwd_fd;
  int         status;
  pid_t       pid;

  if (argc != 2)
    errx(1, "Need socket directory argument");

  initwd_fd = open(".", O_RDONLY);
  comm_fd = hookup(argv[1]);
  pid = getpid();
  write(comm_fd, &pid, sizeof (pid_t));

  for ( ; ; ) {
    cmd_len = read_args(comm_fd, &cmd);
    if (cmd_len == 0)  break;
    read_args(comm_fd, &env);
    read_block(comm_fd, cwd);

    pid = fork();
    if (pid == 0) {
      close(comm_fd);
      chdir(cwd);
      execve(cmd.args[0], cmd.args, env.args);
    }
    wait(&status);
    write(comm_fd, &status, sizeof (int));
  }

  fchdir(initwd_fd);
  return 0;
}
