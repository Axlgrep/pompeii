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

#define MAXLINE 1000
#define SERV_FIFO  "./serv_fifo"
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

int main(int argc, char **argv) {
  int readfifo, writefifo;
  size_t len;
  ssize_t n;
  char *ptr, fifoname[MAXLINE], buff[MAXLINE];
  pid_t pid;

  /* create FIFO with our PID as part of name */
  pid = getpid();
  snprintf(fifoname, sizeof(fifoname), "./fifo.%ld", (long)pid);
  if ((mkfifo(fifoname, FILE_MODE) < 0) && (errno != EEXIST)) {
    std::cout << "file = " << __FILE__ << ", line = " << __LINE__ << ", error = " << strerror(errno) << std::endl;
    exit(-1);
  }

  snprintf(buff, sizeof(buff), "%ld ", (long)pid);
  len = strlen(buff);
  ptr = buff + len;

  fgets(ptr, MAXLINE - len, stdin);
  len = strlen(buff);            /* fgets() guarantees null byte at end */

  /* open FIFO to server and write PID and pathname to FIFO */
  writefifo = open(SERV_FIFO, O_WRONLY, 0);
  write(writefifo, buff, len);

  /* now open our FIFO, blocks untill server opens for writing */
  readfifo = open(fifoname, O_RDONLY, 0);

  /* read from IPC, write to standard output */
  while ((n = read(readfifo, buff, MAXLINE)) > 0) {
    write(STDOUT_FILENO, buff, n);
  }
  close(readfifo);
  unlink(fifoname);
  exit(0);
}
