#include "net_service.h"
#include "coroutine_framework/net_module/net_define.h"
#include "error_define/error_struct.h"
#include "rpc_struct.h"
#include "mechanism/serialization.hpp"
#include "literal.h"
#include "coroutine_framework/scheduler.h"
#include "rpc_mapper.h"
#include <future>
#include <mutex>

using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace ToE
{

void NetService::start() {
  stop_flag_.store(false, std::memory_order_release);
  std::promise<void> promise;
  std::future<void> future = promise.get_future();
  thread_ = std::jthread([this,
                          &promise,
                          scheduler = TLS_SCHEDULER,
                          framework = TLS_FRAMEWORK] {
    TLS_SCHEDULER = scheduler;
    TLS_FRAMEWORK = framework;
    asio::co_spawn(io_ctx_, listener(io_ctx_, listen_port_, promise), asio::detached);
    io_ctx_.run();
  });
  future.wait();
  DEBUG_LOG("NetService started");
}

void NetService::stop() noexcept {
  io_ctx_.stop();
  DEBUG_LOG("NetService stopped");
}

void NetService::wait() noexcept {
  if (thread_.joinable()) [[likely]] {
    thread_.join();
  }
  DEBUG_LOG("NetService joined");
}

void NetService::commit_send_response_task(EndPoint endpoint, NetBuffer &&net_buffer) {
  co_spawn(io_ctx_, send_response(endpoint, std::move(net_buffer)), asio::detached);
}

asio::awaitable<void> NetService::listener(asio::io_context& io_ctx, uint16_t port, std::promise<void> &promise) {
  try {
    INFO_LOG("listen on:{}", port);
    auto executor = co_await asio::this_coro::executor;
    tcp::acceptor acceptor(executor, {tcp::v4(), port});
    promise.set_value();
    while (true) {
      tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
      asio::co_spawn(executor, receive(std::move(socket)), asio::detached);
    }
  } catch (const std::exception &e) {
    ERROR_LOG("listen error:{}", e.what());
  }
}

asio::awaitable<void> NetService::send_response(EndPoint endpoint, NetBuffer net_buffer) {
  co_await send_(endpoint, net_buffer);
}

asio::awaitable<void> NetService::receive(tcp::socket socket) {
  EndPoint peer_endpoint;
  peer_endpoint.from_asio_end_point(socket.remote_endpoint());
  PackageHeader header;
  uint64_t header_size = Serializer<PackageHeader>::get_serialize_size(header);
  constexpr size_t BUFFER_SIZE = 128_B;
  std::byte data[BUFFER_SIZE];
  try {
    uint64_t read_idx = 0;
    while (read_idx < header_size) {
      size_t n = co_await socket.async_read_some(asio::buffer(data + read_idx, header_size - read_idx),
                                                 asio::use_awaitable);
      read_idx += n;
    }
    int64_t pos = 0;
    Serializer<PackageHeader>::deserialize(header, data, header_size, pos);
    NetBuffer received_buffer(header.payload_len_);
    assert(header.checksum_ == PackageHeader::MAGIC_NUMBER);
    read_idx = 0;
    while (read_idx < header.payload_len_) {
      size_t n = co_await socket.async_read_some(asio::buffer(received_buffer.buffer_ + read_idx, received_buffer.buffer_len_ - read_idx),
                                                  asio::use_awaitable);
      read_idx += n;
    }
    assert(TLS_FRAMEWORK != nullptr);
    if (header.get_message_type() == PackageHeader::MessageType::REQUEST) {
      Expected<void> ret = reflect_commit_function(header, peer_endpoint, received_buffer.buffer_, received_buffer.buffer_len_);
      assert(ret);
    } else if (header.get_message_type() == PackageHeader::MessageType::RESPONSE) {
      LinkedCoroutine *need_awak_corotine = nullptr;
      {
        std::lock_guard<std::mutex> lg(lock_);
        auto iter = waiting_coros_.find(header.rpc_id_);
        if (waiting_coros_.end() != iter) [[unlikely]] {
          auto &mantain_info = iter->second;
          peer_endpoint.set_port(header.server_port_);
          mantain_info.coro_rpc_callback_->process_buffer_cb(peer_endpoint, received_buffer, need_awak_corotine);
          if (need_awak_corotine) {
            waiting_coros_.erase(iter);
          }
        } else {
          DEBUG_LOG("not find id:{}", header.rpc_id_);
        }
      }
      if (need_awak_corotine) {
        TLS_SCHEDULER->commit(need_awak_corotine);
      }
    } else {
      std::abort();
    }
    assert(pos == header_size);
    assert(header.payload_len_ <= BUFFER_SIZE - header_size);
    socket.close();
  } catch (const std::exception &e) {
    if (socket.is_open()) {
      DEBUG_LOG("connection closed");
      socket.close();
    }
    DEBUG_LOG(e.what());
  }
}

asio::awaitable<void> NetService::send_(EndPoint endpoints,
                                        NetBufferView net_buffer_view) {
  tcp::socket socket(io_ctx_);
  try {
    co_await socket.async_connect(endpoints.to_asio_endpoint(), asio::use_awaitable);
    co_await async_write(socket, asio::buffer(net_buffer_view.buffer_, net_buffer_view.buffer_len_), asio::use_awaitable);
    boost::system::error_code ec;
    auto ret = socket.shutdown(tcp::socket::shutdown_send, ec);
    if (ret || ec) [[unlikely]] {
      DEBUG_LOG("shutdown error, ret:{}, ec:{},{}", ret.value(), ec.value(), ec.what());
    }
    ret = socket.close(ec);
    if (ret || ec) [[unlikely]] {
      DEBUG_LOG("close error, ret:{}, ec:{},{}", ret.value(), ec.value(), ec.what());
    }
  } catch (const std::exception &e) {
    if (socket.is_open()) {
      socket.close();
    }
    DEBUG_LOG(e.what());
  }
}

asio::awaitable<void> NetService::timeout_(const uint64_t coro_id, const uint64_t timeout_ns) {
  asio::steady_timer timer(co_await asio::this_coro::executor);
  timer.expires_after(std::chrono::nanoseconds(timeout_ns));
  co_await timer.async_wait(asio::use_awaitable);
  LinkedCoroutine *need_awak_corotine = nullptr;
  {
    std::lock_guard<std::mutex> lg(lock_);
    auto iter = waiting_coros_.find(coro_id);
    if (waiting_coros_.end() != iter) [[unlikely]] {
      auto &mantain_info = iter->second;
      mantain_info.coro_rpc_callback_->timeout_cb();
      need_awak_corotine = mantain_info.coroutine_;
      if (need_awak_corotine) {
        waiting_coros_.erase(iter);
      }
    }
  }
  if (need_awak_corotine) {
    TLS_SCHEDULER->commit(need_awak_corotine);
  }
}

}