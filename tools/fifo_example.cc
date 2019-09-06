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
#include <sys/stat.h>
#include <sys/types.h>

#include "iostream"

#define MAXLIEN 1000
#define FIFO1 "./fifo.1"
#define FIFO2 "./fifo.2"
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

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

int main(int argc, char **argv) {
  int readfd, writefd;
  pid_t childpid;

  /* create two FIFOS, OK if they already exist */
  if ((mkfifo(FIFO1, FILE_MODE) < 0) && (errno != EEXIST)) {
    std::cout << "file = " << __FILE__ << ", line = " << __LINE__ << ", error = " << strerror(errno) << std::endl;
    exit(-1);
  }
  if ((mkfifo(FIFO2, FILE_MODE) < 0) && (errno != EEXIST)) {
    unlink(FIFO1);
    std::cout << "file = " << __FILE__ << ", line = " << __LINE__ << ", error = " << strerror(errno) << std::endl;
    exit(-1);
  }

  if ((childpid = fork()) == 0) {   /* child */
    readfd = open(FIFO1, O_RDONLY, 0);
    writefd = open(FIFO2, O_WRONLY, 0);

    server(readfd, writefd);
    exit(0);
  }

  /* parent */
  writefd = open(FIFO1, O_WRONLY, 0); // 如果当前没有任何进程打开某个FIFO来写，那么打开该FIFO来读
  readfd = open(FIFO2, O_RDONLY, 0);  // 的进程将会被阻塞，所以这里parent open FIFO1, FIFO2的次序
                                      // 不能颠倒

  client(readfd, writefd);
  waitpid(childpid, NULL, 0);

  close(readfd);
  close(writefd);

  unlink(FIFO1);
  unlink(FIFO2);
  exit(0);
}

