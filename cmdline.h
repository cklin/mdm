// Time-stamp: <2009-01-01 13:09:59 cklin>

#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#include "bounds.h"

struct argv {
  char buffer[MAX_ARG_SIZE];
  char *args[MAX_ARG_COUNT];
};

int write_args(int fd, const char *args[]);
int read_args(int fd, struct argv *args);

int write_string(int fd, const char buffer[]);
int read_block(int fd, char buffer[]);
int unpack_args(char buffer[], int size, char *args[]);

#endif
