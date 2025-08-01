#pragma once
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include "coroutine_framework/queue.h"
#include "log/logger.h"
#include <coroutine>
#include <thread>
#include "net_define.h"
#include "coroutine_framework/common_execute_module.h"

namespace ToE
{

struct MaintainInfo {
  MaintainInfo(LinkedCoroutine *coroutine, CoRpcCallBack *coro_rpc_callback)
  : coroutine_{coroutine},
  coro_rpc_callback_{coro_rpc_callback} {}
  LinkedCoroutine *coroutine_;
  CoRpcCallBack *coro_rpc_callback_;
};

struct NetService {
  NetService(uint16_t port)
  : listen_port_{port},
  thread_{},
  stop_flag_{true},
  io_ctx_{1},
  lock_{},
  waiting_coros_{} {}
  NetService(const NetService &) = delete;
  NetService(NetService &&) = delete;
  NetService &operator=(const NetService &) = delete;
  NetService &operator=(NetService &&) = delete;
  void start();
  void stop() noexcept;
  void wait() noexcept;
  uint16_t get_listen_port() const { return listen_port_; }
  template <std::ranges::range EndPoints>
  void commit_send_request_task(const uint64_t coro_id,
                                const EndPoints &endpoints,
                                const NetBufferView net_buffer_view,
                                const uint64_t timeout_ns,
                                LinkedCoroutine *coroutine,
                                CoRpcCallBack *coro_rpc_callback) {
    {
      std::unique_lock<std::mutex> lg{lock_};
      boost::asio::co_spawn(io_ctx_, timeout_(coro_id, timeout_ns), boost::asio::detached);
      waiting_coros_.insert({coro_id, MaintainInfo{coroutine, coro_rpc_callback}});
    }
    for (auto &endpoint_with_flag : endpoints) {
      DEBUG_LOG("send request");
      boost::asio::co_spawn(io_ctx_, send_(endpoint_with_flag.first, net_buffer_view), boost::asio::detached);
    }
  }
  void commit_send_response_task(EndPoint endpoint, NetBuffer &&net_buffer);
  boost::asio::awaitable<void> send_request(const std::vector<std::pair<EndPoint, NetBuffer>> &pairs,
                                     const NetBufferView net_buffer);
  boost::asio::awaitable<void> send_response(EndPoint endpoints, NetBuffer net_buffer);
  boost::asio::awaitable<void> listener(boost::asio::io_context& io_ctx, uint16_t port, std::promise<void> &promise);
  boost::asio::awaitable<void> receive(boost::asio::ip::tcp::socket socket);
  boost::asio::awaitable<void> send_(EndPoint endpoint, NetBufferView net_buffer);
  boost::asio::awaitable<void> timeout_(const uint64_t coro_id, const uint64_t timeout_ns);
  uint16_t listen_port_;
  std::jthread thread_;
  std::atomic<bool> stop_flag_;
  boost::asio::io_context io_ctx_;
  std::mutex lock_;
  std::unordered_map<uint64_t/*CoroutineID*/, MaintainInfo> waiting_coros_;
};

}