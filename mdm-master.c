// Time-stamp: <2009-02-23 21:34:59 cklin>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "middleman.h"

typedef struct {
  job   job;
  int   issue_fd, status;
  pid_t pid, run_pid;
  bool  idle;
} slave;

#define GOOD_SLAVE(x) (0 < (x) && (x) < sc)

static slave  slaves[1+MAX_SLAVES];
static fd_set openfds;
static int    maxfd, sc;

static void bump_maxfd(int fd)
{
  if (fd > maxfd)  maxfd = fd;
}

static void slave_init(int fd)
{
  assert(sc < sizeof (slaves));
  slaves[sc].status = 0;
  slaves[sc].issue_fd = fd;
  readn(fd, &(slaves[sc].pid), sizeof (pid_t));
  slaves[sc].idle = true;
  FD_SET(fd, &openfds);
  bump_maxfd(fd);
  sc++;
}

static void slave_exit(int slave, bool halt)
{
  int slave_fd;

  assert(GOOD_SLAVE(slave));
  slave_fd = slaves[slave].issue_fd;
  release_job(&(slaves[slave].job));
  slaves[slave] = slaves[--sc];

  FD_CLR(slave_fd, &openfds);
  if (halt)  write_int(slave_fd, 0);
  close(slave_fd);
}

static char *sockdir;

static int init_issue(void)
{
  char *issue_addr = path_join(sockdir, ISSUE_SOCK);
  int  issue_fd = serv_listen(issue_addr);

  free(issue_addr);
  bump_maxfd(issue_fd);
  return issue_fd;
}

static int mon_fd;

static void init_monitor(void)
{
  char *mon_addr = path_join(sockdir, MON_SOCK);
  int mon_sock_fd = serv_listen(mon_addr);

  mon_fd = serv_accept(mon_sock_fd);
  close(mon_sock_fd);
  free(mon_addr);
}

static void init_mesg_log(void)
{
  char *mesg_file = path_join(sockdir, LOG_FILE);
  int  mesg_fd = open(mesg_file, O_WRONLY | O_CREAT, S_IRUSR);

  if (mesg_fd == -1)  err(2, "Log file %s", mesg_file);
  dup2(mesg_fd, STDERR_FILENO);
  close(mesg_fd);
  free(mesg_file);
}

static int init_fetch(void)
{
  char *fetch_addr = path_join(sockdir, FETCH_SOCK);
  int  fetch_fd = serv_listen(fetch_addr);

  setenv(CMD_SOCK_VAR, fetch_addr, 1);
  free(fetch_addr);
  bump_maxfd(fetch_fd);
  return fetch_fd;
}

static int slave_wait(slave *slv)
{
  slv->idle = true;
  unregister_job(&slv->job.cmd);
  return readn(slv->issue_fd, &(slv->status), sizeof (int));
}

static bool wind_down, pending;
static job  job_pending;
static int  run_fd;

static void fetch(int fetch_fd)
{
  int opcode;

  assert(!pending);
  run_fd = serv_accept(fetch_fd);
  readn(run_fd, &opcode, sizeof (int));

  if (opcode == 1) {
    read_job(run_fd, &job_pending);
    pending = true;
    return;
  }
  warnx("Unknown mdm-run opcode %d", opcode);
}

static void issue(slave *slv)
{
  assert(slv->idle);
  release_job(&(slv->job));
  slv->job = job_pending;
  write_int(slv->issue_fd, 1);
  write_job(slv->issue_fd, &(slv->job));
  readn(slv->issue_fd, &(slv->run_pid), sizeof (pid_t));
  slv->idle = false;

  write_int(mon_fd, 1);
  write_pid(mon_fd, slv->pid);
  write_pid(mon_fd, slv->run_pid);
  write_sv(mon_fd, slv->job.cmd.svec);
}

static void issue_ack(int slave_index)
{
  slave *slv = slaves+slave_index;

  assert(pending);
  issue(slv);
  pending = false;

  write_int(run_fd, slv->status);
  close(run_fd);
}

static void process_tick(void)
{
  int index;

  for (index=sc-1; index>0; index--)
    if (slaves[index].idle) {
      if (pending) {
        if (validate_job(&job_pending.cmd)) {
          register_job(&job_pending.cmd);
          issue_ack(index);
        }
      }
      else if (wind_down) {
        write_int(mon_fd, 4);
        write_pid(mon_fd, slaves[index].pid);
        slave_exit(index, true);
      }
    }
}

static void run_main(int issue_fd, char *argv[])
{
  assert(sc == 0);
  slave_init(serv_accept(issue_fd));

  job_pending.cwd = get_current_dir_name();
  job_pending.cmd.svec = argv;
  job_pending.env.svec = environ;
  issue(slaves);
}

int main(int argc, char *argv[])
{
  int   issue_fd, fetch_fd;
  int   slave_index;

  if (argc < 4)
    errx(1, "Need comms directory, iospec file, and command");
  sockdir = *(++argv);
  check_sockdir(sockdir);

  init_iospec(*(++argv));
  issue_fd = init_issue();
  daemon(1, 0);
  init_mesg_log();
  init_monitor();
  fetch_fd = init_fetch();
  run_main(issue_fd, argv+1);

  wind_down = false;
  while (sc > 1 || !wind_down) {
    fd_set readfds = openfds;
    if (!pending && !wind_down)
      FD_SET(fetch_fd, &readfds);
    if (sc < sizeof (slaves) && !wind_down)
      FD_SET(issue_fd, &readfds);

    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(3, "select");

    if (FD_ISSET(fetch_fd, &readfds))
      fetch(fetch_fd);
    if (FD_ISSET(slaves->issue_fd, &readfds)) {
      slave_wait(slaves);
      wind_down = true;
    }

    for (slave_index=sc-1; slave_index>0; slave_index--) {
      slave *slv = &slaves[slave_index];
      if (FD_ISSET(slv->issue_fd, &readfds)) {
        if (slave_wait(slv) > 0) {
          write_int(mon_fd, 2);
          write_pid(mon_fd, slv->pid);
        } else {
          write_int(mon_fd, 4);
          write_pid(mon_fd, slv->pid);
          slave_exit(slave_index, false);
        }
      }
    }

    if (FD_ISSET(issue_fd, &readfds)) {
      slave_init(serv_accept(issue_fd));
      write_int(mon_fd, 3);
      write_pid(mon_fd, slaves[sc-1].pid);
    }
    process_tick();
  }

  write_int(slaves->issue_fd, 0);
  write_int(mon_fd, 0);
  return 0;
}
