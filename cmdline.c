// Time-stamp: <2008-12-23 18:05:04 cklin>

#include <err.h>
#include <unistd.h>
#include <string.h>
#include "bounds.h"
#include "comms.h"

int write_cmd(int fd, const char *cmd[])
{
  int index, size;

  for (index=0, size=0; cmd[index]; index++, size++)
    size += strlen(cmd[index]);
  if (size > MAX_ARG_SIZE) {
    warnx("write_cmd: cmd (%d bytes) is too big", size);
    return -1;
  }

  writen(fd, &size, sizeof (int));
  for (index=0; cmd[index]; index++)
    writen(fd, cmd[index], strlen(cmd[index])+1);
  return 0;
}

int read_cmd(int fd, char buffer[])
{
  int size = 0;

  readn(fd, &size, sizeof (int));
  if (size > MAX_ARG_SIZE) {
    warnx("read_cmd: input (%d bytes) is too big", size);
    while (size > MAX_ARG_SIZE) {
      readn(fd, &buffer, MAX_ARG_SIZE);
      size -= MAX_ARG_SIZE;
    }
    return -1;
  }
  readn(fd, buffer, size);
  if (buffer[size-1]) {
    warnx("read_cmd: input is not null-terminated");
    return -2;
  }
  return size;
}

int cmd_pointers(char buffer[], int size, char *args[])
{
  int index, arg;

  index = arg = 0;
  args[arg++] = buffer;
  while (index < size) {
    if (arg >= MAX_ARG_COUNT) {
      warnx("cmd_pointers: too many arguments");
      return -1;
    }
    if (buffer[index++])  continue;
    args[arg++] = buffer+index;
  }
  args[--arg] = NULL;
  return arg;
}
