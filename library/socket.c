// Time-stamp: <2009-03-11 22:14:47 cklin>

/*
   socket.c - Middleman System Socket Procedures

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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
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

// Advanced Programming in the Unix Environment, Program 15.9

#define CONTROLLEN (sizeof (struct cmsghdr)+sizeof (int))

static struct cmsghdr *cmptr = NULL;

int send_fd(int sockfd, int fd)
{
  struct iovec   iov;
  struct msghdr  msg;
  char           ch;

  ch              = 0;
  iov.iov_base    = &ch;
  iov.iov_len     = 1;
  msg.msg_iov     = &iov;
  msg.msg_iovlen  = 1;
  msg.msg_name    = NULL;
  msg.msg_namelen = 0;

  if (!cmptr)  cmptr = xmalloc(CONTROLLEN);
  cmptr->cmsg_level  = SOL_SOCKET;
  cmptr->cmsg_type   = SCM_RIGHTS;
  cmptr->cmsg_len    = CONTROLLEN;
  msg.msg_control    = (caddr_t) cmptr;
  msg.msg_controllen = CONTROLLEN;
  *(int *) CMSG_DATA(cmptr) = fd;

  if (sendmsg(sockfd, &msg, 0) != 1) {
    warn("send_fd sendmsg");
    return -1;
  }
  return 0;
}

// Advanced Programming in the Unix Environment, Program 15.10

int recv_fd(int sockfd)
{
  struct cmsghdr *cmptr = NULL;
  struct iovec   iov;
  struct msghdr  msg;
  char           ch;
  int            nread;

  iov.iov_base    = &ch;
  iov.iov_len     = 1;
  msg.msg_iov     = &iov;
  msg.msg_iovlen  = 1;
  msg.msg_name    = NULL;
  msg.msg_namelen = 0;

  if (!cmptr)  cmptr = xmalloc(CONTROLLEN);
  msg.msg_control    = (caddr_t) cmptr;
  msg.msg_controllen = CONTROLLEN;
  nread = recvmsg(sockfd, &msg, 0);

  if (nread < 0) {
    warn("recv_fd recvmsg");
    return -1;
  }
  if (nread == 0) {
    warnx("recv_fd: connection closed by server");
    return -2;
  }
  if (ch == 0)
    if (msg.msg_controllen == CONTROLLEN)
      return *(int *) CMSG_DATA(cmptr);

  warnx("recv_fd: protocol error");
  return -3;
}

// Write a scalar value to a file descriptor

ssize_t write_int(int fd, int v)
{
  return writen(fd, &v, sizeof (int));
}

ssize_t write_pid(int fd, pid_t v)
{
  return writen(fd, &v, sizeof (pid_t));
}

ssize_t write_time(int fd, time_t v)
{
  return writen(fd, &v, sizeof (time_t));
}

// Basic security checks for the IPC directory

void check_sockdir(const char *path)
{
  struct stat st;

  if (lstat(path, &st) < 0)
    err(110, "stat(\"%s\")", path);
  if (! S_ISDIR(st.st_mode))
    errx(111, "%s is not a directory", path);
  if (st.st_mode & S_IWOTH)
    errx(112, "%s is world-writable", path);
}
