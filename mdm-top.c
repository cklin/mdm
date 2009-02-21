// Time-stamp: <2009-02-20 21:30:12 cklin>

#include <assert.h>
#include <err.h>
#include <stdbool.h>
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

typedef struct {
  sv    cmd;
  pid_t pid;
  bool  running, done;
} run;

#define MAX_HISTORY 10

static run runs[MAX_HISTORY];
static int rc, sc;

static void release_run(void)
{
  int index;

  assert(rc == MAX_HISTORY);
  for (index=0; runs[index].running; index++);

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

  errx(2, "Cannot find run with pid %d", pid);
  return -1;
}

static void start_run(pid_t pid)
{
  int index = find_run(pid);
  assert(runs[index].running == false);
  assert(runs[index].done    == false);
  runs[index].running = true;
}

static void end_run(pid_t pid)
{
  int index = find_run(pid);
  assert(runs[index].running == true);
  assert(runs[index].done    == false);
  runs[index].running = false;
  runs[index].done    = true;
}

void update_display(void)
{
  int index;

  mvprintw(0, 2, "Slaves online: %d", sc);
  for (index=0; index<rc; index++) {
    if (runs[index].running)
      attron(A_REVERSE);
    mvprintw(index+2, 2, "Run %5d", runs[index].pid);
    attroff(A_REVERSE);
  }
  move(0, 0);
  refresh();
}

int main(int argc, char *argv[])
{
  int   master_fd, op;
  pid_t slv_pid;

  if (argc != 2)
    errx(1, "Need socket directory argument");
  master_fd = hookup(argv[1]);

  initscr();
  nonl();
  cbreak();
  noecho();

  for ( ; ; ) {
    readn(master_fd, &op, sizeof (int));
    if (op == 0)  break;

    readn(master_fd, &slv_pid, sizeof (pid_t));
    switch (op) {
    case 1:
      init_run(slv_pid);
      start_run(slv_pid);
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
      errx(3, "Unknown opcode %d", op);
    }
    update_display();
  }

  getch();
  endwin();
  return 0;
}

