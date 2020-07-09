#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <iostream>

#include <gtest/gtest.h>
#include <hiredis.h>

//static const char *hostname = "127.0.0.1";
static const char *hostname = "9.134.74.156";
static const int port = 18889;
static redisContext *ctx;
static redisReply *res;

class SpecialTest : public ::testing::Test {
 public:
  SpecialTest() {}
  virtual ~SpecialTest() {}

  static void SetUpTestCase();
  static void TearDownTestCase();

  virtual void SetUp();
  virtual void TearDown() { }
};

void SpecialTest::SetUpTestCase() {
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

void SpecialTest::TearDownTestCase() {
  redisFree(ctx);
}

void SpecialTest::SetUp() {
  res = (redisReply*)redisCommand(ctx, "SELECT 0");
  if (strcasecmp("OK", res->str)) {
    fprintf(stderr, "Select db error: %s, exit...\n", res->str);
    exit(1);
  }
  freeReplyObject(res);
}

TEST_F(SpecialTest, SPECIAL_CASE_1) {
  int clientfd;
  struct hostent *hp;
  struct sockaddr_in serveraddr;
  char buf[1024];

  clientfd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_NE(clientfd, -1);

  hp = gethostbyname(hostname);
  int ret = (hp == NULL) ? -1 : 0;
  ASSERT_NE(ret, -1);

  bzero((char*)&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char*)hp->h_addr_list[0], (char*)&serveraddr.sin_addr.s_addr, hp->h_length);
  serveraddr.sin_port = htons(port);

  ret = connect(clientfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
  ASSERT_NE(ret, -1);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
