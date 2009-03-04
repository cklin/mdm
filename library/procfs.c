// Time-stamp: <2009-02-28 11:32:50 cklin>

/*
   procfs.c - Middleman System Linux /proc Procedures

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

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "middleman.h"

static time_t boot_time(void)
{
  static unsigned long boot = 0;

  if (boot == 0) {
    char buffer[1024];
    FILE *stat_fp = fopen("/proc/stat", "r");
    while (fgets(buffer, sizeof (buffer), stat_fp))
      if (strncmp(buffer, "btime ", 6) == 0) {
        sscanf(buffer+6, "%lu", &boot);
        break;
      }
    fclose(stat_fp);
    assert(boot != 0);
  }
  return boot;
}

static time_t jiffy_to_sec(unsigned long long jiffy)
{
  static unsigned long long clk_tck;

  if (clk_tck == 0)
    clk_tck = sysconf(_SC_CLK_TCK);
  return jiffy/clk_tck;
}

#pragma GCC diagnostic ignored "-Wformat"

bool proc_stat(pid_t pid, proc *pptr)
{
  unsigned long long start_jiffies;
  char               path[40], buffer[1024], *start;
  int                stat_fd, num;

  snprintf(path, sizeof (path), "/proc/%d/stat", pid);
  stat_fd = open(path, O_RDONLY);
  if (stat_fd < 0)  return false;

  num = read(stat_fd, buffer, sizeof (buffer)-1);
  close(stat_fd);
  if (num < 0)  return false;
  buffer[num] = '\0';

  start = strrchr(buffer, ')')+2;
  if (start == NULL)  return false;

  num = sscanf(start,
               "%c "                        // state
               "%d %*d %*d %*d %*d "        // ppid
               "%*u %*lu %*lu %*lu %*lu "   // flags
               "%lu %*lu %*ld %*ld "        // utime
               "%*ld %*ld %*ld %*ld "       // priority
               "%llu",                      // starttime
               &(pptr->state),
               &(pptr->ppid),
               &(pptr->utime),
               &start_jiffies);
  if (num != 4)  return false;

  pptr->utime = jiffy_to_sec(pptr->utime);
  pptr->start_time = boot_time() + jiffy_to_sec(start_jiffies);
  return true;
}

char *time_string(unsigned long seconds)
{
  static char   buffer[10];
  unsigned long minutes;

  minutes = seconds / 60;
  seconds %= 60;

  if (minutes > 99)
    return "99:59+";
  sprintf(buffer, "%2lu:%02lu ", minutes, seconds);
  return buffer;
}
