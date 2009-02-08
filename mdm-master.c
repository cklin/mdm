// Time-stamp: <2009-02-08 10:04:51 cklin>

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
  int   issue_fd, status;
  pid_t pid;
} slave;

#define GOOD_SLAVE(x) (0 <= (x) && (x) < sc)

static slave  slaves[MAX_SLAVES];
static fd_set openfds;
static int    maxfd, sc;

static void slave_init(int fd, pid_t pid)
{
  assert(sc < MAX_SLAVES);
  slaves[sc].status = 0;
  slaves[sc].issue_fd = fd;
  slaves[sc++].pid = pid;
  FD_SET(fd, &openfds);
  if (fd > maxfd)  maxfd = fd;
}

static void slave_exit(int slave)
{
  int slave_fd;

  assert(GOOD_SLAVE(slave));
  slave_fd = slaves[slave].issue_fd;
  slaves[slave] = slaves[--sc];

  FD_CLR(slave_fd, &openfds);
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

static int get_status(slave *slv)
{
  return readn(slv->issue_fd, &(slv->status), sizeof (int));
}

static bool wind_down;

static int accept_run(void)
{
  int run_fd, opcode;

  run_fd = serv_accept(fetch_fd);
  readn(run_fd, &opcode, sizeof (int));
  if (opcode == 0) {
    close(run_fd);
    wind_down = true;
    return -1;
  }
  return run_fd;
}

static void issue(int slave)
{
  slave *slv = &slaves[slave];
  int   run_fd;
  job   job;

  if (!wind_down)
    run_fd = accept_run();
  if (wind_down) {
    warnx("[%5d] exit", slv->pid);
    slave_exit(slave);
    return;
  }

  read_job(run_fd, &job);
  write_int(run_fd, slv->status);
  close(run_fd);

  write_int(slv->issue_fd, 1);
  write_job(slv->issue_fd, &job);

  warnx("[%5d] > %s", slv->pid, job.cmd.svec[0]);
  release_job(&job);
}

int main(int argc, char *argv[])
{
  char  *fetch_addr;
  pid_t main_pid;
  int   issue_fd;
  slave *slv;
  int   slave;

  if (argc < 3)
    errx(1, "Need comms directory and command");
  sockdir = *(++argv);
  check_sockdir(sockdir);

  issue_fd = init_issue();
  daemon(1, 0);
  init_mesg_log();
  fetch_addr = init_fetch();
  main_pid = run_main(issue_fd, fetch_addr, argv+1);

  wind_down = false;
  while (sc > 0 || !wind_down) {
    fd_set readfds = openfds;
    if (sc < MAX_SLAVES && !wind_down)
      FD_SET(issue_fd, &readfds);
    if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0)
      err(3, "select");

    for (slave=sc-1; slave>=0; slave--) {
      slv = &slaves[slave];
      if (FD_ISSET(slv->issue_fd, &readfds)) {
        if (get_status(slv) > 0) {
          warnx("[%5d] done (%d)", slv->pid, slv->status);
          issue(slave);
        }
        else {
          warnx("[%5d] lost", slv->pid);
          slave_exit(slave);
        }
      }
    }

    if (FD_ISSET(issue_fd, &readfds)) {
      int   slave_fd = serv_accept(issue_fd);
      pid_t slave_pid;
      readn(slave_fd, &slave_pid, sizeof (pid_t));
      slave_init(slave_fd, slave_pid);
      warnx("[%5d] online!", slave_pid);
      issue(sc-1);
    }
  }
  kill(main_pid, SIGALRM);
  return 0;
}
