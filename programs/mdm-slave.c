// Time-stamp: <2009-03-04 13:30:44 cklin>

/*
   mdm-slave.c - Middleman System Job Runner

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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
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

void screen_title(const char *format, ...)
{
  va_list argp;

  if (isatty(STDIN_FILENO)) {
    va_start(argp, format);
    printf("\ek");
    vprintf(format, argp);
    printf("\e\\\n");
    va_end(argp);
  }
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

  for ( ; ; ) {
    screen_title("Idle");

    readn(master_fd, &op, sizeof (int));
    if (op == 0)  break;
    read_job(master_fd, &job);

    pid = fork();
    if (pid == 0) {
      close(master_fd);
      chdir(job.cwd);
      environ = job.env.svec;
      execvp(job.cmd.svec[0], job.cmd.svec);
    }

    screen_title("Process %u", pid);
    write_pid(master_fd, pid);
    wait(&status);
    write_int(master_fd, status);
  }
  return 0;
}
