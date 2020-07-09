#include <stdio.h>
#include <string.h>

#include <thread>
#include <sstream>
#include <iostream>

#include <hiredis.h>

static const char *hostname = "127.0.0.1";
static const int port = 18889;
std::string channel = "test_channel";

void PublishThread() {

  struct timeval timeout = { 1, 500000 }; // 1.5 seconds
  redisContext* ctx = redisConnectWithTimeout(hostname, port, timeout);

  if (ctx == NULL || ctx->err) {
    if (ctx) {
      fprintf(stderr, "Connection error: %s\n", ctx->errstr);
      redisFree(ctx);
    } else {
      fprintf(stderr, "Connection error: can't allocate redis context\n");
    }
    exit(1);
  }

  size_t idx = 0;
  std::stringstream ss;
  ss<<"publish"<<" "<<channel<<" "<<"message";
  std::string publish_command = ss.str();

  while (true) {
    redisReply* res = (redisReply*)redisCommand(ctx, publish_command.data());
    freeReplyObject(res);
  }
}

int main() {
  std::thread t1(PublishThread);
  t1.join();
}
