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

// 如果当前FIFO缓冲区中数据总量是100 Bytes, 那么创建一个
// readfd1，从中读取了30 Bytes之后再创建一个readfd2, 那么
// 可以读取剩余数据就是70 Bytes.
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

int main(int argc, char **argv) {
  int readfd, writefd;
  writefd = open(FIFO1, O_WRONLY, 0);
  readfd = open(FIFO2, O_RDONLY, 0);

  client(readfd, writefd);

  close(readfd);
  close(writefd);

  unlink(FIFO1);
  unlink(FIFO2);
  exit(0);
}

