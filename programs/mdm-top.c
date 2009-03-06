// Time-stamp: <2009-03-05 22:44:23 cklin>

/*
   mdm-top.c - Middleman System Monitoring Utility

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
#include <err.h>
#include <errno.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>
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

typedef struct {
  sv    cmd;
  proc  pc;
  pid_t pid;
  bool  running, done;
  int   status;
} run;

static run runs[MAX_HISTORY];
static int rc, sc, ac;

static void release_run(void)
{
  int index;

  assert(rc == MAX_HISTORY);
  for (index=0; runs[index].running; index++);

  release_sv(&(runs[index].cmd));
  while (++index < rc)
    runs[index-1] = runs[index];
  rc--;
}

static void init_run(sv *cmd)
{
  if (rc == MAX_HISTORY)
    release_run();

  runs[rc].pid = 0;
  runs[rc].running = false;
  runs[rc].done = false;
  runs[rc].cmd = *cmd;
  flatten_sv(&(runs[rc].cmd));
  rc++;
}

static int find_run(pid_t pid)
{
  int index;

  for (index=0; index<rc; index++)
    if (runs[index].pid == pid && !runs[index].done)
      return index;

  errx(3, "Cannot find run with pid %d", pid);
  return -1;
}

static void start_run(pid_t pid)
{
  int index = find_run(0);
  assert(index == rc-1);
  assert(runs[index].running == false);
  assert(runs[index].done    == false);
  runs[index].pid     = pid;
  runs[index].running = true;
  ac++;
}

static void end_run(pid_t pid, int status)
{
  int index = find_run(pid);
  run *rptr = runs+index;

  if (status > 255)
    status = 255;

  assert(rptr->running == true);
  assert(rptr->done    == false);
  rptr->running  = false;
  rptr->done     = true;
  rptr->pc.state = ' ';
  rptr->status   = status;
  ac--;
}

void update_display(void)
{
  char      start[9], *utime;
  struct tm *ltime;
  time_t    now;
  run       *rptr;
  proc      *pptr;
  int       index, row, col, y, x;

  now = time(NULL);
  mvprintw(0, 2, "Active Jobs: %2d/%2d", ac, sc);
  mvprintw(0, 28, "%s", ctime(&now));

  getmaxyx(stdscr, row, col);

  mvaddstr(2, 3, "START   PID  ST   CPU   COMMAND");
  for (index=0, y=3; index<rc; index++) {
    if (row-y <= rc-index && !rptr->running)
      continue;
    rptr = runs+index;
    pptr = &(rptr->pc);

    if (rptr->running) {
      pptr->state = '!';
      proc_stat(rptr->pid, pptr);
      attron(A_REVERSE);
    }

    move(y++, 0);
    if (rptr->running || rptr->done) {
      utime = time_string(pptr->utime);
      ltime = localtime(&(pptr->start_time));
      strftime(start, sizeof (start), "%T", ltime);
      printw("%s %5d ", start, rptr->pid);
      if (rptr->running)
        printw("  %c %s  ", pptr->state, utime);
      else
        printw("%3d %s  ", rptr->status, utime);
    }
    else {
      addstr("       -     - ");
      addstr("        -   ");
    }

    addnstr(rptr->cmd.buffer, col-27);
    for (x=27+strlen(rptr->cmd.buffer); x<col; x++)
      addch(' ');
    attroff(A_REVERSE);
  }
  move(0, col-1);
  refresh();
}

void sig_winch(int sig)
{
  endwin();
  initscr();
  refresh();
}

int main(int argc, char *argv[])
{
  int            master_fd, op, status;
  pid_t          run_pid;
  sv             cmd;
  fd_set         readfds;
  struct timeval tout;

  if (argc != 2)
    errx(1, "Need socket directory argument");
  master_fd = hookup(argv[1]);

  initscr();
  signal(SIGWINCH, sig_winch);
  nonl();
  cbreak();
  noecho();

  for ( ; ; ) {
    update_display();

    FD_ZERO(&readfds);
    FD_SET(master_fd, &readfds);
    gettimeofday(&tout, NULL);
    tout.tv_sec = 0;
    tout.tv_usec = 1050000-tout.tv_usec;

    if (select(master_fd+1, &readfds, NULL, NULL, &tout) < 0) {
      if (errno == EINTR)  continue;
      err(4, "select");
    }

    if (!FD_ISSET(master_fd, &readfds))
      continue;
    readn(master_fd, &op, sizeof (int));
    if (op == TOP_OP_EXIT)  break;

    switch (op) {
    case TOP_OP_FETCH:
      read_sv(master_fd, &cmd);
      init_run(&cmd);
      break;
    case TOP_OP_ISSUE:
      readn(master_fd, &run_pid, sizeof (pid_t));
      start_run(run_pid);
      break;
    case TOP_OP_DONE:
      readn(master_fd, &run_pid, sizeof (pid_t));
      readn(master_fd, &status, sizeof (pid_t));
      end_run(run_pid, status);
      break;
    case TOP_OP_ONLINE:
      sc++;
      break;
    case TOP_OP_OFFLINE:
      sc--;
      break;
    default:
      errx(5, "Unknown opcode %d", op);
    }
  }

  endwin();
  return 0;
}

