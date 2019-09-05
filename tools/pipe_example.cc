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

void client(int readfd, int writefd) {
  size_t len;
  ssize_t n;

  char buff[MAXLIEN];
  fgets(buff, MAXLIEN, stdin);
  len = strlen(buff);

  if (buff[len - 1] == '\n')
    len--;

  write(writefd, buff, len);

  while ((n = read(readfd, buff, MAXLIEN)) > 0) {
    write(STDOUT_FILENO, buff, n);
  }
}

void server(int readfd, int writefd) {
  int fd;
  ssize_t n;
  char buff[MAXLIEN];

  if ((n = read(readfd, buff, MAXLIEN)) == 0) {
    std::cout << "file = " << __FILE__ << ", line = " << __LINE__ << ", error = " << strerror(errno) << std::endl;
    exit(-1);
  }
  buff[n] = '\0';

  if ((fd = open(buff, O_RDONLY)) < 0) {
    /* error must tell client */
    snprintf(buff + n, sizeof(buff) - n, ":can't open, %s\n", strerror(errno));
    n = strlen(buff);
    write(writefd, buff, n);
  } else {
    /* open successed: copy the file to IPC channel */
    while ((n = read(fd, buff, MAXLIEN)) > 0) {
      write(writefd, buff, n);
    }
    close(fd);
  }
}

int main() {
  int pipe1[2], pipe2[2];
  pid_t child_pid;

  pipe(pipe1);
  pipe(pipe2);

  // child  (pipe1[0] read)  (pipe2[1] write)
  // server (pipe1[1] write) (pipe2[0] read)
  if ((child_pid = fork()) == 0) {  /* child */
    close(pipe1[1]);
    close(pipe2[0]);

    server(pipe1[0], pipe2[1]);
    exit(0);
  }

  /* parent */
  close(pipe1[0]);
  close(pipe2[1]);
  client(pipe2[0], pipe1[1]);

  waitpid(child_pid, NULL, child_pid);
  exit(0);
}
