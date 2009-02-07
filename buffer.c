// Time-stamp: <2009-02-07 13:40:36 cklin>

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include "middleman.h"

void *xmalloc(size_t size)
{
  void *ptr = malloc(size);
  if (size > 0 && ptr == NULL)
    errx(100, "Memory allocation failure (%u bytes)", size);
  return ptr;
}

char *path_join(const char *path, const char *name)
{
  int path_len, name_len;
  char *pathname;

  if (path == NULL || name == NULL)
    errx(101, "NULL argument in path_join");
  if (path[0] != '/')
    errx(102, "Relative path argument 1 in path_join");
  if (strchr(name, '/'))
    errx(103, "Relative path argument 2 in path_join");

  path_len = strlen(path);
  name_len = strlen(name);
  pathname = xmalloc(path_len+name_len+2);

  strcpy(pathname, path);
  if (pathname[path_len-1] != '/')
    pathname[path_len++] = '/';
  strcpy(pathname+path_len, name);
  return pathname;
}

void release_job(job *job)
{
  free(job->cwd);
  free(job->cmd.buffer);
  free(job->cmd.svec);
  free(job->env.buffer);
  free(job->env.svec);
}
