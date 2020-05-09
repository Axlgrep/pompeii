#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "iostream"

#include <hiredis.h>

using namespace std;

static const char *hostname = "127.0.0.1";
static const int port = 18889;
static redisContext *ctx;
static redisReply *res;

void ConnectRedis() {
  struct timeval timeout = { 1, 500000 }; // 1.5 seconds
  ctx = redisConnectWithTimeout(hostname, port, timeout);
  if (ctx == NULL || ctx->err) {
    if (ctx) {
      fprintf(stderr, "Connection error: %s\n", ctx->errstr);
      redisFree(ctx);
    } else {
      fprintf(stderr, "Connection error: can't allocate redis context\n");
    }
    exit(1);
  }
}

int main() {
  printf("start time: %d\n", time(NULL));
  FILE *fp = NULL;
  fp = fopen("./10131.log", "r");
  if (fp == NULL) {
    fprintf(stderr, "open file error, exit...\n");
    return -1;
  }

  ConnectRedis();

  int count = 0;
  uint64_t line = 0;
  char *ptr, *pEnd;
  char buff[1024];
  int argc, len;
  string argv[1024];
  while (true) {
    ptr = fgets(buff, 1024, (FILE*)fp);
    line++;
    if (ptr == NULL) {
      fprintf(stderr, "eof, exit...\n");
      freeReplyObject(res);
      redisFree(ctx);
      break;
    }
    if (strlen(buff) == 0) {
      continue;
    }
    if (buff[0] == '*') {
      count++;
      int idx = 0;
      argc = strtoll(buff + 1, &pEnd, 10);
      while (idx < argc) {
        ptr = fgets(buff, 1024, (FILE*)fp);
        line++;
        if (ptr == NULL || strlen(buff) == 0 || buff[0] != '$') {
          fprintf(stderr, "parse error(ptr null: %d), exit...\n", ptr == NULL);
          return -1;
        }
        len = strtoll(buff + 1, &pEnd, 10);
        if (len == 0) {
          fprintf(stderr, "parse argv len(%d) error, exit...\n", len);
        }
        ptr = fgets(buff, 1024, (FILE*)fp);
        line++;
        if (ptr == NULL || strlen(buff) == 0) {
          fprintf(stderr, "parse argv error, exit...\n");
        }
        argv[idx++] = std::string(buff, buff + len);
      }

      std::string command = argv[0];
      for (size_t i = 1; i < argc; i++) {
        command += " " + argv[i];
      }

      res = (redisReply*)redisCommand(ctx, command.data());
      if (res == NULL) {
        fprintf(stderr, "res NULL, exit...\n");
        return -1;
      }

      if (res->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "res error: %s, command: %s, exit...\n", res->str, command.data());
        return -1;
      }

      if (!strcasecmp(argv[0].data(), "multi") || !strcasecmp(argv[0].data(), "exec") || !strcasecmp(argv[0].data(), "discard")) {
        printf("time: %d, multi command: %s\n", time(NULL), command.data());
        fflush(stdout);
      }
      freeReplyObject(res);

      if (count % 100000 == 0){ printf("time: %d, process %d command, %llu line\n", time(NULL), count, line); fflush(stdout);}

      //printf("argc: %d\n", argc);
      //printf("%s\n", command.data());
    }
  }
  fclose(fp);
  return 0;
}
