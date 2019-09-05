//  Copyright (c) 2017-present The gilmour Authors.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/wait.h>
#include <sys/types.h>

#include "iostream"

#define MAXLIEN 1000

int main(int argc, char **argv) {
  size_t n;
  char buff[MAXLIEN], command[MAXLIEN];
  FILE *fp;

  /* read pathname */
  fgets(buff, MAXLIEN, stdin);
  n = strlen(buff);
  if (buff[n - 1] == '\n') {
    n--;                     /* delete newline from fgets() */
  }

  snprintf(command, sizeof(command), "cat %s", buff);
  fp = popen(command, "r");

  /* copy from pipe to standard output */
  while (fgets(buff, MAXLIEN, fp) != NULL) {
    fputs(buff, stdout);
  }
  pclose(fp);
  exit(0);
}
