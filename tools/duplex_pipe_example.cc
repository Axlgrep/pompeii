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

int main(int argc, char** argv) {
  int fd[2], n;
  char c;
  pid_t child_pid;

  pipe(fd);
  if ((child_pid = fork()) == 0) { /* child */
    sleep(3);

    if ((n = read(fd[0], &c, 1)) != 1) {
      std::cout << "file = " << __FILE__ << ", line = " << __LINE__ << ", error = " << strerror(errno) << std::endl;
      exit(-1);
    }
    printf("child read %c\n", c);
    write(fd[0], "c", 1);
  }

  /* parent */
  write(fd[1], "p", 1);
  if ((n = read(fd[1], &c, 1)) != 1) {
    std::cout << "file = " << __FILE__ << ", line = " << __LINE__ << ", error = " << strerror(errno) << std::endl;
    exit(-1);
  }
  printf("parent read %c\n", c);
  exit(0);
}
