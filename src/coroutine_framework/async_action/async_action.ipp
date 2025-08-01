#ifndef SRC_COROUTINE_FRAMEWORK_ASYNC_ACTION_IPP
#define SRC_COROUTINE_FRAMEWORK_ASYNC_ACTION_IPP
#include "coroutine_framework/net_module/rpc_struct.h"
#include "mechanism/serialization.hpp"
#include "coroutine_framework/utils.h"
#include "coroutine_framework/time_module/time_service.h"
#include "coroutine_framework/scheduler.h"
#include "coroutine_framework/net_module/net_service.h"
#ifndef SRC_COROUTINE_FRAMEWORK_ASYNC_ACTION_H_IPP
#define SRC_COROUTINE_FRAMEWORK_ASYNC_ACTION_H_IPP
#include "async_action.h"
#endif

namespace ToE
{

template <typename Promise>
void co_sleep::await_suspend(std::coroutine_handle<Promise> handle) {
  auto& promise = handle.promise();
  promise.coro_local_var_->wake_up_ts_ = SteadyClockTime::now() + sleep_ts_;
  promise.sync_release();
  auto &time_module = TLS_FRAMEWORK->get_time_module();
  time_module.register_frame(&promise);
}

template <auto FUNC_PTR>
template <std::ranges::range RANGE>
void RpcBase<FUNC_PTR>::on(const RANGE &endpoints) {
  endpoints_.reserve(std::ranges::size(endpoints));
  for (auto &endpoint : endpoints) {
    endpoints_.emplace_back(endpoint, false);
  }
  received_results_.reserve(std::ranges::size(endpoints));
}

template <auto FUNC_PTR>
void RpcBase<FUNC_PTR>::on(const EndPoint &endpoint) {
  endpoints_.reserve(1);
  endpoints_.emplace_back(endpoint, false);
  received_results_.reserve(1);
}

template <auto FUNC_PTR>
template <typename ...Args>
void RpcBase<FUNC_PTR>::with_args(Args &&...args) {
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

template <auto FUNC_PTR>
template <typename Promise>
void RpcBase<FUNC_PTR>::await_suspend(std::coroutine_handle<Promise> handle) {
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
  net_module.commit_send_request_task(coro_id,
                                      endpoints_,
                                      NetBufferView{send_buffer_},
                                      timeout_,
                                      promise,
                                      this);
}

template <auto FUNC_PTR>
void RpcBase<FUNC_PTR>::process_buffer_cb(const EndPoint peer_endpoint,
                                          const NetBuffer &net_buffer,
                                          LinkedCoroutine *&coroutine) {
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

template <auto FUNC_PTR>
void RpcBase<FUNC_PTR>::timeout_cb() {
  for (auto &endpoint_with_flag : endpoints_) {
    if (!endpoint_with_flag.second) {
      received_results_.push_back({endpoint_with_flag.first, UnExpected{Error::RPC_TIMEOUT}});
    }
  }
  assert(received_results_.size() == endpoints_.size());
}

}

#endif