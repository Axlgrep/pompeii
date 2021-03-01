#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#include <atomic>
#include <iostream>

#include <iostream>
#include <hiredis.h>

const size_t PIPELINE_LIMIT = 1000;
std::string hostname = "127.0.0.1";
size_t port = 18889;
std::string password = "";
size_t client_num = 10;

const size_t KEY_NUM = 100000000;
const size_t FIELD_NUM = 1000;
std::atomic<long long> total_request(0);
std::atomic<long long> total_request_old(0);

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

void transactionCommand(redisContext *c) {
  redisReply *res;
  res = (redisReply*)redisCommand(c, "MULTI");
  if (strcmp("OK", res->str)) {
    fprintf(stderr, "multi result error: %s\n", res->str);
    exit(-1);
  } else {
    freeReplyObject(res);
  }

  std::string value = std::to_string(time(NULL));
  res = (redisReply*)redisCommand(c, "set transaction_key %s", value.data());
  if (strcmp("QUEUED", res->str)) {
    fprintf(stderr, "set transaction_key error: %s\n", res->str);
    exit(-1);
  } else {
    freeReplyObject(res);
  }

  res = (redisReply*)redisCommand(c, "get transaction_key");
  if (strcmp("QUEUED", res->str)) {
    fprintf(stderr, "get transaction_key error: %s\n", res->str);
    exit(-1);
  } else {
    freeReplyObject(res);
  }

  res = (redisReply*)redisCommand(c, "del transaction_key");
  if (strcmp("QUEUED", res->str)) {
    fprintf(stderr, "del transaction_key error: %s\n", res->str);
    exit(-1);
  } else {
    freeReplyObject(res);
  }

  res = (redisReply*)redisCommand(c, "exec");
  if (REDIS_REPLY_ARRAY != res->type) {
    fprintf(stderr, "exec result error\n");
    exit(-1);
  }

  if (res->elements != 3) {
    fprintf(stderr, "exec result array size error\n");
    exit(-1);
  }
}

void MSetAndMGetCommand(redisContext *c) {
  redisReply *res;

  std::vector<std::string> keys;
  std::vector<std::string> values;
  const char* mset_argv[1001] = {"MSET"};
  size_t mset_argv_len[1001] = {4};
  for (int32_t idx = 0; idx < 500; idx++) {
    std::string key = "mset_and_mget_key_" +  std::to_string(idx);
    std::string value = "mset_and_mget_value_" + std::to_string(idx);
    keys.push_back(key);
    values.push_back(value);
    mset_argv[1 + 2 * idx] = keys[idx].data(), mset_argv_len[1 + 2 * idx] = keys[idx].size();
    mset_argv[1 + 2 * idx + 1] = values[idx].data(), mset_argv_len[1 + 2 * idx + 1] = values[idx].size();
  }


  res = (redisReply*)redisCommandArgv(c,
                                      1001,
                                      reinterpret_cast<const char**>(mset_argv),
                                      reinterpret_cast<const size_t*>(mset_argv_len));
  if (strcmp("OK", res->str)) {
    fprintf(stderr, "mset error: %s\n", res->str);
    exit(-1);
  } else {
    freeReplyObject(res);
  }

  const char* mget_argv[501] = {"MGET"};
  size_t mget_argv_len[501] = {4};
  for (int32_t idx = 0; idx < 500; idx++) {
    mget_argv[1 + idx] = keys[idx].data(), mget_argv_len[1 + idx] = keys[idx].size();
  }

  res = (redisReply*)redisCommandArgv(c,
                                      501,
                                      reinterpret_cast<const char**>(mget_argv),
                                      reinterpret_cast<const size_t*>(mget_argv_len));

  if (REDIS_REPLY_ARRAY != res->type) {
    fprintf(stderr, "mget result type error\n");
    exit(-1);
  }

  if (res->elements != 500) {
    fprintf(stderr, "mget elements number error\n");
    exit(-1);
  }

  for (int32_t idx = 0; idx < 500; idx++) {
    if (strcmp(res->element[idx]->str, values[idx].data())) {
      fprintf(stderr, "mget value error\n");
      exit(-1);
    }
  }

  freeReplyObject(res);
}

void pipelineSetCommand(redisContext *c) {
  redisReply *res;
  const char* argv[3] = {"SET", NULL, NULL};
  size_t argv_len[3] = {3, 0, 0};
  for (int32_t idx = 0; idx < PIPELINE_LIMIT; ++idx) {
    std::string key = "pipeline_key_" +  std::to_string(idx);
    std::string value = "value_" + std::to_string(idx);
    argv[1] = key.data(), argv_len[1] = key.size();
    argv[2] = value.data(), argv_len[2] = value.size();

    if (redisAppendCommandArgv(c,
                               3,
                               reinterpret_cast<const char**>(argv),
                               reinterpret_cast<const size_t*>(argv_len)) == REDIS_ERR) {
      printf("Redis Append Command Argv Error, exit...\n");
      exit(-1);
    }
  }

  for (int32_t idx = 0; idx < PIPELINE_LIMIT; ++idx) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      printf("Redis Get Reply Error, exit...\n");
      exit(-1);
    } else {
      if (res == NULL) {
        fprintf(stderr, "Redis Get Reply Error, Index: %d, Reply: NULL, exit...\n", idx);
        exit(-1);
      } else {
        if (strcmp("OK", res->str)) {
          fprintf (stderr, "set key value result error: %s", res->str);
        }
      }
      freeReplyObject(res);
    }
  }
}

void pipelineGetCommand(redisContext *c) {
  redisReply *res;
  const char* argv[2] = {"Get", NULL};
  size_t argv_len[2] = {3, 0};
  for (int32_t idx = 0; idx < PIPELINE_LIMIT; ++idx) {
    std::string key = "pipeline_key_" +  std::to_string(idx);
    argv[1] = key.data(), argv_len[1] = key.size();

    if (redisAppendCommandArgv(c,
                               2,
                               reinterpret_cast<const char**>(argv),
                               reinterpret_cast<const size_t*>(argv_len)) == REDIS_ERR) {
      fprintf(stderr, "Redis Append Command Argv Error, exit...\n");
      exit(-1);
    }
  }

  for (int32_t idx = 0; idx < PIPELINE_LIMIT; ++idx) {
    if (redisGetReply(c, reinterpret_cast<void**>(&res)) == REDIS_ERR) {
      fprintf(stderr, "Redis Get Reply Error, exit...\n");
      exit(-1);
    } else {
      if (res == NULL) {
        fprintf(stderr, "Redis Get Reply Error, Index: %d, Reply: NULL, exit...\n", idx);
        exit(-1);
      } else {
        std::string value = "value_" + std::to_string(idx);
        if (res->str == NULL || strcmp(value.data(), res->str)) {
          fprintf(stderr, "get key value result error");
        }
      }
      freeReplyObject(res);
    }
  }
}

void bigValueSetGetCommand(redisContext* c) {
  redisReply *res;
  const char* set_argv[3] = {"Set", NULL, NULL};
  size_t set_argv_len[3] = {3, 0, 0};
  std::string key = "big_value_key";
  std::string value = "big_value_" + std::string(1024, 'x');
  set_argv[1] = key.data(), set_argv_len[1] = key.size();
  set_argv[2] = value.data(), set_argv_len[2] = value.size();

  res = (redisReply*)redisCommandArgv(c,
                                      3,
                                      reinterpret_cast<const char**>(set_argv),
                                      reinterpret_cast<const size_t*>(set_argv_len));
  if (strcmp("OK", res->str)) {
    fprintf(stderr, "big value set error: %s\n", res->str);
    exit(-1);
  } else {
    freeReplyObject(res);
  }

  const char* get_argv[2] = {"Get", NULL};
  size_t get_argv_len[2] = {3, 0};
  get_argv[1] = key.data(), get_argv_len[1] = key.size();

  res = (redisReply*)redisCommandArgv(c,
                                      2,
                                      reinterpret_cast<const char**>(get_argv),
                                      reinterpret_cast<const size_t*>(get_argv_len));
  if (strcmp(value.data(), res->str)) {
    fprintf(stderr, "big value get result error: %s\n", res->str);
    exit(-1);
  } else {
    freeReplyObject(res);
  }
}

void SetCommand(redisContext* c) {
  redisReply *res;
  const char* set_argv[3] = {"set"};
  size_t set_argv_len[2 + FIELD_NUM] = {3};
  for (size_t i = 0; i < KEY_NUM; i++) {
    std::string key = "strings_key_" + std::to_string(i);
    set_argv[1] = key.data();
    set_argv_len[1] = key.size();
    res = (redisReply*)redisCommandArgv(c,
                                        3,
                                        reinterpret_cast<const char**>(set_argv),
                                        reinterpret_cast<const size_t*>(set_argv_len));
    if (res->type != REDIS_REPLY_STATUS) {
      fprintf(stderr, "set error: %s\n", res->str);
      exit(-1);
    } else {
      freeReplyObject(res);
    }
  }
}

void LpushCommand(redisContext* c) {
  redisReply *res;
  std::vector<std::string> nodes;
  const char* lpush_argv[2 + FIELD_NUM] = {"lpush"};
  size_t lpush_argv_len[2 + FIELD_NUM] = {5};
  for (size_t i = 0; i < FIELD_NUM; i++) {
    nodes.push_back("node_" + std::to_string(i));
  }
  for (size_t i = 0; i < KEY_NUM; i++) {
    std::string key = "list_key_" + std::to_string(i);
    lpush_argv[1] = key.data();
    lpush_argv_len[1] = key.size();
    for (size_t j = 0; j < nodes.size(); j++) {
      lpush_argv[2 + j] = nodes[j].data();
      lpush_argv_len[2 + j] = nodes[j].size();
    }
    res = (redisReply*)redisCommandArgv(c,
                                        FIELD_NUM + 2,
                                        reinterpret_cast<const char**>(lpush_argv),
                                        reinterpret_cast<const size_t*>(lpush_argv_len));
    if (res->type != REDIS_REPLY_INTEGER) {
      fprintf(stderr, "lpush error: %s\n", res->str);
      exit(-1);
    } else {
      freeReplyObject(res);
    }
  }
}

void HMSetCommand(redisContext* c) {
  redisReply *res;
  std::vector<std::string> fields;
  std::vector<std::string> values;
  const char* hset_argv[2 + 2 * FIELD_NUM] = {"hmset"};
  size_t hset_argv_len[2 + 2 * FIELD_NUM] = {5};
  for (size_t i = 0; i < FIELD_NUM; i++) {
    fields.push_back("field_" + std::to_string(i));
    values.push_back("value_" + std::to_string(i));
  }
  for (size_t i = 0; i < KEY_NUM; i++) {
    std::string key = "hash_key_" + std::to_string(i);
    hset_argv[1] = key.data();
    hset_argv_len[1] = key.size();
    for (size_t j = 0; j < fields.size(); j++) {
      hset_argv[2 + 2 * j] = fields[j].data();
      hset_argv_len[2 + 2 * j] = fields[j].size();

      hset_argv[2 + 2 * j + 1] = values[j].data();
      hset_argv_len[2 + 2 * j + 1] = values[j].size();
    }
    res = (redisReply*)redisCommandArgv(c,
                                        2 + 2 * FIELD_NUM,
                                        reinterpret_cast<const char**>(hset_argv),
                                        reinterpret_cast<const size_t*>(hset_argv_len));
    if (!strcmp(res->str, "ok")) {
      fprintf(stderr, "hmset error: %s\n", res->str);
      exit(-1);
    } else {
      freeReplyObject(res);
    }
  }
}

void SAddCommand(redisContext* c) {
  redisReply *res;
  std::vector<std::string> members;
  const char* sadd_argv[2 + FIELD_NUM] = {"sadd"};
  size_t sadd_argv_len[2 + FIELD_NUM] = {4};
  for (size_t i = 0; i < FIELD_NUM; i++) {
    members.push_back("member_" + std::to_string(i));
  }
  for (size_t i = 0; i < KEY_NUM; i++) {
    std::string key = "set_key_" + std::to_string(i);
    sadd_argv[1] = key.data();
    sadd_argv_len[1] = key.size();
    for (size_t j = 0; j < members.size(); j++) {
      sadd_argv[2 + j] = members[j].data();
      sadd_argv_len[2 + j] = members[j].size();
    }
    res = (redisReply*)redisCommandArgv(c,
                                        FIELD_NUM + 2,
                                        reinterpret_cast<const char**>(sadd_argv),
                                        reinterpret_cast<const size_t*>(sadd_argv_len));
    if (res->type != REDIS_REPLY_INTEGER) {
      fprintf(stderr, "sadd error: %s\n", res->str);
      exit(-1);
    } else {
      freeReplyObject(res);
    }
  }
}

void ZAddCommand(redisContext* c) {
  redisReply *res;
  std::vector<std::string> scores;
  std::vector<std::string> members;
  const char* zadd_argv[2 + 2 * FIELD_NUM] = {"zadd"};
  size_t zadd_argv_len[2 + 2 * FIELD_NUM] = {4};
  for (size_t i = 0; i < FIELD_NUM; i++) {
    scores.push_back(std::to_string(i));
    members.push_back("members_" + std::to_string(i));
  }
  for (size_t i = 0; i < KEY_NUM; i++) {
    std::string key = "zadd_key_" + std::to_string(i);
    zadd_argv[1] = key.data();
    zadd_argv_len[1] = key.size();
    for (size_t j = 0; j < scores.size(); j++) {
      zadd_argv[2 + 2 * j] = scores[j].data();
      zadd_argv_len[2 + 2 * j] = scores[j].size();

      zadd_argv[2 + 2 * j + 1] = members[j].data();
      zadd_argv_len[2 + 2 * j + 1] = members[j].size();
    }
    res = (redisReply*)redisCommandArgv(c,
                                        2 + 2 * FIELD_NUM,
                                        reinterpret_cast<const char**>(zadd_argv),
                                        reinterpret_cast<const size_t*>(zadd_argv_len));
    if (res->type != REDIS_REPLY_INTEGER) {
      fprintf(stderr, "zadd error: %s\n", res->str);
      exit(-1);
    } else {
      freeReplyObject(res);
    }
  }
}

void PublishCommand(redisContext* c) {
  std::vector<std::string> channels;
  channels.push_back("channel1");
  channels.push_back("channel2");
  channels.push_back("channel3");
  channels.push_back("channel4");
  channels.push_back("channel5");
  channels.push_back("channel6");
  channels.push_back("channel7");
  channels.push_back("channel8");
  channels.push_back("channel9");
  channels.push_back("channel10");
  channels.push_back("channel11");
  channels.push_back("channel12");
  channels.push_back("channel13");
  channels.push_back("channel14");
  channels.push_back("channel15");
  channels.push_back("channel16");
  const char* publish_argv[3] = {"publish", "", "message"};
  size_t publish_argv_len[3] = {7, 0, 7};

  for (const auto& channel : channels) {
    redisReply *res;
    publish_argv[1] = channel.data();
    publish_argv_len[1] = channel.size();
    res = (redisReply*)redisCommandArgv(c,
                                        3,
                                        reinterpret_cast<const char**>(publish_argv),
                                        reinterpret_cast<const size_t*>(publish_argv_len));

    if (res->type != REDIS_REPLY_INTEGER) {
      fprintf(stderr, "publish message error: %s\n", res->str);
      exit(-1);
    } else {
      total_request++;
      freeReplyObject(res);
    }
  }
}

void SubscribeCommand(redisContext* c) {
  const char* subscribe_argv[2] = {"subscribe", "channel"};
  size_t subscribe_argv_len[2] = {9, 7};

  redisReply *res;
  res = (redisReply*)redisCommandArgv(c,
                                      2,
                                      reinterpret_cast<const char**>(subscribe_argv),
                                      reinterpret_cast<const size_t*>(subscribe_argv_len));
  printf("res: %s\n", res->str);

  /*
    if (res->type != REDIS_REPLY_INTEGER) {
      fprintf(stderr, "publish message error: %s\n", res->str);
      exit(-1);
    } else {
      total_request++;
      freeReplyObject(res);
    }
  }
  */
}


void* pressurePipeline(void*) {
  
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  while (true) {
    for (size_t i = 0; i < client_num; i++) {
      pipelineSetCommand(client[i]);
      pipelineGetCommand(client[i]);
    }
  }
}

void* pressureTransaction(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  while (true) {
    for (size_t i = 0; i < client_num; i++) {
      transactionCommand(client[i]);
    }
  }
}

void* pressureMsetMget(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  while (true) {
    for (size_t i = 0; i < client_num; i++) {
      MSetAndMGetCommand(client[i]);
    }
  }
}

void* pressureBigValueSetGet(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  for (size_t i = 0; i < client_num; i++) {
    bigValueSetGetCommand(client[i]);
  }
}

void* pressureSet(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  for (size_t i = 0; i < client_num; i++) {
    SetCommand(client[i]);
  }
}

void* pressureLPush(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  for (size_t i = 0; i < client_num; i++) {
    LpushCommand(client[i]);
  }
}

void* pressureHSet(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  for (size_t i = 0; i < client_num; i++) {
    HMSetCommand(client[i]);
  }
}

void* pressureSAdd(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  for (size_t i = 0; i < client_num; i++) {
    SAddCommand(client[i]);
  }
}

void* pressureZAdd(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  for (size_t i = 0; i < client_num; i++) {
    ZAddCommand(client[i]);
  }
}

void* pressureSubscribe(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  for (size_t i = 0; i < client_num; i++) {
    SubscribeCommand(client[i]);
  }
  while (true) {
  }
}


void* pressurePublish(void*) {
  redisContext* client[client_num];
  for (size_t i = 0; i < client_num; i++) {
    client[i] = createClient(hostname, port);
    if (client[i] == NULL) {
      fprintf (stderr, "create client error");
      exit(-1);
    }
  }

  while (true) {
    for (size_t i = 0; i < client_num; i++) {
      PublishCommand(client[i]);
    }
  }
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
  while ((ch = getopt(argc, argv, "h:p:a:")) != -1) {

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
      case 'c' :
        client_num = atoi(optarg);
        break;
      default:
        fprintf (stderr, "Unknown option character %c.\n", optopt);
        return 1;
    }
  }

  pthread_t tid[128];

  /*
  for (size_t i = 0; i < 10; i++) {
    if (pthread_create(&tid[i], NULL, pressurePipeline, NULL)) {
      fprintf (stderr, "pthread create error");
      return 1;
    }
  }
  for (size_t i = 10; i < 20; i++) {
    if (pthread_create(&tid[i], NULL, pressureTransaction, NULL)) {
      fprintf (stderr, "pthread create error");
      return 1;
    }
  }
  */
  /*
  for (size_t i = 0; i < 10; i++) {
    if (pthread_create(&tid[i], NULL, pressureMsetMget, NULL)) {
      fprintf(stderr, "pthrad create error");
      return 1;
    }
  }
  */
  /*
  for (size_t i = 30; i < 40; i++) {
    if (pthread_create(&tid[i], NULL, pressureBigValueSetGet, NULL)) {
      fprintf(stderr, "pthrad create error");
      return 1;
    }
  }
  */

  /*
  pthread_create(&tid[0], NULL, pressureSet, NULL);
  pthread_create(&tid[1], NULL, pressureLPush, NULL);
  pthread_create(&tid[2], NULL, pressureHSet, NULL);
  pthread_create(&tid[3], NULL, pressureSAdd, NULL);
  pthread_create(&tid[4], NULL, pressureZAdd, NULL);
  */
  for (size_t i = 0; i < 128; i++) {
    pthread_create(&tid[i], NULL, pressureSubscribe, NULL);
  }

  pthread_t update_qps_thread;
  pthread_create(&update_qps_thread, NULL, updateQPS, NULL);

  while (true) {
    sleep(1);
  }
  return 0;


}
