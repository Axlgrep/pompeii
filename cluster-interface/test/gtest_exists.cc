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

class ExistsTest : public ::testing::Test {
 public:
  ExistsTest() {}
  virtual ~ExistsTest() {}

  static void SetUpTestCase();
  static void TearDownTestCase();

  virtual void SetUp();
  virtual void TearDown() {}
};

void ExistsTest::SetUpTestCase() {
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

void ExistsTest::TearDownTestCase() {
  redisFree(ctx);
}

void ExistsTest::SetUp() {
  res = (redisReply*)redisCommand(ctx, "SELECT 0");
  if (strcasecmp("OK", res->str)) {
    fprintf(stderr, "Select db error: %s, exit...\n", res->str);
    exit(1);
  }
  freeReplyObject(res);
}

TEST_F(ExistsTest, EXISTS_CASE_1) {
  res = (redisReply*)redisCommand(ctx, "EXISTS");
  ASSERT_EQ(REDIS_REPLY_ERROR, res->type);
  ASSERT_STREQ("ERR wrong number of arguments for 'exists' command", res->str);
  freeReplyObject(res);
}

TEST_F(ExistsTest, EXISTS_CASE_2) {
  res = (redisReply*)redisCommand(ctx, "EXISTS 06S 6ZJ");
  ASSERT_EQ(REDIS_REPLY_INTEGER, res->type);
  ASSERT_EQ(0, res->integer);
  freeReplyObject(res);
}

TEST_F(ExistsTest, EXISTS_CASE_3) {
  res = (redisReply*)redisCommand(ctx, "MSET 06S v");
  ASSERT_EQ(REDIS_REPLY_STATUS, res->type);
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXISTS 06S 6ZJ");
  ASSERT_EQ(REDIS_REPLY_INTEGER, res->type);
  ASSERT_EQ(1, res->integer);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DEL 06S");
  freeReplyObject(res);
}

TEST_F(ExistsTest, EXISTS_CASE_4) {
  res = (redisReply*)redisCommand(ctx, "MSET 06S v 6ZJ v");
  ASSERT_EQ(REDIS_REPLY_STATUS, res->type);
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXISTS 06S 6ZJ");
  ASSERT_EQ(REDIS_REPLY_INTEGER, res->type);
  ASSERT_EQ(2, res->integer);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DEL 06S 6ZJ");
  freeReplyObject(res);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
