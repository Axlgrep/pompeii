#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <mutex>
#include <atomic>
#include <thread>
#include <algorithm>
#include <unordered_map>

#include <assert.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <event2/event.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>

DEFINE_int32(threads, 10, "Number of threads");
DEFINE_int32(clients_num_per_thread, 700, "Number of concurrent clients per thread");
DEFINE_int32(value_size, 256, "value length");
DEFINE_int32(cmd_timeout, 5000, "timeout for command execution");
DEFINE_int32(pipeline_num, 10, "pipe_line_num");
DEFINE_int32(connect_timeout, 5000, "timeout for command execution");
DEFINE_string(host, "9.134.241.132", "Redis server host");
DEFINE_int32(port, 18889, "Redis server port");
DEFINE_string(password, "abc", "Redis server password");
DEFINE_int64(key_min, 0, "Key left limit");
DEFINE_int64(key_max, 80000000000, "Key right limit");

class RedisPool;
class RedisConnection;

enum class State {
  CONNECTING,
  CONNECTED,
  DISCONNECTED
};

static const char *LUA_SCRIPT =
    "local key = KEYS[1]; "
    "local value = ARGV[1]; "
    "local expire = tonumber(ARGV[2]); "
    "if (expire == nil) then return -1 end; "
    "redis.call('hincrby', key, 'c', 1); "
    "local result = redis.call('hset', key, 'v', value); "
    "if (expire > 0) then redis.call('expire', key, expire) end; "
    "return result;";

struct Stats {
  std::atomic<int64_t> current_online_client_num;
  std::atomic<int64_t> total_command_process;
  std::atomic<int64_t> total_created_client_num;
  std::atomic<int64_t> timeout_cmd_num;
  Stats& operator=(const Stats& other) {
    if (this != &other) {  // 防止自赋值
        // 原子操作保证线程安全
        current_online_client_num.store(
            other.current_online_client_num.load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        
        total_command_process.store(
            other.total_command_process.load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        
        total_created_client_num.store(
            other.total_created_client_num.load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
        timeout_cmd_num.store(
            other.timeout_cmd_num.load(std::memory_order_relaxed),
            std::memory_order_relaxed
        );
    }
    return *this;
  }
};


void pipeline_hget(RedisConnection* conn, redisAsyncContext *ac);
void pipeline_eval(RedisConnection* conn, redisAsyncContext *ac);

void connect_callback(const redisAsyncContext* ctx, int status);
void disconnect_callback(const redisAsyncContext* ctx, int status);
void connect_timeout_callback(evutil_socket_t fd, short event, void* arg);
void auth_cmd_callback(redisAsyncContext* ctx, void* r, void* priv);
void hget_cmd_callback(redisAsyncContext* ctx, void* r, void* priv);
void eval_cmd_callback(redisAsyncContext* ctx, void* r, void* priv);
void cmd_timeout_callback(evutil_socket_t fd, short event, void* arg);

static Stats old_stats;
static Stats new_stats;
static std::atomic<int32_t> ID_Generator;

static size_t random_value_array_size = 1000;
static std::vector<std::string> random_values;

void generate_random_strings(size_t count, size_t length) {
  // 定义可用字符集(排除易混淆字符)
  constexpr char charset[] = 
      "ABCDEFGHJKLMNPQRSTUVWXYZ"    // 去除I, O
      "abcdefghjkmnpqrstuvwxyz"     // 去除i, l, o
      "23456789";                   // 去除0, 1
  constexpr size_t charset_size = sizeof(charset) - 1;

  // 准备随机数生成器（线程安全版本）
  thread_local static std::mt19937 engine{std::random_device{}()};
  thread_local static std::uniform_int_distribution<size_t> dist(0, charset_size - 1);

  random_values.clear();
  for (size_t i = 0; i < count; ++i) {
      std::string str;
      str.reserve(length);
      std::generate_n(std::back_inserter(str), length, [&] {
          return charset[dist(engine)];
      });
      random_values.emplace_back(std::move(str));
  }
}


class RedisConnection {
public:
  event_base* base;
  RedisPool* redis_pool;

  bool in_use;
  bool authed;
  std::atomic<State> state;
  int32_t client_id;
  int32_t remaining;

  redisAsyncContext* context;
  struct event* cmd_timeout_ev;
  struct event* connect_timeout_ev;


  RedisConnection(event_base* ev_base, RedisPool* p)
    : base(ev_base),
      redis_pool(p),
      in_use(false),
      authed(false),
      remaining(0),
      context(nullptr),
      cmd_timeout_ev(nullptr),
      connect_timeout_ev(nullptr) {

    new_stats.total_created_client_num.fetch_add(1);
    client_id = ID_Generator.fetch_add(1);

    state.store(State::CONNECTING);
    context = redisAsyncConnect(FLAGS_host.data(), FLAGS_port);
    // LOG(INFO) << "Client id(" << client_id << ") created, context: " << context;
    if (context->err) {
      LOG(FATAL) << "Connection error: " << context->errstr;
      redisAsyncFree(context);
      context = nullptr;
      return;
    }

    context->data = this;
    redisAsyncSetConnectCallback(context, connect_callback);
    redisAsyncSetDisconnectCallback(context, disconnect_callback);
    redisLibeventAttach(context, ev_base);

    //设置连接超时回调
    assert(FLAGS_connect_timeout > 10);
    timeval connect_timeout_tv;
    connect_timeout_tv.tv_sec = FLAGS_connect_timeout / 1000;
    connect_timeout_tv.tv_usec = (FLAGS_connect_timeout % 1000) * 1000;
    connect_timeout_ev = evtimer_new(base, connect_timeout_callback, this);
    event_add(connect_timeout_ev, &connect_timeout_tv);

    redisAsyncCommand(context, nullptr, nullptr, "ping");
  }

  ~RedisConnection() {
    // LOG(INFO) << "Client id(" << client_id << " desctory";
    context->data = nullptr;
    if (cmd_timeout_ev) event_free(cmd_timeout_ev);
    if (connect_timeout_ev) event_free(connect_timeout_ev);
    if (context) redisAsyncFree(context);
  }
};

class RedisPool {
 public:
  RedisPool(event_base* ev_base, int size)
    : base(ev_base), client_limit(size), call(0), rng(std::random_device{}()) {
    assert(FLAGS_cmd_timeout > 1);
    LOG(INFO) << "client limit: " << client_limit;
    cmd_timeout_tv.tv_sec = FLAGS_cmd_timeout / 1000;
    cmd_timeout_tv.tv_usec = (FLAGS_cmd_timeout % 1000) * 1000;
    for (int i = 0; i < client_limit; ++i) {
      add_client_to_pool();
    }
  }

  void add_client_to_pool() {
    RedisConnection* const conn = new RedisConnection(base, this);
    assert(clients.find(conn->client_id) == clients.end());
    clients.emplace(conn->client_id, conn);
    new_stats.current_online_client_num.fetch_add(1);
  }

  void remove_client_from_pool(int32_t client_id) {
    assert(clients.find(client_id) != clients.end());
    RedisConnection* const conn = clients[client_id];
    clients.erase(client_id);
    delete conn;
    new_stats.current_online_client_num.fetch_sub(1);
  }

  void send_random_command() {
    std::uniform_int_distribution<int64_t> dist(FLAGS_key_min, FLAGS_key_max);
    for (auto& item : clients) {
      auto& conn = item.second;
      if (conn->state.load() == State::CONNECTED && !conn->in_use && conn->remaining == 0) {
        //LOG(INFO) << "Client id(" << conn->client_id << ") send command";
        conn->in_use = true;
        std::string key = "type_hash_" + std::to_string(dist(rng));

        conn->cmd_timeout_ev = evtimer_new(base, cmd_timeout_callback, conn);
        event_add(conn->cmd_timeout_ev, &cmd_timeout_tv);


        size_t random_value_idx = dist(rng) % random_value_array_size;
        if (conn->authed) {
          if (call++ % 3 == 0) {
            pipeline_hget(conn, conn->context);
          } else {
            pipeline_eval(conn, conn->context);
          }
        } else {
          redisAsyncCommand(conn->context, auth_cmd_callback, nullptr, "auth %s", FLAGS_password.data());
          conn->remaining++;
        }
      }
    }
  }

  void maintain_pool() {
    while (clients.size() < client_limit) {
      add_client_to_pool();
    }
  }

 private:
  event_base* base;
  std::unordered_map<int32_t, RedisConnection*> clients;
  std::mt19937 rng;
  timeval cmd_timeout_tv;
  int client_limit;
  int call;
};

void auth_cmd_callback(redisAsyncContext *ctx, void *r, void *priv) {
  if (!ctx->data) return;
  auto* conn = static_cast<RedisConnection*>(ctx->data);
  redisReply* reply = static_cast<redisReply*>(r);

  if (!reply) {
    // LOG(WARNING) << "Client id(" << conn->client_id << ") command failed, remove it from client pool";
    auto* redis_pool = conn->redis_pool;
    redis_pool->remove_client_from_pool(conn->client_id);
    return;
  }

  if (reply->type == REDIS_REPLY_ERROR) {
    LOG(FATAL) << "Auth error: " << reply->str;
    return;
  }

  // 处理成功响应
  if (conn->cmd_timeout_ev) {
    event_free(conn->cmd_timeout_ev);
    conn->cmd_timeout_ev = nullptr;
  }

  new_stats.total_command_process.fetch_add(1);
  conn->remaining--;
  conn->in_use = false;
  conn->authed = true;
}

void eval_cmd_callback(redisAsyncContext* ctx, void* r, void* priv) {
  if (!ctx->data) return;
  auto* conn = static_cast<RedisConnection*>(ctx->data);
  redisReply* reply = static_cast<redisReply*>(r);

  if (reply == nullptr) {
    // LOG(WARNING) << "Client id(" << conn->client_id << ") command failed, remove it from client pool";
    auto* redis_pool = conn->redis_pool;
    redis_pool->remove_client_from_pool(conn->client_id);
    return;
  }

  // LOG(INFO) << "eval cmd callback, remaining: " << conn->remaining;
  new_stats.total_command_process.fetch_add(1);
  conn->remaining--;
  conn->in_use = false;

  // 处理成功响应
  if (conn->remaining == 0 && conn->cmd_timeout_ev) {
    event_free(conn->cmd_timeout_ev);
    conn->cmd_timeout_ev = nullptr;
    // LOG(INFO) << "eval cmd callback, clear timeout";
  }
}

void pipeline_hget(RedisConnection* conn, redisAsyncContext *ac) {
  thread_local static std::mt19937 engine{std::random_device{}()};
  thread_local static std::uniform_int_distribution<size_t> dist(FLAGS_key_min, FLAGS_key_max - 1);

  std::vector<const char*> argv;
  for (size_t i = 0; i < FLAGS_pipeline_num; i++) {
    std::string key = "type_hash_" + std::to_string(dist(engine));
    redisAsyncCommand(ac, hget_cmd_callback, nullptr, "hget %s v", key.c_str());
    conn->remaining++;
  }
}

void pipeline_eval(RedisConnection* conn, redisAsyncContext *ac) {
  thread_local static std::mt19937 engine{std::random_device{}()};
  thread_local static std::uniform_int_distribution<size_t> dist(FLAGS_key_min, FLAGS_key_max - 1);

  std::vector<const char*> argv;
  for (size_t i = 0; i < FLAGS_pipeline_num; i++) {
    std::string key = "type_hash_" + std::to_string(dist(engine));
    redisAsyncCommand(ac, eval_cmd_callback, nullptr, "EVAL %s 1 %s %s 0", LUA_SCRIPT, key.data(), random_values[dist(engine) % random_values.size()].data());
    conn->remaining++;
  }
}

void hget_cmd_callback(redisAsyncContext* ctx, void* r, void* priv) {
  if (!ctx->data) return;
  auto* conn = static_cast<RedisConnection*>(ctx->data);
  redisReply* reply = static_cast<redisReply*>(r);

  if (reply == nullptr) {
    LOG(WARNING) << "Client id(" << conn->client_id << ") command failed, remove it from client pool";
    auto* redis_pool = conn->redis_pool;
    redis_pool->remove_client_from_pool(conn->client_id);
    return;
  }

  // LOG(INFO) << "hget cmd callback, remaining: " << conn->remaining;
  new_stats.total_command_process.fetch_add(1);
  conn->remaining--;
  conn->in_use = false;

  // 处理成功响应
  if (conn->remaining == 0 && conn->cmd_timeout_ev) {
    event_free(conn->cmd_timeout_ev);
    conn->cmd_timeout_ev = nullptr;
    // LOG(INFO) << "hget cmd callback, clear timeout";
  }
}

void connect_timeout_callback(evutil_socket_t fd, short event, void* arg) {
  auto* conn = static_cast<RedisConnection*>(arg);
  assert(conn->state.load() == State::CONNECTING);
  // LOG(WARNING) << "Client id(" << conn->client_id << ") Connection timeout, remove it from client pool";
  auto* redis_pool = conn->redis_pool;
  redis_pool->remove_client_from_pool(conn->client_id);
}

void connect_callback(const redisAsyncContext* ctx, int status) {
  if (!ctx->data) return;
  auto* conn = static_cast<RedisConnection*>(ctx->data);
  event_free(conn->connect_timeout_ev);
  conn->connect_timeout_ev = nullptr;

  if (status != REDIS_OK) {
    LOG(ERROR) << "Client id(" << conn->client_id << ") Connection error: " << ctx->errstr;
    conn->state.store(State::DISCONNECTED);

    LOG(WARNING) << "Client id(" << conn->client_id << ") timeout, remove it from client pool";
    auto* redis_pool = conn->redis_pool;
    redis_pool->remove_client_from_pool(conn->client_id);
  } else {
    // LOG(INFO) << "Client id(" << conn->client_id << ") Successfully connected to server";
    conn->state.store(State::CONNECTED);
  }
}

void disconnect_callback(const redisAsyncContext* ctx, int status) {
  if (!ctx->data) return;
  auto* conn = static_cast<RedisConnection*>(ctx->data);
  //LOG(INFO) << "Client id(" << conn->client_id << ") disconnect callback";
  auto* redis_pool = conn->redis_pool;
  redis_pool->remove_client_from_pool(conn->client_id);
}

void cmd_timeout_callback(evutil_socket_t fd, short event, void* arg) {
  auto* conn = static_cast<RedisConnection*>(arg);
  auto* redis_pool = conn->redis_pool;
  // LOG(WARNING) << "Client id(" << conn->client_id << ") command timeout, remove it from client pool";
  redis_pool->remove_client_from_pool(conn->client_id);
  new_stats.timeout_cmd_num.fetch_add(1);
}

struct ThreadContext {
  event_base* base;
  RedisPool* redis_pool;
  std::thread* thread;
};

void ThreadFun(const ThreadContext& context) {
  event_base* base = context.base;
  RedisPool* redis_pool = context.redis_pool;
  std::thread* thread = context.thread;

  timeval tv = {0, 10000};
  event* ev = event_new(base, -1, EV_PERSIST, [](evutil_socket_t, short, void* arg) {
    auto* redis_pool = static_cast<RedisPool*>(arg);
    redis_pool->send_random_command();
    redis_pool->maintain_pool();
  }, redis_pool);
  event_add(ev, &tv);

  event_base_dispatch(base);
  event_free(ev);
}

void DumpLogThread() {
  while (true) {
    LOG(INFO) << "Current client: " << new_stats.current_online_client_num
              << ", requests per second: " << new_stats.total_command_process - old_stats.total_command_process
              << ", created client per second: " << new_stats.total_created_client_num - old_stats.total_created_client_num
              << ", timeout cmd per second: " << new_stats.timeout_cmd_num - old_stats.timeout_cmd_num;
    old_stats = new_stats;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void init_glog() {
  google::InitGoogleLogging("lys-bench");
	FLAGS_alsologtostderr = true;
  FLAGS_logbufsecs = 0;        // 立即输出日志
  FLAGS_max_log_size = 10;     // 每个日志文件最大10MB
  google::SetLogDestination(google::INFO,    "./logs/info_");
  google::SetLogDestination(google::WARNING, "./logs/warning_");
  google::SetLogDestination(google::ERROR,   "./logs/error_");
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  init_glog();
  generate_random_strings(random_value_array_size, FLAGS_value_size);

  std::thread* dump_log_thread = new std::thread(DumpLogThread);

  std::vector<ThreadContext> thread_context_array;
  thread_context_array.resize(FLAGS_threads);
  for (auto& item : thread_context_array) {
    item.base = event_base_new();
    LOG(INFO) << "event base new: " << item.base;
    item.redis_pool = new RedisPool(item.base, FLAGS_clients_num_per_thread);
    item.thread = new std::thread(ThreadFun, item);
  }

  while(1) {
  }

  for (auto& item : thread_context_array) {
    item.thread->join();
    event_base_free(item.base);
    delete item.redis_pool;
  }

  delete dump_log_thread;

  google::ShutdownGoogleLogging();
  return 0;
}
