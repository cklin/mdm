// Time-stamp: <2009-01-31 19:11:15 cklin>

#include <err.h>
#include <unistd.h>
#include <string.h>
#include "middleman.h"

int write_args(int fd, const char *args[])
{
  int index, size;

  for (index=0, size=0; args[index]; index++, size++)
    size += strlen(args[index]);
  if (size > MAX_ARG_SIZE) {
    warnx("write_args: args (%d bytes) is too long", size);
    return -1;
  }

  writen(fd, &size, sizeof (int));
  for (index=0; args[index]; index++)
    writen(fd, args[index], strlen(args[index])+1);
  return 0;
}

int read_args(int fd, struct argv *args)
{
  int size;

  size = read_block(fd, args->buffer);
  if (size > 0)
    unpack_args(args->buffer, size, args->args);
  return size;
}

int write_string(int fd, const char buffer[])
{
  int size;

  size = strlen(buffer)+1;
  writen(fd, &size, sizeof (int));
  writen(fd, buffer, size);
  return size;
}

int read_block(int fd, char buffer[])
{
  int size = 0;

  readn(fd, &size, sizeof (int));
  if (size > MAX_ARG_SIZE) {
    warnx("read_block: input (%d bytes) is too big", size);
    while (size > MAX_ARG_SIZE) {
      readn(fd, &buffer, MAX_ARG_SIZE);
      size -= MAX_ARG_SIZE;
    }
    return -1;
  }
  readn(fd, buffer, size);
  if (buffer[size-1]) {
    warnx("read_block: input is not null-terminated");
    return -2;
  }
  return size;
}

int unpack_args(char buffer[], int size, char *args[])
{
  int index, arg;

  index = arg = 0;
  args[arg++] = buffer;
  while (index < size) {
    if (arg >= MAX_ARG_COUNT) {
      warnx("unpack_args: too many segments");
      return -1;
    }
    if (buffer[index++])  continue;
    args[arg++] = buffer+index;
  }
  args[--arg] = NULL;
  return arg;
}
