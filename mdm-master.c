// Time-stamp: <2009-02-12 00:12:39 cklin>

#include <assert.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "middleman.h"

typedef struct {
  job   job;
  int   issue_fd, status;
  pid_t pid;
  bool  idle;
} slave;

#define GOOD_SLAVE(x) (0 <= (x) && (x) < sc)

static slave  slaves[MAX_SLAVES];
static fd_set openfds;
static int    maxfd, sc;

static void slave_init(int fd)
{
  assert(sc < MAX_SLAVES);
  slaves[sc].status = 0;
  slaves[sc].issue_fd = fd;
  readn(fd, &(slaves[sc].pid), sizeof (pid_t));
  slaves[sc].idle = true;
  FD_SET(fd, &openfds);
  if (fd > maxfd)  maxfd = fd;
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
  if (halt)
    write_int(slave_fd, 0);
  close(slave_fd);
}

static char *sockdir;

static int init_issue(void)
{
  char *issue_addr = path_join(sockdir, ISSUE_SOCK);
  int  issue_fd = serv_listen(issue_addr);

  maxfd = issue_fd;
  free(issue_addr);
  return issue_fd;
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

static int fetch_fd;

static char *init_fetch(void)
{
  char *fetch_addr = path_join(sockdir, FETCH_SOCK);

  fetch_fd = serv_listen(fetch_addr);
  return fetch_addr;
}

static void sig_alarm(int signo)
{
  return;
}

static pid_t run_main(int issue_fd, const char *addr, char *argv[])
{
  pid_t main_pid;
  job   job;
  int   main_fd, master_fd, status;

  main_fd = serv_accept(issue_fd);
  main_pid = fork();
  if (main_pid == 0) {
    close(fetch_fd);
    close(issue_fd);

    setenv(CMD_SOCK_VAR, addr, 1);
    job.cwd = get_current_dir_name();
    job.cmd.svec = argv;
    job.env.svec = environ;

    readn(main_fd, &status, sizeof (pid_t));
    write_int(main_fd, 1);
    write_job(main_fd, &job);
    readn(main_fd, &status, sizeof (int));

    master_fd = cli_conn(addr);
    write_int(master_fd, 0);
    signal(SIGALRM, sig_alarm);
    pause();

    write_int(main_fd, 0);
    exit(status);
  }
  close(main_fd);
  return main_pid;
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

static void fetch(void)
{
  int opcode;

  assert(!pending);
  run_fd = serv_accept(fetch_fd);
  readn(run_fd, &opcode, sizeof (int));
  if (opcode == 0) {
    close(run_fd);
    wind_down = true;
  }
  else {
    read_job(run_fd, &job_pending);
    pending = true;
  }
}

static void issue(int slave_index)
{
  slave *slv = &slaves[slave_index];

  assert(pending);
  write_int(run_fd, slv->status);
  close(run_fd);

  release_job(&(slv->job));
  slv->job = job_pending;
  write_int(slv->issue_fd, 1);
  write_job(slv->issue_fd, &(slv->job));
  warnx("[%5d] > %s", slv->pid, slv->job.cmd.svec[0]);
  slv->idle = false;
  pending = false;
}

static void process_tick(void)
{
  int index;

  assert(pending || wind_down);
  for (index=sc-1; index>=0; index--)
    if (slaves[index].idle) {
      if (pending) {
        if (validate_job(&job_pending.cmd)) {
          register_job(&job_pending.cmd);
          issue(index);
          fetch();
        }
      }
      else {
        warnx("[%5d] exit", slaves[index].pid);
        slave_exit(index, true);
      }
    }
}

int main(int argc, char *argv[])
{
  char  *fetch_addr;
  pid_t main_pid;
  int   issue_fd;
  int   slave_index;

  if (argc < 4)
    errx(1, "Need comms directory, iospec file, and command");
  sockdir = *(++argv);
  check_sockdir(sockdir);

  init_iospec(*(++argv));
  issue_fd = init_issue();
  daemon(1, 0);
  init_mesg_log();
  fetch_addr = init_fetch();
  main_pid = run_main(issue_fd, fetch_addr, argv+1);

  wind_down = false;
  fetch();

  while (sc > 0 || !wind_down) {
    fd_set readfds = openfds;
    if (sc < MAX_SLAVES && !wind_down)
      FD_SET(issue_fd, &readfds);
    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(3, "select");

    for (slave_index=sc-1; slave_index>=0; slave_index--) {
      slave *slv = &slaves[slave_index];
      if (FD_ISSET(slv->issue_fd, &readfds)) {
        if (slave_wait(slv) > 0)
          warnx("[%5d] done (%d)", slv->pid, slv->status);
        else {
          warnx("[%5d] lost", slv->pid);
          slave_exit(slave_index, false);
        }
      }
    }

    if (FD_ISSET(issue_fd, &readfds)) {
      slave_init(serv_accept(issue_fd));
      warnx("[%5d] online!", slaves[sc-1].pid);
    }
    process_tick();
  }
  kill(main_pid, SIGALRM);
  return 0;
}
