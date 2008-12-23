// Time-stamp: <2008-12-23 09:47:43 cklin>

#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include "comms.h"

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
      warn("write");
      return written;
    }
    remaining -= written;
    ptr += written;
  }
  return 0;
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
      warn("read");
      return received;
    }
    remaining -= received;
    ptr += received;
  }
  return 0;
}

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

// Advanced Programming in the Unix Environment, Program 15.24

int serv_accept(int listenfd)
{
  int connfd;

  connfd = accept(listenfd, NULL, NULL);
  if (connfd < 0)
    warn("accept");
  return connfd;
}
