// Time-stamp: <2009-03-11 22:48:46 cklin>

/*
   buffer.c - Middleman System String and Buffer Procedures

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

#include <assert.h>
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

char *xstrdup(const char *s)
{
  char *dup = strdup(s);
  if (dup == NULL)  err(104, "strdup");
  return dup;
}

void release_sv(sv *sv)
{
  free(sv->buffer);
  free(sv->svec);
  sv->buffer = NULL;
  sv->svec = NULL;
}

void release_job(job *job)
{
  release_sv(&(job->cmd));
  release_sv(&(job->env));
}

void flatten_sv(sv *sv)
{
  char *vp, *bp;
  int  index;

  assert(sv->buffer != NULL && sv->svec != NULL);
  assert(sv->svec[0] == sv->buffer);

  for (index=0, bp=sv->buffer; sv->svec[index]; index++) {
    vp = sv->svec[index];
    while (*vp)  *(bp++) = *(vp++);
    *(bp++) = ' ';
  }
  *(--bp) = '\0';
  free(sv->svec);
  sv->svec = NULL;
}
