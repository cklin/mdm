// Time-stamp: <2009-02-04 22:36:37 cklin>

#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "middleman.h"

static bool check_exec(const char *pathname)
{
  struct stat sb;

  if (stat(pathname, &sb) < 0)
    if (S_ISREG(sb.st_mode))
      return (euidaccess(pathname, X_OK) == 0);
  return false;
}

char *resolv_exec(char *exec)
{
  char *pathname, *env_path, *start, *end;

  if (!exec)  return NULL;

  // Absolute or relative pathname, no PATH resolution

  if (strchr(exec, '/')) {
    pathname = canonicalize_file_name(exec);
    if (pathname)
      if (check_exec(pathname))
        return pathname;
    return NULL;
  }

  // Executable name only, require PATH resolution

  env_path = getenv("PATH");
  if (!env_path)  return NULL;

  pathname = malloc(strlen(env_path)+strlen(exec)+2);
  if (!pathname)  return NULL;
  strcpy(pathname, env_path);

  start = end = env_path;
  while (*end) {
    end = strchrnul(start, ':');
    if (start[0] == '/') {
      int span = (int) (end-start);
      strncpy(pathname, start, span);
      if (pathname[span-1] != '/')
        pathname[span++] = '/';
      strcpy(pathname+span, exec);
      if (check_exec(pathname))
        return pathname;
    }
    start = end+1;
  }
  return NULL;
}
