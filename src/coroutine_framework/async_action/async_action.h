#pragma once
#include "coroutine_framework/net_module/net_define.h"
#include "coroutine_framework/net_module/net_service.h"
#include "coroutine_framework/queue.h"
#include "log/logger.h"
#include "coroutine_framework/utils.h"
#include "coroutine_framework/time_module/time_service.h"
#include "coroutine_framework/scheduler.h"
#include "coroutine_framework/net_module/rpc_mapper.h"
#include "coroutine_framework/net_module/rpc_struct.h"
#include "mechanism/serialization.hpp"
#include <utility>
#include "coroutine_framework/net_module/rpc_mapper.h"

namespace G
{

struct co_sleep {
  co_sleep(const uint64_t sleep_ts) : sleep_ts_{sleep_ts} {}
  bool await_ready() { return false; }
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise> handle) {
    auto& promise = handle.promise();
    promise.coro_local_var_->wake_up_ts_ = SteadyClockTime::now() + sleep_ts_;
    promise.sync_release();
    auto &time_module = TLS_FRAMEWORK->get_time_module();
    time_module.register_frame(&promise);
  }
  void await_resume() {}
private:
  const uint64_t sleep_ts_;
};

template <auto FUNC_PTR>
struct RpcBase : public CoRpcCallBack {
  static constexpr size_t RPC_ID = FunctionToID<FUNC_PTR>::value;
  using CORO_RET = FunctionToID<FUNC_PTR>::FunctionTraits::return_type;
  using RET = CORO_RET::return_type;
  static constexpr uint64_t LOCAL_BUFFER_SIZE = 1_KiB;
  RpcBase()
  : endpoints_{},
  finish_condition_{FinishCondition::WHEN_ALL},
  timeout_{1_s},
  func_id_{RPC_ID},
  send_buffer_{},
  total_serialize_size_{0},
  received_results_{},
  on_each_function_{},
  coroutine_{nullptr} {}
  RpcBase(RpcBase<FUNC_PTR> &&rhs)
  : endpoints_{std::move(rhs.endpoints_)},
  finish_condition_{rhs.finish_condition_},
  timeout_{rhs.timeout_},
  func_id_{rhs.func_id_},
  send_buffer_{std::move(rhs.send_buffer_)},
  total_serialize_size_{rhs.total_serialize_size_},
  received_results_{std::move(rhs.received_results_)},
  on_each_function_{std::move(rhs.on_each_function_)},
  coroutine_{rhs.coroutine_} {}
  void timeout(const uint64_t timeout) {
    timeout_ = timeout;
  }
  template <std::ranges::range RANGE>
  void on(const RANGE &endpoints) {
    endpoints_.reserve(std::ranges::size(endpoints));
    for (auto &endpoint : endpoints) {
      endpoints_.emplace_back(endpoint, false);
    }
    received_results_.reserve(std::ranges::size(endpoints));
  }
  void on(const EndPoint &endpoint) {
    endpoints_.reserve(1);
    endpoints_.emplace_back(endpoint, false);
    received_results_.reserve(1);
  }
  template <typename FUNC>
  void on_each_result(FUNC &&func) {
    on_each_function_ = func;
  }
  template <typename ...Args>
  void with_args(Args &&...args) {
    using ArgsTuple = FunctionToID<FUNC_PTR>::FunctionTraits::args_tuple;
    ArgsTuple args_tuple{std::forward<Args>(args)...};
    PackageHeader header_placeholder{1, func_id_, 0, 0, 0}; // placeholder
    uint64_t header_serialize_size = Serializer<PackageHeader>::get_serialize_size(header_placeholder);
    total_serialize_size_ = header_serialize_size;
    for_each_tuple([this](const auto &arg) {
      total_serialize_size_ += Serializer<DECAY_T(arg)>::get_serialize_size(arg);
    }, args_tuple);
    assert(total_serialize_size_ <= LOCAL_BUFFER_SIZE); // FIXME: use dynamic buffer if too large
    new (&send_buffer_) NetBuffer{total_serialize_size_};
    int64_t pos = header_serialize_size;
    for_each_tuple([this, &pos](const auto &arg) {
      Serializer<DECAY_T(arg)>::serialize(arg, send_buffer_.buffer_, send_buffer_.buffer_len_, pos);
    }, args_tuple);
    assert(pos == total_serialize_size_);
  }
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise> handle) {
    auto &net_module = TLS_FRAMEWORK->get_net_module();
    Promise *promise = &handle.promise();
    coroutine_ = promise;
    uint64_t coro_id = promise->coro_local_var_->coro_id_;
    PackageHeader header{1, func_id_, coro_id, net_module.get_listen_port(), 0};
    header.set_message_type(PackageHeader::MessageType::REQUEST);
    uint64_t header_serialize_size = Serializer<PackageHeader>::get_serialize_size(header);
    header.payload_len_ = total_serialize_size_ - header_serialize_size;
    int64_t pos = 0;
    Serializer<PackageHeader>::serialize(header, send_buffer_.buffer_, send_buffer_.buffer_len_, pos);
    assert(pos == header_serialize_size);
    promise->sync_release();
    net_module.commit_send_request_task(
      coro_id,
      endpoints_,
      NetBufferView{send_buffer_},
      timeout_,
      promise,
      this);
  }
  void process_buffer_cb(const EndPoint peer_endpoint, const NetBuffer &net_buffer, LinkedCoroutine *&coroutine) override {
    auto total = endpoints_.size();
    Expected<RET> rpc_ret;
    RET &return_value = rpc_ret.value();
    bool need_awaken = false;
    Serializer<RET>::deserialize(return_value, net_buffer.buffer_, net_buffer.buffer_len_);
    if (on_each_function_) {
      on_each_function_(peer_endpoint, return_value, need_awaken);
    }
    received_results_.push_back({peer_endpoint, Expected<RET>{std::move(rpc_ret)}});
    for (auto &endpoint_with_flag : endpoints_) {
      if (endpoint_with_flag.first == peer_endpoint) {
        endpoint_with_flag.second = true;
        break;
      }
    }
    auto size = received_results_.size();
    switch (finish_condition_) {
      case FinishCondition::WHEN_ANY:
        need_awaken = true;
        break;
      case FinishCondition::WHEN_ALL:
        need_awaken = (size == total);
        break;
      case FinishCondition::WHEN_MAJORITY:
        need_awaken = (size == (total / 2 + 1));
        break;
      default:
        std::abort();
    }
    if (need_awaken) {
      coroutine = coroutine_;
    }
  }
  void timeout_cb() override {
    for (auto &endpoint_with_flag : endpoints_) {
      if (!endpoint_with_flag.second) {
        received_results_.push_back({endpoint_with_flag.first, UnExpected{Error::RPC_TIMEOUT}});
      }
    }
    assert(received_results_.size() == endpoints_.size());
  }
  std::vector<std::pair<EndPoint, bool/*receive flag*/>> endpoints_;
  FinishCondition finish_condition_;
  uint64_t timeout_;
  uint16_t func_id_;
  NetBuffer send_buffer_;
  uint64_t total_serialize_size_;
  std::vector<std::pair<EndPoint, Expected<RET>>> received_results_;
  std::function<void(const EndPoint &/*response from*/,
                     RET &,/*response result*/
                     bool &/*need_resume*/)> on_each_function_;
  LinkedCoroutine *coroutine_;
};

template <auto FUNC_PTR, bool MULTI_RPC = false>
struct co_rpc {
  co_rpc() : rpc_base_{} {}
  co_rpc(RpcBase<FUNC_PTR> &&rhs) : rpc_base_{std::move(rhs)} {}
  constexpr bool await_ready() const noexcept { return false; }
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise> handle) { return rpc_base_.await_suspend(handle); }
  auto await_resume() {
    if constexpr (MULTI_RPC) {
      return std::move(rpc_base_.received_results_);
    } else {
      return std::move(rpc_base_.received_results_[0].second);
    }
  }
  template <typename ...Args>
  co_rpc<FUNC_PTR, MULTI_RPC> with_args(Args &&...args) {
    rpc_base_.with_args(std::forward<Args>(args)...);
    return co_rpc<FUNC_PTR, MULTI_RPC>{std::move(rpc_base_)};
  }
  co_rpc<FUNC_PTR, MULTI_RPC> timeout(const uint64_t timeout) {
    rpc_base_.timeout(timeout);
    return co_rpc<FUNC_PTR, MULTI_RPC>{std::move(rpc_base_)};
  }
  template <std::ranges::range RANGE>
  co_rpc<FUNC_PTR, true> on(const RANGE &endpoints) {
    rpc_base_.on(endpoints);
    return co_rpc<FUNC_PTR, true>{std::move(rpc_base_)};
  }
  co_rpc<FUNC_PTR, false> on(const EndPoint &endpoint) {
    rpc_base_.on(endpoint);
    return co_rpc<FUNC_PTR, false>{std::move(rpc_base_)};
  }
  co_rpc<FUNC_PTR, MULTI_RPC> when_all() noexcept {
    rpc_base_.finish_condition_ = FinishCondition::WHEN_ALL;
    return co_rpc<FUNC_PTR, MULTI_RPC>{std::move(rpc_base_)};
  }
  co_rpc<FUNC_PTR, MULTI_RPC> when_any() noexcept {
    rpc_base_.finish_condition_ = FinishCondition::WHEN_ANY;
    return co_rpc<FUNC_PTR, MULTI_RPC>{std::move(rpc_base_)};
  }
  co_rpc<FUNC_PTR, MULTI_RPC> when_majority() noexcept {
    rpc_base_.finish_condition_ = FinishCondition::WHEN_MAJORITY;
    return co_rpc<FUNC_PTR, MULTI_RPC>{std::move(rpc_base_)};
  }
  template <typename FUNC>
  co_rpc<FUNC_PTR, MULTI_RPC> on_each_result(FUNC &&func) noexcept {
    rpc_base_.on_each_function_ = std::forward<FUNC>(func);
    return co_rpc<FUNC_PTR, MULTI_RPC>{std::move(rpc_base_)};
  }
private:
  RpcBase<FUNC_PTR> rpc_base_;
};

}