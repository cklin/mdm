// Time-stamp: <2009-02-09 01:51:22 cklin>

#include <assert.h>
#include <err.h>
#include <ctype.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "middleman.h"

#define IOS_MAX  4096
#define UTIL_MAX 4096
#define BUF_MAX  1024

typedef struct {
  char *name;
  char *resources;
  int  res_count;
} iospec;

static void read_iospec(const char *spec, iospec *ios)
{
  int  count;
  bool new_token;
  char *cp;

  ios->resources = NULL;
  ios->name = xstrdup(spec);

  new_token = false;
  for (cp=ios->name, count=0; *cp; cp++) {
    if (isspace(*cp)) {
      new_token = true;
      *cp = '\0';
      continue;
    }
    if (new_token) {
      if (ios->resources == NULL)
        ios->resources = cp;
      new_token = false;
      count++;
    }
  }
  ios->res_count = count;
}

static iospec iost[IOS_MAX];
static int    ios_count;

void init_iospec(const char *config_name)
{
  char buffer[BUF_MAX];
  FILE *config;

  config = fopen(config_name, "r");
  if (config == NULL)
    err(120, "Cannot open config file %s", config_name);

  ios_count = 0;
  while (fgets(buffer, BUF_MAX, config)) {
    if (strlen(buffer) == BUF_MAX-1)
      err(121, "Line too long in config file");
    if (buffer[0] == '#')
      continue;
    if (isspace(buffer[0]))
      errx(122, "Line begins with space");
    if (ios_count == IOS_MAX)
      errx(123, "Too many lines in config file");
    read_iospec(buffer, &(iost[ios_count++]));
  }
  fclose(config);
}

static iospec *find_iospec(const char *name)
{
  int index;

  for (index=0; index<ios_count; index++)
    if (strcmp(name, iost[index].name) == 0)
      return iost+index;
  return NULL;
}

static char calc_usage(const char *opt, iospec *ios)
{
  int  count = 0;
  bool new_token = true;
  char *cp = ios->resources;

  for ( ; count < ios->res_count; cp++) {
    if (*cp == '\0') {
      new_token = true;
      continue;
    }
    if (new_token) {
      if (strcmp(opt, cp+1) == 0)
        return *cp;
      new_token = false;
      count++;
    }
  }
  return '-';
}

typedef struct {
  char usage;
  char *name;
  int  count;
} util;

static util utit[UTIL_MAX];
static int  uti_count;

static bool conflict(char c1, char c2)
{
  return (c1 == 'W' || c1 != c2);
}

static bool check_conflict(char usage, const char *name)
{
  int index;

  for (index=0; index<uti_count; index++)
    if (strcmp(name, utit[index].name) == 0)
      if (conflict(usage, utit[index].usage))
        return true;
  return false;
}

static void add_uti(char usage, const char *name)
{
  int index;

  for (index=0; index<uti_count; index++)
    if (strcmp(name, utit[index].name) == 0)
      if (usage == utit[index].usage)
        break;

  if (index == UTIL_MAX)
    errx(124, "Utilization table is full");

  if (index < uti_count)
    utit[index].count++;
  else {
    utit[index].usage = usage;
    utit[index].name = xstrdup(name);
    utit[index].count = 1;
    uti_count++;
  }
}

static void del_uti(char usage, const char *name)
{
  int index;

  for (index=0; index<uti_count; index++)
    if (strcmp(name, utit[index].name) == 0)
      if (usage == utit[index].usage)
        break;

  assert(index < uti_count);
  if (--utit[index].count == 0) {
    free(utit[index].name);
    utit[index] = utit[--uti_count];
  }
}

bool register_job(sv *cmd)
{
  char   *name, *base;
  iospec *ios;
  char   *opt, **ptr, usage, nul[] = "";

  name = xstrdup(cmd->svec[0]);
  base = basename(name);
  ios = find_iospec(base);
  if (ios == NULL)  return true;

  opt = nul;
  for (ptr = cmd->svec+1; *ptr; ptr++) {
    if (*ptr[0] == '-')
      opt = *ptr;
    else {
      usage = calc_usage(opt, ios);
      if (usage != '-')
        if (check_conflict(usage, *ptr))
          return false;
      opt = nul;
    }
  }

  opt = nul;
  for (ptr = cmd->svec; *ptr; ptr++) {
    if (*ptr[0] == '-')
      opt = *ptr;
    else {
      usage = calc_usage(opt, ios);
      if (usage != '-')
        add_uti(usage, *ptr);
      opt = nul;
    }
  }
  return true;
}

void unregister_job(sv *cmd)
{
  char   *name, *base;
  iospec *ios;
  char   *opt, **ptr, usage, nul[] = "";

  name = xstrdup(cmd->svec[0]);
  base = basename(name);
  ios = find_iospec(base);
  if (ios == NULL)  return;

  opt = nul;
  for (ptr = cmd->svec; *ptr; ptr++) {
    if (*ptr[0] == '-')
      opt = *ptr;
    else {
      usage = calc_usage(opt, ios);
      if (usage != '-')
        del_uti(usage, *ptr);
      opt = nul;
    }
  }
  return;
}
