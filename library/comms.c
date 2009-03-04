// Time-stamp: <2009-02-28 11:29:41 cklin>

/*
   comms.c - Middleman System Communications Procedures

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
#include <string.h>
#include <unistd.h>
#include "middleman.h"

// Read variable recorded-size block from file descriptor

static int read_block(int fd, char **buffer)
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

static int unpack_svec(sv *sv, int size)
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

static int write_string(int fd, const char buffer[])
{
  int size;

  size = strlen(buffer)+1;
  write_int(fd, size);
  writen(fd, buffer, size);
  return size;
}

// Write NULL-terminated string vector to file descriptor

int write_sv(int fd, char *const svec[])
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
  write_sv(fd, job->cmd.svec);
  write_sv(fd, job->env.svec);
}
