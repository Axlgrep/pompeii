#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include <gtest/gtest.h>
#include <hiredis.h>

static const char *hostname = "127.0.0.1";
static const int port = 18889;
static redisContext *ctx;
static redisReply *res;

class PipelineTest : public ::testing::Test {
 public:
  PipelineTest() {}
  virtual ~PipelineTest() {}

  static void SetUpTestCase();
  static void TearDownTestCase();

  virtual void SetUp();
  virtual void TearDown() {}
};

void PipelineTest::SetUpTestCase() {
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

void PipelineTest::TearDownTestCase() {
  redisFree(ctx);
}

void PipelineTest::SetUp() {
  res = (redisReply*)redisCommand(ctx, "SELECT 0");
  if (strcasecmp("OK", res->str)) {
    fprintf(stderr, "Select db error: %s, exit...\n", res->str);
    exit(1);
  }
  freeReplyObject(res);
}

TEST_F(PipelineTest, PIPELINE_CASE_1) {
  redisAppendCommand(ctx, "DEL 06S");
  redisAppendCommand(ctx, "HSET 06S f1 v1 f2 v2");

  redisGetReply(ctx, reinterpret_cast<void**>(&res));
  ASSERT_EQ(REDIS_REPLY_INTEGER, res->type);
  ASSERT_EQ(0, res->integer);

  redisGetReply(ctx, reinterpret_cast<void**>(&res));
  ASSERT_EQ(REDIS_REPLY_INTEGER, res->type);
  ASSERT_EQ(2, res->integer);

  res = (redisReply*)redisCommand(ctx, "DEL 06S");
  freeReplyObject(res);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
