// Time-stamp: <2009-02-28 11:31:36 cklin>

/*
   mdm-run.c - Middleman System Job Proxy

   Copyright 2009 Chuan-kai Lin

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
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
