#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "hiredis.h"
#include "async.h"
#include "adapters/libevent.h"

static const char *hostname = "127.0.0.1";
static const int port = 18889;

void onMessage(redisAsyncContext *c, void *reply, void *privdata) {
  redisReply *r = static_cast<redisReply*>(reply);
  if (reply == NULL) return;

  if (r->type == REDIS_REPLY_ARRAY) {
    for (int j = 0; j < r->elements; j++) {
      printf("%u) %s\n", j, r->element[j]->str);
    }
  }
}

int main (int argc, char **argv) {
  signal(SIGPIPE, SIG_IGN);
  struct event_base *base = event_base_new();

  redisAsyncContext *c = redisAsyncConnect(hostname, port);
  if (c->err) {
    printf("error: %s\n", c->errstr);
    return 1;
  }

  redisLibeventAttach(c, base);
  redisAsyncCommand(c, onMessage, NULL, "SUBSCRIBE test_channel");
  event_base_dispatch(base);
  return 0;
}
