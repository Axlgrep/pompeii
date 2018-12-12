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

  {
    size_t MAX_BATCH_LIMIT = 10000000;
    const char* argv[3] = {"SET", NULL, NULL};
    size_t argv_len[3] = {3, 0, 0};
    for (int32_t idx = 0; idx < MAX_BATCH_LIMIT; ++idx) {
      std::string key = "key_" +  std::to_string(idx);
      std::string value = "value_" + std::to_string(idx);
      argv[1] = key.data(), argv_len[1] = key.size();
      argv[2] = value.data(), argv_len[2] = value.size();

      if (redisAppendCommandArgv(c,
                                 3,
                                 reinterpret_cast<const char**>(argv),
                                 reinterpret_cast<const size_t*>(argv_len)) == REDIS_ERR) {
        printf("Redis Append Command Argv Error, exit...\n");
        exit(-1);
      } else {
        printf("success Append Command, Index: %d\n", idx);
      }
    }

    for (int32_t idx = 0; idx < MAX_BATCH_LIMIT; ++idx) {
      if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
        printf("Redis Get Reply Error, exit...\n");
        exit(-1);
      } else {
        if (res == NULL) {
          printf("Redis Get Reply Error, Index: %d, Reply: NULL, exit...\n", idx);
          exit(-1);
        } else {
          printf("success Get Reply, Index: %d, Reply: %s\n", idx, res->str);
        }
        freeReplyObject(res);
      }
    }
  }

  for (int32_t sidx = 0; sidx < 10000; ++sidx) {
    const char* set_argv[3];
    size_t set_argvlen[3];
    set_argv[0] = "set"; set_argvlen[0] = 3;
    for (int32_t idx = 0; idx < 1000; ++idx) {
      std::string key = std::to_string(sidx) + "_key_" + std::to_string(idx);
      std::string value = "value_" + std::to_string(idx);
      set_argv[1] = key.c_str(); set_argvlen[1] = key.size();
      set_argv[2] = value.c_str(); set_argvlen[2] = value.size();

      res = reinterpret_cast<redisReply*>(redisCommandArgv(c,
                                                           3,
                                                           reinterpret_cast<const char**>(set_argv),
                                                           reinterpret_cast<const size_t*>(set_argvlen)));
      if (res == NULL) {
        printf("set %s %s, Redis command argv error\n", key.c_str(), value.c_str());
      } else {
        printf("set %s %s, res : %s Redis command argv success\n", key.c_str(), value.c_str(), res->str);
      }
      freeReplyObject(res);
    }

    // sets
    const char* sadd_argv[3];
    size_t sadd_argvlen[3];
    sadd_argv[0] = "sadd"; sadd_argvlen[0] = 4;
    for (int32_t idx = 0; idx < 10; idx++) {
      std::string key = std::to_string(sidx) + "_set_key_" + std::to_string(idx);
      sadd_argv[1] = key.c_str(), sadd_argvlen[1] = key.size();
      for (int32_t sidx = 0; sidx < 1000; ++sidx) {
        std::string member = "set_member_" + std::to_string(sidx);
        sadd_argv[2] = member.c_str(), sadd_argvlen[2] = member.size();
        res = reinterpret_cast<redisReply*>(redisCommandArgv(c,
                                                             3,
                                                             reinterpret_cast<const char**>(sadd_argv),
                                                             reinterpret_cast<const size_t*>(sadd_argvlen)));
        if (res == NULL) {
          printf("sadd %s %s, Redis command argv error\n", key.c_str(), member.c_str());
        } else {
          printf("sadd %s %s, res : %s Redis command argv success\n", key.c_str(), member.c_str(), res->str);
        }
        freeReplyObject(res);
      }
    }

    // zsets
    const char* zadd_argv[4];
    size_t zadd_argvlen[4];
    zadd_argv[0] = "zadd"; zadd_argvlen[0] = 4;
    for (int32_t idx = 0; idx < 10; idx++) {
      std::string key = std::to_string(sidx) + "_zset_key_" + std::to_string(idx);
      zadd_argv[1] = key.c_str(), zadd_argvlen[1] = key.size();
      for (int32_t sidx = 0; sidx < 1000; ++sidx) {
        std::string score = std::to_string(sidx);
        std::string member = "set_member_" + std::to_string(sidx);
        zadd_argv[2] = score.c_str(), zadd_argvlen[2] = score.size();
        zadd_argv[3] = member.c_str(), zadd_argvlen[3] = member.size();
        res = reinterpret_cast<redisReply*>(redisCommandArgv(c,
                                                             4,
                                                             reinterpret_cast<const char**>(zadd_argv),
                                                             reinterpret_cast<const size_t*>(zadd_argvlen)));
        if (res == NULL) {
          printf("zadd %s %s %s, Redis command argv error\n", key.c_str(), score.c_str(), member.c_str());
        } else {
          printf("zadd %s %s %s, res : %s Redis command argv success\n", key.c_str(), score.c_str(), member.c_str(), res->str);
        }
        freeReplyObject(res);
      }
    }

    // hashes
    const char* hset_argv[4];
    size_t hset_argvlen[4];
    hset_argv[0] = "hset"; hset_argvlen[0] = 4;
    for (int32_t idx = 0; idx < 10; idx++) {
      std::string key = std::to_string(sidx) + "_hset_key_" + std::to_string(idx);
      hset_argv[1] = key.c_str(), hset_argvlen[1] = key.size();
      for (int32_t sidx = 0; sidx < 1000; ++sidx) {
        std::string field = "hash_field_" + std::to_string(sidx);
        std::string member = "hash_value_" + std::to_string(sidx);
        hset_argv[2] = field.c_str(), hset_argvlen[2] = field.size();
        hset_argv[3] = member.c_str(), hset_argvlen[3] = member.size();
        res = reinterpret_cast<redisReply*>(redisCommandArgv(c,
                                                             4,
                                                             reinterpret_cast<const char**>(hset_argv),
                                                             reinterpret_cast<const size_t*>(hset_argvlen)));
        if (res == NULL) {
          printf("sadd %s %s, Redis command argv error\n", key.c_str(), member.c_str());
        } else {
          printf("sadd %s %s, res : %s Redis command argv success\n", key.c_str(), member.c_str(), res->str);
        }
        freeReplyObject(res);
      }
    }

    // lists
    const char* rpush_argv[3];
    size_t rpush_argvlen[3];
    rpush_argv[0] = "rpush"; rpush_argvlen[0] = 5;
    for (int32_t idx = 0; idx < 10; idx++) {
      std::string key = std::to_string(sidx) + "_list_key_" + std::to_string(idx);
      rpush_argv[1] = key.c_str(), rpush_argvlen[1] = key.size();
      for (int32_t sidx = 0; sidx < 1000; ++sidx) {
        std::string node = "list_node_" + std::to_string(sidx);
        rpush_argv[2] = node.c_str(), rpush_argvlen[2] = node.size();
        res = reinterpret_cast<redisReply*>(redisCommandArgv(c,
                                                             3,
                                                             reinterpret_cast<const char**>(rpush_argv),
                                                             reinterpret_cast<const size_t*>(rpush_argvlen)));
        if (res == NULL) {
          printf("rpush %s %s, Redis command argv error\n", key.c_str(), node.c_str());
        } else {
          printf("rpush %s %s, res : %s Redis command argv success\n", key.c_str(), node.c_str(), res->str);
        }
        freeReplyObject(res);
      }
    }
  }
  redisFree(c);
  return 0;
}
