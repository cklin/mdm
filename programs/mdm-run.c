// Time-stamp: <2009-03-04 14:35:17 cklin>

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
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "middleman.h"

extern char **environ;

int main(int argc, char *argv[])
{
  struct stat sock_stat;
  char        *master_addr;
  bool        sync_mode;
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

  sync_mode = !strcmp(basename(*argv), "mdm-sync");
  write_int(master_fd, sync_mode ? 2 : 1);

  job.cwd = get_current_dir_name();
  job.cmd.svec = ++argv;
  job.env.svec = environ;
  write_job(master_fd, &job);
  readn(master_fd, &status, sizeof (int));

  if (sync_mode)
    if (execvp(*argv, argv) < 0)
      errx(2, "execve: %s", *argv);

  readn(master_fd, &status, sizeof (int));
  close(master_fd);

  return status;
}
