// Time-stamp: <2009-03-04 13:52:53 cklin>

/*
   hazard.c - Job Interference Decision Procedures

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

static bool iterate_spec(iospec *ios_arg, char **res)
{
  static iospec *ios = NULL;
  static int    count;
  static bool   new_token;
  static char   *cp;

  if (ios_arg) {
    ios = ios_arg;
    count = 0;
    new_token = true;
    cp = ios->resources;
    return false;
  }
  if (ios == NULL)
    return false;

  for ( ; count < ios->res_count; cp++) {
    if (*cp == '\0') {
      new_token = true;
      continue;
    }
    if (new_token) {
      new_token = false;
      count++;
      *res = cp;
      return true;
    }
  }
  return false;
}

static char calc_usage(const char *opt, iospec *ios)
{
  char *cp;

  iterate_spec(ios, NULL);
  while (iterate_spec(NULL, &cp))
    if (strcmp(opt, cp+1) == 0) {
      if (*cp == '0' && *opt == '-')
        return calc_usage("", ios);
      return *cp;
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

static void add_utilization(char usage, const char *name)
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

static void del_utilization(char usage, const char *name)
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

static bool iterate_usage(sv *cmd, char **ar, char *au)
{
  static char   *name, *base;
  static char   *opt, **ptr, *nul = "";
  static iospec *ios;
  static bool   fixed, initial;
  char          *spec;

  if (cmd) {
    free(name);
    name = xstrdup(cmd->svec[0]);
    base = basename(name);

    ios = find_iospec(base);
    opt = nul;
    ptr = cmd->svec;
    fixed = true;
    initial = true;
    if (ios)  iterate_spec(ios, NULL);
    return false;
  }
  assert(ar && au);

  if (fixed) {
    *ar = "global";
    *au = 'R';
    fixed = false;
    return true;
  }

  if (ios == NULL)  return false;

  if (initial) {
    while (iterate_spec(NULL, &spec))
      if (spec[1] != '-' && spec[1] != '\0') {
        *ar = spec+1;
        *au = *spec;
        return true;
      }
    initial = false;
  }

  while (*(++ptr))
    if (**ptr != '-') {
      char usage = calc_usage(opt, ios);
      opt = nul;
      if (usage != '-') {
        *ar = *ptr;
        *au = usage;
        return true;
      }
    }
    else opt = *ptr;

  ptr--;
  return false;
}

bool validate_job(sv *cmd)
{
  char *res, usage;

  assert(cmd->buffer);
  iterate_usage(cmd, NULL, NULL);
  while (iterate_usage(NULL, &res, &usage))
    if (check_conflict(usage, res))
      return false;
  return true;
}

void register_job(sv *cmd)
{
  char *res, usage;

  assert(cmd->buffer);
  iterate_usage(cmd, NULL, NULL);
  while (iterate_usage(NULL, &res, &usage))
    add_utilization(usage, res);
}

void unregister_job(sv *cmd)
{
  char *res, usage;

  assert(cmd->buffer);
  iterate_usage(cmd, NULL, NULL);
  while (iterate_usage(NULL, &res, &usage))
    del_utilization(usage, res);
}
