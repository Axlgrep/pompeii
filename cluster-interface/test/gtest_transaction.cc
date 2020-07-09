#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>

#include <iostream>

#include <gtest/gtest.h>
#include <hiredis.h>

static const char *hostname = "127.0.0.1";
static const int port = 18889;
static redisContext *ctx;
static redisReply *res;

class TransactionTest : public ::testing::Test {
 public:
  TransactionTest() {}
  virtual ~TransactionTest() {}

  static void SetUpTestCase();
  static void TearDownTestCase();

  virtual void SetUp();
  virtual void TearDown() { }
};

void TransactionTest::SetUpTestCase() {
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

void TransactionTest::TearDownTestCase() {
  redisFree(ctx);
}

void TransactionTest::SetUp() {
  res = (redisReply*)redisCommand(ctx, "SELECT 0");
  if (strcasecmp("OK", res->str)) {
    fprintf(stderr, "Select db error: %s, exit...\n", res->str);
    exit(1);
  }
  freeReplyObject(res);
}

TEST_F(TransactionTest, MULTI_CASE_1) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_EQ(REDIS_REPLY_ERROR, res->type);
  ASSERT_STREQ("ERR MULTI calls can not be nested", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ARRAY, res->type);
  ASSERT_EQ(0, res->elements);
  freeReplyObject(res);
}

TEST_F(TransactionTest, MULTI_CASE_2) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_EQ(REDIS_REPLY_ERROR, res->type);
  ASSERT_STREQ("ERR MULTI calls can not be nested", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ARRAY, res->type);
  ASSERT_STREQ("OK", res->element[0]->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DEL 06S");
  freeReplyObject(res);
}

TEST_F(TransactionTest, DISCARD_CASE_1) {
  res = (redisReply*)redisCommand(ctx, "DISCARD");
  ASSERT_STREQ("ERR DISCARD without MULTI", res->str);
  freeReplyObject(res);
}

// multi -> discard
TEST_F(TransactionTest, DISCARD_CASE_2) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_EQ(REDIS_REPLY_STATUS, res->type);
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DISCARD");
  ASSERT_EQ(REDIS_REPLY_STATUS, res->type);
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);
}

// multi -> set Qi value -> discard
// get Qi -> expect Qi(NULL)
TEST_F(TransactionTest, DISCARD_CASE_3) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET Qi value");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DISCARD");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "GET Qi");
  ASSERT_EQ(REDIS_REPLY_NIL, res->type);
  freeReplyObject(res);
}

// multi -> select 1 -> discard
// set Qi value
// select 0 -> get Qi -> expect Qi(value)
TEST_F(TransactionTest, DISCARD_CASE_4) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SELECT 1");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DISCARD");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET Qi value");
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SELECT 0");
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "GET Qi");
  ASSERT_STREQ("value", res->str);
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "DEL Qi");
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SELECT 1");
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "GET Qi");
  ASSERT_EQ(REDIS_REPLY_NIL, res->type);
  freeReplyObject(res);
}

TEST_F(TransactionTest, WATCH_CASE_1) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "WATCH Qi");
  ASSERT_EQ(REDIS_REPLY_ERROR , res->type);
  ASSERT_STREQ("ERR WATCH inside MULTI is not allowed", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DISCARD");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);
}

TEST_F(TransactionTest, WATCH_CASE_2) {
  res = (redisReply*)redisCommand(ctx, "WATCH Qi");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(0, res->elements);
  freeReplyObject(res);
}

TEST_F(TransactionTest, WATCH_CASE_3) {
  res = (redisReply*)redisCommand(ctx, "WATCH Qi");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S value");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(1, res->elements);
  ASSERT_STREQ("OK", res->element[0]->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DEL 06S");
  freeReplyObject(res);
}

TEST_F(TransactionTest, WATCH_CASE_4) {
  res = (redisReply*)redisCommand(ctx, "WATCH Qi");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 6ZJ value");
  ASSERT_EQ(REDIS_REPLY_ERROR , res->type);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ERROR , res->type);
  ASSERT_STREQ("EXECABORT Transaction discarded because of previous errors.", res->str);
  freeReplyObject(res);
}

TEST_F(TransactionTest, WATCH_CASE_5) {
  res = (redisReply*)redisCommand(ctx, "WATCH Qi");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "INFO");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(1, res->elements);
  ASSERT_EQ(REDIS_REPLY_STRING, res->element[0]->type);
  freeReplyObject(res);
}

TEST_F(TransactionTest, WATCH_CASE_6) {
  res = (redisReply*)redisCommand(ctx, "WATCH Qi");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "AXL");
  ASSERT_STREQ("ERR unknown command 'axl'", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET Qi value");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ERROR , res->type);
  ASSERT_STREQ("EXECABORT Transaction discarded because of previous errors.", res->str);
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_1) {
  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_STREQ("ERR EXEC without MULTI", res->str);
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_2) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "INFO");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_STREQ("ERR EXEC failed, because of routing failure", res->str);
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_3) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "INFO");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(2, res->elements);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DEL 06S");
  freeReplyObject(res);

}

TEST_F(TransactionTest, EXEC_CASE_4) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET Qi v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ARRAY, res->type);
  ASSERT_STREQ("OK", res->element[0]->str);
  ASSERT_STREQ("OK", res->element[1]->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DEL 06S Qi");
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_5) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "AXL");
  ASSERT_STREQ("ERR unknown command 'axl'", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ERROR, res->type);
  ASSERT_STREQ("EXECABORT Transaction discarded because of previous errors.", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "Get 06S");
  ASSERT_EQ(REDIS_REPLY_NIL, res->type);
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_6) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET Qi");
  ASSERT_STREQ("ERR wrong number of arguments for 'set' command", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ERROR, res->type);
  ASSERT_STREQ("EXECABORT Transaction discarded because of previous errors.", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "Get 06S");
  ASSERT_EQ(REDIS_REPLY_NIL, res->type);
  freeReplyObject(res);
}

// make sure 'shutdown' in disabled command list
TEST_F(TransactionTest, EXEC_CASE_7) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SHUTDOWN");
  ASSERT_STREQ("ERR shutdown is disabled command", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ERROR, res->type);
  ASSERT_STREQ("EXECABORT Transaction discarded because of previous errors.", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "Get 06S");
  ASSERT_EQ(REDIS_REPLY_NIL, res->type);
  freeReplyObject(res);
}

// we test in a MULTI context, exec local command(client list)
TEST_F(TransactionTest, EXEC_CASE_8) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "CLIENT LIST");
  ASSERT_STREQ("ERR client inside MULTI is not allowed", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ERROR, res->type);
  ASSERT_STREQ("EXECABORT Transaction discarded because of previous errors.", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "Get 06S");
  ASSERT_EQ(REDIS_REPLY_NIL, res->type);
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_9) {
  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MSET 06S v Qi v");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXISTS 06S Qi");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MGET 06S Qi");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DEL 06S Qi");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "UNLINK 06S Qi");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ARRAY, res->type);
  ASSERT_EQ(5, res->elements);
  ASSERT_STREQ("OK", res->element[0]->str);
  ASSERT_EQ(2, res->element[1]->integer);
  ASSERT_STREQ("v", res->element[2]->element[0]->str);
  ASSERT_STREQ("v", res->element[2]->element[1]->str);
  ASSERT_EQ(2, res->element[3]->integer);
  ASSERT_EQ(0, res->element[4]->integer);
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_10) {
  res = (redisReply*)redisCommand(ctx, "SELECT 1");
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S db1");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SELECT 2");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SET 06S db2");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SELECT 3");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ARRAY, res->type);
  ASSERT_EQ(4, res->elements);

  res = (redisReply*)redisCommand(ctx, "SET 06S db3");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "SELECT 1");
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "GET 06S");
  ASSERT_STREQ("db1", res->str);
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "DEL 06S");

  res = (redisReply*)redisCommand(ctx, "SELECT 2");
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "GET 06S");
  ASSERT_STREQ("db2", res->str);
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "DEL 06S");

  res = (redisReply*)redisCommand(ctx, "SELECT 3");
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "GET 06S");
  ASSERT_STREQ("db3", res->str);
  freeReplyObject(res);
  res = (redisReply*)redisCommand(ctx, "DEL 06S");
}

TEST_F(TransactionTest, EXEC_CASE_11) {
  res = (redisReply*)redisCommand(ctx, "MSET 06S v Qi v");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MULTI");
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "MGET 06S Qi");
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "EXEC");
  ASSERT_EQ(REDIS_REPLY_ARRAY, res->type);
  ASSERT_EQ(1, res->elements);
  ASSERT_STREQ("v", res->element[0]->element[0]->str);
  ASSERT_STREQ("v", res->element[0]->element[1]->str);
  freeReplyObject(res);

  res = (redisReply*)redisCommand(ctx, "DEL 06S Qi");
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_12) {
  redisAppendCommand(ctx, "multi");
  redisAppendCommand(ctx, "get axl");
  //redisAppendCommand(ctx, "get key");
  redisAppendCommand(ctx, "exec");

  redisAppendCommand(ctx, "multi");
  redisAppendCommand(ctx, "get axl");
  //redisAppendCommand(ctx, "get key");
  redisAppendCommand(ctx, "exec");

  redisGetReply(ctx, reinterpret_cast<void**>(&res));
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  redisGetReply(ctx, reinterpret_cast<void**>(&res));
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  redisGetReply(ctx, reinterpret_cast<void**>(&res));
  ASSERT_EQ(REDIS_REPLY_ARRAY, res->type);
  freeReplyObject(res);

  redisGetReply(ctx, reinterpret_cast<void**>(&res));
  ASSERT_STREQ("OK", res->str);
  freeReplyObject(res);

  redisGetReply(ctx, reinterpret_cast<void**>(&res));
  ASSERT_STREQ("QUEUED", res->str);
  freeReplyObject(res);

  redisGetReply(ctx, reinterpret_cast<void**>(&res));
  ASSERT_EQ(REDIS_REPLY_ARRAY, res->type);
  freeReplyObject(res);
}

TEST_F(TransactionTest, EXEC_CASE_13) {
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

  ret = fcntl(clientfd, F_SETFL, fcntl(clientfd, F_GETFD, 0) | O_NONBLOCK);
  ASSERT_NE(ret, -1);

  std::string str1 = "*1\r\n$5\r\nmulti\r\n*2\r\n$3\r\nget\r\n$3\r\naxl\r\n*1\r\n$4\r\nexec";
  std::string str2 = "\r\n*1\r\n$5\r\nmulti\r\n*2\r\n$3\r\nget\r\n$3\r\naxl\r\n*1\r\n$4\r\nexec\r\n";

  int writeLen = write(clientfd, str1.data(), str1.size());
  ASSERT_EQ(writeLen, str1.size());
  sleep(1);
  writeLen = write(clientfd, str2.data(), str2.size());
  ASSERT_EQ(writeLen, str2.size());

  std::string reply;
  long long start = time(NULL);
  while (time(NULL) - start < 3) {
    int readLen = read(clientfd, buf, 1024);
    ASSERT_NE(readLen, 0);
    if (readLen == -1) {
      continue;
    }
    reply.append(buf, readLen);
  }
  close(clientfd);
  ASSERT_EQ("+OK\r\n+QUEUED\r\n*1\r\n$-1\r\n+OK\r\n+QUEUED\r\n*1\r\n$-1\r\n", reply);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
