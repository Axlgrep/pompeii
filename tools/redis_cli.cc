#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <hiredis.h>

static const char *hostname = "127.0.0.1";
static const int port = 9221;
int main() {
  redisContext *c;
  redisReply *res;

  struct timeval timeout = { 1, 500000 }; // 1.5 seconds
  c = redisConnectWithTimeout(hostname, port, timeout);
  if (c == NULL || c->err) {
    if (c) {
      printf("Connection error: %s\n", c->errstr);
      redisFree(c);
    } else {
      printf("Connection error: can't allocate redis context\n");
    }
    exit(1);
  } else {
    printf("Connect success...\n");
  }

  const char* argv[4];
  size_t argvlen[4];
  argv[0] = "zadd"; argvlen[0] = 4;
  argv[1] = "zset"; argvlen[1] = 4;
  for (int32_t idx = 0; idx < 1000; ++idx) {
    std::string score = std::to_string(idx);
    std::string member = "mm_" + score;
    argv[2] = score.data(); argvlen[2] = score.size();
    argv[3] = member.data(); argvlen[3] = member.size();
    res = reinterpret_cast<redisReply*>(redisCommandArgv(c,
                                                         4,
                                                         reinterpret_cast<const char**>(argv),
                                                         reinterpret_cast<const size_t*>(argvlen)));
    if (res == NULL
      || res->type != REDIS_REPLY_INTEGER) {
      printf("idx = %d, Redis command argv error\n", idx);
    } else {
      printf("idx = %d, Redis command argv success\n", idx);
    }
    freeReplyObject(res);
  }
  redisFree(c);
  return 0;
}
