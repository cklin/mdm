// Time-stamp: <2009-02-06 23:17:01 cklin>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <err.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include "middleman.h"

// Advanced Programming in the Unix Environment, Program 15.22

int serv_listen(const char *name)
{
  int                sockfd, addrlen;
  struct sockaddr_un unix_addr;
  struct sockaddr    *sock_addr;

  sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    warn("socket");
    return -1;
  }

  addrlen = strlen(name);
  if (addrlen >= sizeof (unix_addr.sun_path)) {
    warnx("serv_listen: socket name too long");
    return -2;
  }
  addrlen += sizeof (unix_addr.sun_family);

  memset(&unix_addr, 0, sizeof (unix_addr));
  unix_addr.sun_family = AF_UNIX;
  strcpy(unix_addr.sun_path, name);

  unlink(name);
  sock_addr = (struct sockaddr *) &unix_addr;
  if (bind(sockfd, sock_addr, addrlen) < 0) {
    warn("bind");
    return -3;
  }

  if (listen(sockfd, 5) < 0) {
    warn("listen");
    return -4;
  }
  return sockfd;
}

// Advanced Programming in the Unix Environment, Program 15.24

int serv_accept(int listenfd)
{
  int connfd;

  connfd = accept(listenfd, NULL, NULL);
  if (connfd < 0)
    warn("accept");
  return connfd;
}

// Advanced Programming in the Unix Environment, Program 15.23

int cli_conn(const char *name)
{
  int                sockfd, addrlen;
  struct sockaddr_un unix_addr;
  struct sockaddr    *sock_addr;

  sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    warn("socket");
    return -1;
  }

  addrlen = strlen(name);
  if (addrlen >= sizeof (unix_addr.sun_path)) {
    warnx("serv_listen: socket name too long");
    return -2;
  }
  addrlen += sizeof (unix_addr.sun_family);

  memset(&unix_addr, 0, sizeof (unix_addr));
  unix_addr.sun_family = AF_UNIX;
  strcpy(unix_addr.sun_path, name);

  sock_addr = (struct sockaddr *) &unix_addr;
  if (connect(sockfd, sock_addr, addrlen) < 0) {
    warn("connect");
    return -3;
  }
  return sockfd;
}

// Advanced Programming in the Unix Environment, Program 12.13

ssize_t readn(int fd, void *vptr, size_t n)
{
  size_t  remaining;
  ssize_t received;
  char    *ptr;

  remaining = n;
  ptr = vptr;
  while (remaining > 0) {
    received = read(fd, ptr, remaining);
    if (received <= 0) {
      if (received < 0)  warn("read");
      return received;
    }
    remaining -= received;
    ptr += received;
  }
  return n;
}

// Advanced Programming in the Unix Environment, Program 12.12

ssize_t writen(int fd, const void *vptr, size_t n)
{
  size_t     remaining;
  ssize_t    written;
  const char *ptr;

  remaining = n;
  ptr = vptr;
  while (remaining > 0) {
    written = write(fd, ptr, remaining);
    if (written <= 0) {
      if (written < 0)  warn("write");
      return written;
    }
    remaining -= written;
    ptr += written;
  }
  return n;
}

// Write an integer to a file descriptor

ssize_t write_int(int fd, int v)
{
  return writen(fd, &v, sizeof (int));
}

// Basic security checks for the IPC directory

void check_sockdir(const char *path)
{
  struct stat st;

  if (lstat(path, &st) < 0)
    err(2, "stat(\"%s\")", path);
  if (! S_ISDIR(st.st_mode))
    errx(2, "%s is not a directory", path);
  if (st.st_mode & S_IWOTH)
    errx(2, "%s is world-writable", path);
}

// Read variable recorded-size block from file descriptor

int read_block(int fd, char **buffer)
{
  int size = 0;

  readn(fd, &size, sizeof (int));
  if (size == 0) {
    *buffer = NULL;
    return 0;
  }
  *buffer = xmalloc(size);
  readn(fd, *buffer, size);
  if ((*buffer)[size-1]) {
    warnx("read_block: input is not null-terminated");
    return -2;
  }
  return size;
}

// Unpack string vector written by write_sv

int unpack_svec(sv *sv, int size)
{
  int ch, seg;

  ch = seg = 0;
  sv->svec[seg++] = sv->buffer;
  while (ch < size) {
    if (sv->buffer[ch++])  continue;
    sv->svec[seg++] = sv->buffer+ch;
  }
  sv->svec[--seg] = NULL;
  return seg;
}

// Read string vector from file descriptor

int read_sv(int fd, sv *sv)
{
  int size, count;

  readn(fd, &count, sizeof (int));
  sv->svec = xmalloc(++count * sizeof (char *));
  size = read_block(fd, &(sv->buffer));
  if (size > 0)  unpack_svec(sv, size);
  return size;
}

// Write zero-terminated string with size to file descriptor

int write_string(int fd, const char buffer[])
{
  int size;

  size = strlen(buffer)+1;
  write_int(fd, size);
  writen(fd, buffer, size);
  return size;
}

// Write NULL-terminated string vector to file descriptor

int write_sv(int fd, const char *svec[])
{
  int index, size;

  for (index=0, size=0; svec[index]; index++, size++)
    size += strlen(svec[index]);
  write_int(fd, index);
  write_int(fd, size);
  for (index=0; svec[index]; index++)
    writen(fd, svec[index], strlen(svec[index])+1);
  return 0;
}

void read_job(int fd, job *job)
{
  read_block(fd, &(job->cwd));
  read_sv(fd, &(job->cmd));
  read_sv(fd, &(job->env));
}

void write_job(int fd, const job *job)
{
  write_string(fd, job->cwd);
  write_sv(fd, (const char **) job->cmd.svec);
  write_sv(fd, (const char **) job->env.svec);
}

