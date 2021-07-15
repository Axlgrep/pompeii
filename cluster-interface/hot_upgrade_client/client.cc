#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#include <math.h>
#include <atomic>
#include <iostream>

#include <iostream>
#include <hiredis.h>

#define KEY_SIZE 18

#define PRESSURE_SET   1<<0
#define PRESSURE_GET   1<<1
#define PRESSURE_MSET  1<<2
#define PRESSURE_MGET  1<<3
#define PRESSURE_LPUSH 1<<4

#define PRESSURE_CUSTOM 1<<5

std::string hostname = "127.0.0.1";
size_t port = 18889;
std::string password = "";

size_t numreq = 1;
size_t data_size = 2;
size_t randomkeys_keyspacelen = 1;
size_t client_num = 10;

const size_t KEY_NUM = 100000000;
const size_t FIELD_NUM = 1000;
std::atomic<long long> total_request(0);
std::atomic<long long> total_request_old(0);

std::string GenRandomKey() {
  char randptr[KEY_SIZE];
  memcpy(randptr, "key:00000000000000", KEY_SIZE);

  size_t serial = random() % randomkeys_keyspacelen;
  for (size_t i = KEY_SIZE - 1; i > 4; i--) {
    randptr[i] = '0' + serial % 10;
    serial /= 10;
  }
  return std::string(randptr, KEY_SIZE);
}

std::string GenValue() {
  return std::string(data_size, 'x');
}

redisContext* createClient(const std::string& hostname, int port) {
  redisContext *c;

  struct timeval timeout = { 50, 500000 }; // 1.5 seconds
  c = redisConnectWithTimeout(hostname.data(), port, timeout);
  if (c == NULL || c->err) {
    if (c) {
      printf("Connection error: %s\n", c->errstr);
      redisFree(c);
    } else {
      printf("Connection error: can't allocate redis context\n");
    }
    return NULL;
  }

  if (!password.empty()) {
    redisReply *res;
    const char* auth_argv[2] = {"auth", password.data()};
    size_t auth_argv_len[2] = {4, password.size()};
    res = (redisReply*)redisCommandArgv(c,
                                        2,
                                        reinterpret_cast<const char**>(auth_argv),
                                        reinterpret_cast<const size_t*>(auth_argv_len));
    if (res->type == REDIS_REPLY_ERROR) {
      fprintf(stderr, "client auth failed: %s\n", res->str);
      freeReplyObject(res);
      exit(-1);
    }
    freeReplyObject(res);
  }
  return c;
}

#define MSET_MGET_KEY_NUM 10
void MSetCommand(redisContext *c) {
  redisReply *res;
  const char* argv[1 + 2 * MSET_MGET_KEY_NUM] = {"MSET"};
  size_t argv_len[1 + 2 * MSET_MGET_KEY_NUM] = {4};

  std::vector<std::string> keys;
  std::vector<std::string> values;
  for (size_t i = 0; i < MSET_MGET_KEY_NUM; i++) {
    keys.push_back(GenRandomKey());
    values.push_back(GenValue());
  }
  for (size_t i = 0; i < MSET_MGET_KEY_NUM; i++) {
    argv[1 + 2 * i] = keys[i].data();
    argv_len[1 + 2 * i] = keys[i].size();

    argv[1 + 2 * i + 1] = values[i].data();
    argv_len[1 + 2 * i + 1] = values[i].size();
  }

  for (size_t i = 0; i < numreq; i++) {
    if (redisAppendCommandArgv(c,
                               1 + 2 * MSET_MGET_KEY_NUM,
                               reinterpret_cast<const char**>(argv),
                               reinterpret_cast<const size_t*>(argv_len)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    }
  }

  for (size_t i = 0; i < numreq; i++) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    } else {
      if (strcmp("OK", res->str)) {
        fprintf (stderr, "mset key value result error: %s", res->str);
        exit(-1);
      }
    }
    freeReplyObject(res);
  }
}

void MGetCommand(redisContext *c) {
  redisReply *res;
  const char* argv[1 + MSET_MGET_KEY_NUM] = {"MGET"};
  size_t argv_len[1 + MSET_MGET_KEY_NUM] = {4};

  std::vector<std::string> keys;
  for (size_t i = 0; i < MSET_MGET_KEY_NUM; i++) {
    keys.push_back(GenRandomKey());
  }
  for (size_t i = 0; i < MSET_MGET_KEY_NUM; i++) {
    argv[1 + i] = keys[i].data();
    argv_len[1 + i] = keys[i].size();
  }

  for (size_t i = 0; i < numreq; i++) {
    if (redisAppendCommandArgv(c,
                               1 + MSET_MGET_KEY_NUM,
                               reinterpret_cast<const char**>(argv),
                               reinterpret_cast<const size_t*>(argv_len)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    }
  }

  for (size_t i = 0; i < numreq; i++) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    } else {
      if (res->elements != MSET_MGET_KEY_NUM) {
        fprintf (stderr, "mget key value result error: %s", res->str);
        exit(-1);
      }
    }
    freeReplyObject(res);
  }
}

void SetCommand(redisContext* c) {
  redisReply *res;
  const std::string& key = GenRandomKey();
  const std::string& value = GenValue();
  const char* argv[3] = {"SET", key.data(), value.data()};
  size_t argv_len[3] = {3, key.size(), value.size()};
  for (size_t i = 0; i < numreq; i++) {
    if (redisAppendCommandArgv(c,
                               3,
                               reinterpret_cast<const char**>(argv),
                               reinterpret_cast<const size_t*>(argv_len)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    }
  }

  for (size_t i = 0; i < numreq; i++) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    } else {
      if (strcmp("OK", res->str)) {
        fprintf (stderr, "set key value result error: %s", res->str);
        exit(-1);
      }
    }
    freeReplyObject(res);
  }
}

void GetCommand(redisContext* c) {
  redisReply *res;
  const std::string& key = GenRandomKey();
  const char* argv[2] = {"GET", key.data()};
  size_t argv_len[2] = {3, key.size()};
  for (size_t i = 0; i < numreq; i++) {
    if (redisAppendCommandArgv(c,
                               2,
                               reinterpret_cast<const char**>(argv),
                               reinterpret_cast<const size_t*>(argv_len)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    }
  }

  for (size_t i = 0; i < numreq; i++) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    } else {
      if (strcmp("OK", res->str)) {
        fprintf (stderr, "get result error: %s", res->str);
        exit(-1);
      }
    }
    freeReplyObject(res);
  }
}

void LPushCommand(redisContext* c) {
  redisReply *res;
  const std::string& key = GenRandomKey();
  const std::string& value = GenValue();
  const char* argv[3] = {"LPUSH", key.data(), value.data()};
  size_t argv_len[3] = {5, key.size(), value.size()};
  for (size_t i = 0; i < numreq; i++) {
    if (redisAppendCommandArgv(c,
                               3,
                               reinterpret_cast<const char**>(argv),
                               reinterpret_cast<const size_t*>(argv_len)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    }
  }

  for (size_t i = 0; i < numreq; i++) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    } else {
      if (strcmp("OK", res->str)) {
        fprintf (stderr, "lpush result error: %s", res->str);
        exit(-1);
      }
    }
    freeReplyObject(res);
  }
}

void CustomComandSequenceTestMulti(redisContext* c) {
  redisReply *res;
  std::string value(64 * 1024, 'x');
  redisAppendCommand(c, "set XxxX %s", value.data());
  redisAppendCommand(c, "set XxxX_01 value");
  redisAppendCommand(c, "set XxxX_02 value");
  redisAppendCommand(c, "set XxxX_03 value");
  redisAppendCommand(c, "set XxxX_04 value");
  redisAppendCommand(c, "multi");
  redisAppendCommand(c, "set XxxX_05 value");
  redisAppendCommand(c, "set XxxX_06 value");
  redisAppendCommand(c, "set XxxX_07 value");
  redisAppendCommand(c, "exec");
  redisAppendCommand(c, "set XxxX_08 value");
  redisAppendCommand(c, "set XxxX_09 value");
  redisAppendCommand(c, "set XxxX_10 value");

  for (size_t i = 0; i < 13; i++) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    }
    freeReplyObject(res);
  }

  redisAppendCommand(c, "set XxxX_11 value");
  redisAppendCommand(c, "set XxxX_12 value");
  redisAppendCommand(c, "set XxxX_13 value");
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
}

void CustomComandSequenceTestQuit(redisContext* c) {
  redisReply *res;
  std::string value(64 * 1024, 'x');
  redisAppendCommand(c, "set XxxX_01 value", value.data());
  redisAppendCommand(c, "set XxxX_02 %s", value.data());
  redisAppendCommand(c, "set XxxX_03 %s", value.data());
  redisAppendCommand(c, "set XxxX_04 %s", value.data());
  redisAppendCommand(c, "set XxxX_05 %s", value.data());
  redisAppendCommand(c, "set XxxX_06 %s", value.data());
  redisAppendCommand(c, "set XxxX_07 %s", value.data());
  redisAppendCommand(c, "get XxxX_01", value.data());
  redisAppendCommand(c, "quit");
  redisAppendCommand(c, "set XxxX_08 value");
  redisAppendCommand(c, "get XxxX_01", value.data());

  for (size_t i = 0; i < 10; i++) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      fprintf(stderr, "Redis Get Command  Error Reply, exit...\n");
      exit(-1);
    }
    printf("reply: %s\n", res->str);
    freeReplyObject(res);
  }
  sleep(1);
}

void CustomComandSequenceTestUnwatch(redisContext* c) {
  redisReply *res;
  redisAppendCommand(c, "watch key");
  redisAppendCommand(c, "multi");
  redisAppendCommand(c, "get key");
  redisAppendCommand(c, "exec");
  redisAppendCommand(c, "unwatch");

  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  printf("unwatch reply: %s\n", res->str);
  freeReplyObject(res);
}

void CustomComandSequenceTestConfig(redisContext* c) {
  redisReply *res;
  redisAppendCommand(c, "config get databases");
  redisAppendCommand(c, "senitinel master");
  redisAppendCommand(c, "info");
  redisAppendCommand(c, "cluster nodes");
  redisAppendCommand(c, "echo axl");

  printf("start get reply\n");
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  redisGetReply(c, reinterpret_cast<void**>(&res));
  freeReplyObject(res);
  printf("Finish get all reply\n");
}

void* Benchmark(void* ptr) {
  size_t benchmark_flags = *(size_t *)ptr;

  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    if ((client[i] = createClient(hostname, port)) == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }
  while (true) {
    for (size_t i = 0; i < client_num; i++) {
      if (benchmark_flags & PRESSURE_MSET) {
        MSetCommand(client[i]);
      }
      if (benchmark_flags & PRESSURE_CUSTOM) {
        CustomComandSequenceTestConfig(client[i]);
        exit(-1);
      }
    }
  }
}

void* ScanData(void*) {
  redisContext* client;
  client = createClient(hostname, port);
  if (client == NULL) {
    fprintf (stderr, "create client error");
    exit(-1);
  }

  size_t cursor = 0;
  size_t total_key = 0;
  const char* scan_argv[2] = {"scan", ""};
  size_t scan_argv_len[2] = {4, 0};
  do {
    std::string cursor_str = std::to_string(cursor);

    redisReply *res;
    scan_argv[1] = cursor_str.data();
    scan_argv_len[1] = cursor_str.size();
    res = (redisReply*)redisCommandArgv(client,
                                        2,
                                        reinterpret_cast<const char**>(scan_argv),
                                        reinterpret_cast<const size_t*>(scan_argv_len));

    if (res->type != REDIS_REPLY_ARRAY) {
      fprintf(stderr, "scan reply error: %s\n", res->str);
      exit(-1);
    }
    cursor = atol(res->element[0]->str);
    total_key += res->element[1]->elements;
    size_t master_node_idx_mask = pow(2, 12) - 1;
    size_t master_node_idx = (cursor & master_node_idx_mask);
    printf("total_key: %12lu, cursor: %12lu, current_master_idx: %lu\n", total_key, cursor, master_node_idx);
  } while (cursor != 0);
  printf("finish\n");
}

void* updateQPS(void *) {
  struct timeval now_tv;
  int64_t last_time = 0;
  int64_t new_time_ms = 0;
  while (true) {
    gettimeofday(&now_tv, NULL);
    new_time_ms = (int64_t)now_tv.tv_sec * 1000 + now_tv.tv_usec / 1000;

    if (last_time + 1000 <= new_time_ms) {
      printf("QPS: %lld\n", total_request - total_request_old);
      total_request_old.store(total_request);
      last_time = new_time_ms;
    }
  }
}


int main(int argc, char **argv) {

  int ch;
  while ((ch = getopt(argc, argv, "h:p:a:d:P:r:c:")) != -1) {
    switch (ch) {
      case 'h' :
        hostname = std::string(optarg);
        break;
      case 'p' :
        port = atoi(optarg);
        break;
      case 'a' :
        password = std::string(optarg);
        break;
      case 'd' :
        data_size = std::atoi(optarg);
        break;
      case 'P' :
        numreq = std::atoi(optarg);
        break;
      case 'r' :
        randomkeys_keyspacelen = std::atoi(optarg);
        break;
      case 'c' :
        client_num = atoi(optarg);
        break;
      default:
        fprintf (stderr, "Unknown option character %c.\n", optopt);
        return 1;
    }
  }


  pthread_t tid[128];
  size_t benchmark_flags = PRESSURE_CUSTOM;
  for (size_t i = 0; i < 1; i++) {
    pthread_create(&tid[0], NULL, Benchmark, &benchmark_flags);
  }

  while (true) {
    sleep(1);
  }
  return 0;
}
