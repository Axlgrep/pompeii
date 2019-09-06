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

static int  read_cnt;
static char *read_ptr;
static char read_buf[MAXLINE];

static ssize_t my_read(int fd, char *ptr) {

  if (read_cnt <= 0) {
again:
    if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
      if (errno == EINTR)
        goto again;
      return(-1);
    } else if (read_cnt == 0)
      return(0);
    read_ptr = read_buf;
  }

  read_cnt--;
  *ptr = *read_ptr++;
  return(1);
}

ssize_t readline(int fd, void *vptr, size_t maxlen) {
  ssize_t n, rc;
  char  c, *ptr;

  ptr = static_cast<char*>(vptr);
  for (n = 1; n < maxlen; n++) {
    if ( (rc = my_read(fd, &c)) == 1) {
      *ptr++ = c;
      if (c == '\n')
        break;  /* newline is stored, like fgets() */
    } else if (rc == 0) {
      *ptr = 0;
      return(n - 1);  /* EOF, n - 1 bytes were read */
    } else
      return(-1);   /* error, errno set by read() */
  }

  *ptr = 0; /* null terminate like fgets() */
  return(n);
}

ssize_t Readline(int fd, void *ptr, size_t maxlen) {
  ssize_t n;
  if ( (n = readline(fd, ptr, maxlen)) < 0) {
    std::cout << "file = " << __FILE__ << ", line = " << __LINE__ << ", error = " << strerror(errno) << std::endl;
    exit(-1);
  }
  return(n);
}

int main(int argc, char **argv) {
  int readfifo, writefifo, dummyfd, fd;
  char *ptr, buff[MAXLINE + 1], fifoname[MAXLINE];
  pid_t pid;
  ssize_t n;

  /* create server's well-known FIFO; OK if already exists */
  if ((mkfifo(SERV_FIFO, FILE_MODE) < 0) && (errno != EEXIST)) {
    std::cout << "file = " << __FILE__ << ", line = " << __LINE__ << ", error = " << strerror(errno) << std::endl;
    exit(-1);
  }

  /* open server's well-known FIFO for reading and writing */
  readfifo = open(SERV_FIFO, O_RDONLY, 0);
  dummyfd = open(SERV_FIFO, O_WRONLY, 0);       /* never used */

  // pid filepath
  while ((n = readline(readfifo, buff, MAXLINE)) > 0) {
    if (buff[n - 1] == '\n')
      n--;
    buff[n] = '\0';

    if ((ptr = strchr(buff, ' ')) == NULL) {
      std::cout << "bogus request: " << buff << std::endl;
      continue;
    }

    *ptr++ = 0;    /* null terminate PID, ptr = pathname */
    pid = atol(buff);
    snprintf(fifoname, sizeof(fifoname), "./fifo.%ld", (long)pid);
    if ((writefifo = open(fifoname, O_WRONLY, 0)) < 0) {
      std::cout << "cannot open: " << fifoname << std::endl;
      continue;
    }
    if ((fd = open(ptr, O_RDONLY)) < 0) {
      /* error: must tell client */
      snprintf(buff + n, sizeof(buff) - n, ":can't open, %s\n", strerror(errno));
      n = strlen(ptr);
      write(writefifo, ptr, n);
      close(writefifo);
    } else {
      while ((n = read(fd, buff, MAXLINE)) > 0) {
        write(writefifo, buff, n);
      }
      close(fd);
      close(writefifo);
    }
  }
  exit(0);
}

