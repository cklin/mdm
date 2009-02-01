// Time-stamp: <2009-01-31 22:00:34 cklin>

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "middleman.h"

bool check_exec(const char *addr)
{
  struct stat exe_stat;
  uid_t       euid;
  gid_t       egid;

  euid = geteuid();
  egid = getegid();

  if (stat(addr, &exe_stat) < 0)
    return false;
  if (!S_ISREG(exe_stat.st_mode))
    return false;
  if (exe_stat.st_uid == euid && (exe_stat.st_mode & S_IXUSR))
    return true;
  if (exe_stat.st_gid == egid && (exe_stat.st_mode & S_IXGRP))
    return true;
  if (exe_stat.st_mode & S_IXOTH)
    return true;
  return false;
}

char *resolv_exec(char *exe)
{
  static char addr[MAX_PATH_SIZE];
  char        *path, *end;
  int         span, elen;

  elen = strlen(exe);

  if (exe[0] == '/') {
    if (check_exec(exe))
      return exe;
    return NULL;
  }

  if (strchr(exe, '/')) {
    if (!getcwd(addr, MAX_PATH_SIZE))
      return NULL;
    span = strlen(addr);
    if (span+elen+1 < MAX_PATH_SIZE) {
      addr[span] = '/';
      strcpy(addr+span+1, exe);
      if (check_exec(addr))
        return addr;
    }
    return NULL;
  }

  path = getenv("PATH");
  if (!path)  return NULL;

  for ( ; ; ) {
    end = strchrnul(path, ':');
    span = (int) (end-path);
    if (span+elen+1 < MAX_PATH_SIZE) {
      strncpy(addr, path, span);
      addr[span] = '/';
      strcpy(addr+span+1, exe);
      if (check_exec(addr))
        return addr;
    }
    if (*end != ':')
      break;
    path = end+1;
  }
  return NULL;
}
