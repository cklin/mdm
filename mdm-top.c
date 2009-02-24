// Time-stamp: <2009-02-23 23:08:10 cklin>

#include <assert.h>
#include <err.h>
#include <ncurses.h>
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
  pid_t pid, run_pid;
  bool  running, done;
} run;

static run runs[MAX_HISTORY];
static int rc, sc;

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

static void init_run(pid_t pid)
{
  if (rc == MAX_HISTORY)
    release_run();

  runs[rc].pid = pid;
  runs[rc].running = false;
  runs[rc].done = false;
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

static void start_run(pid_t pid, pid_t run_pid, sv *cmd)
{
  int index = find_run(pid);
  assert(runs[index].running == false);
  assert(runs[index].done    == false);
  runs[index].run_pid = run_pid;
  runs[index].running = true;
  runs[index].cmd = *cmd;
  flatten_sv(&(runs[index].cmd));
}

static void end_run(pid_t pid)
{
  int index = find_run(pid);
  run *rptr = runs+index;

  assert(rptr->running == true);
  assert(rptr->done    == false);
  rptr->running = false;
  rptr->done    = true;
  rptr->pc.state = ' ';
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
  mvprintw(0, 2, "Slaves online: %d", sc);
  mvprintw(0, 28, "%s", ctime(&now));

  getmaxyx(stdscr, row, col);

  for (index=1, y=2; index<rc; index++) {
    if (row-y <= rc-index && !rptr->running)
      continue;
    rptr = runs+index;
    pptr = &(rptr->pc);

    if (rptr->running) {
      pptr->state = '!';
      proc_stat(rptr->run_pid, pptr);
      attron(A_REVERSE);
    }
    utime = time_string(pptr->utime);
    ltime = localtime(&(pptr->start_time));
    strftime(start, sizeof (start), "%T", ltime);

    move(y++, 0);
    printw("%s %5d ", start, rptr->run_pid);
    printw("%c %s  ", pptr->state, utime);
    addnstr(rptr->cmd.buffer, col-25);
    for (x=25+strlen(rptr->cmd.buffer); x<col; x++)
      addch(' ');
    attroff(A_REVERSE);
  }
  move(0, col-1);
  refresh();
}

int main(int argc, char *argv[])
{
  int            master_fd, op;
  pid_t          slv_pid, run_pid;
  sv             cmd;
  fd_set         readfds;
  struct timeval tout;

  if (argc != 2)
    errx(1, "Need socket directory argument");
  master_fd = hookup(argv[1]);

  initscr();
  nonl();
  cbreak();
  noecho();

  for ( ; ; ) {
    FD_ZERO(&readfds);
    FD_SET(master_fd, &readfds);
    gettimeofday(&tout, NULL);
    tout.tv_sec = 0;
    tout.tv_usec = 1050000-tout.tv_usec;

    if (select(master_fd+1, &readfds, NULL, NULL, &tout) < 0)
      err(4, "select");

    if (!FD_ISSET(master_fd, &readfds)) {
      update_display();
      continue;
    }

    readn(master_fd, &op, sizeof (int));
    if (op == 0)  break;

    readn(master_fd, &slv_pid, sizeof (pid_t));
    switch (op) {
    case 1:
      init_run(slv_pid);
      readn(master_fd, &run_pid, sizeof (pid_t));
      read_sv(master_fd, &cmd);
      start_run(slv_pid, run_pid, &cmd);
      break;
    case 2:
      end_run(slv_pid);
      break;
    case 3:
      sc++;
      break;
    case 4:
      sc--;
      break;
    default:
      errx(5, "Unknown opcode %d", op);
    }
    update_display();
  }

  getch();
  endwin();
  return 0;
}

