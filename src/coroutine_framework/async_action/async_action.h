#ifndef SRC_COROUTINE_FRAMEWORK_ASYNC_ACTION_H
#define SRC_COROUTINE_FRAMEWORK_ASYNC_ACTION_H
#include "coroutine_framework/net_module/net_define.h"
#include "coroutine_framework/queue.h"
#include "coroutine_framework/net_module/rpc_mapper.h"
#include <utility>

namespace G
{

// namespace combine
// {

// enum class Operator {
//   OR = 1,
//   AND = 2,
// };

// struct IAsyncCallBack {
//   IAsyncCallBack() : is_finished_{false}, concurrent_controlloer_{nullptr} {}
//   constexpr bool await_ready() const noexcept { return false; }
//   virtual void acquire_lock() const {
//     if (concurrent_controlloer_) { // if not null, means there is a combined async operations
//       combined_async_operations_->async_start();
//     }
//   }
//   virtual void async_finish() {
//     is_finished_ = true;
//   }
//   virtual void cancel() const = 0;
//   bool is_finished() const { return is_finished_; }
//   bool is_finished_;
//   ConCurrentController *concurrent_controlloer_;
// };

// template <Operator OPERATOR, typename T, typename U>
// struct AsyncOperations;

// template <typename T>
// struct AsyncSingleOperation_CRTP : public IAsyncCallBack {
//   AsyncSingleOperation_CRTP() : IAsyncCallBack{} {}
//   template <typename U>
//   AsyncOperations<Operator::OR, T, U> operator||(AsyncSingleOperation_CRTP<U> &&rhs) {
//     return {std::move(*((T *)this)), std::move(rhs)};
//   }
//   template <typename U>
//   AsyncOperations<Operator::AND, T, U> operator&&(AsyncSingleOperation_CRTP<U> &&rhs) {
//     return {std::move(static_cast<T&&>(*this)), std::move(static_cast<U&&>(rhs))};
//   }
// };

// template <Operator OPERATOR, typename T, typename U>
// struct AsyncOperations {
//   using tuple_types = std::tuple<T, U>;
//   AsyncOperations(T &&arg1, U &&arg2)
//   : async_pair_{std::move(arg1), std::move(arg2)} {}
//   bool is_finished() const noexcept {
//     bool ret = false;
//     if constexpr (OPERATOR == Operator::OR) {
//       ret = async_pair_.first.is_finished() || async_pair_.second.is_finished();
//     } else if constexpr (OPERATOR == Operator::AND) {
//       ret = async_pair_.first.is_finished() && async_pair_.second.is_finished();
//     } else {
//       static_assert(false, "not support");
//     }
//     return ret;
//   }
//   std::pair<T, U> async_pair_;
// };

// }

struct co_sleep {
  co_sleep(const uint64_t sleep_ts) : sleep_ts_{sleep_ts} {}
  constexpr bool await_ready() const noexcept { return false; }
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise> handle);
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
  void timeout(const uint64_t timeout) { timeout_ = timeout; }
  template <std::ranges::range RANGE>
  void on(const RANGE &endpoints);
  void on(const EndPoint &endpoint);
  template <typename FUNC>
  void on_each_result(FUNC &&func) { on_each_function_ = func; }
  template <typename ...Args>
  void with_args(Args &&...args);
  template <typename Promise>
  void await_suspend(std::coroutine_handle<Promise> handle);
  virtual void process_buffer_cb(const EndPoint peer_endpoint,
                                 const NetBuffer &net_buffer,
                                 LinkedCoroutine *&coroutine) override;
  virtual void timeout_cb() override;
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

#ifndef SRC_COROUTINE_FRAMEWORK_ASYNC_ACTION_H_IPP
#define SRC_COROUTINE_FRAMEWORK_ASYNC_ACTION_H_IPP
#include "async_action.ipp"
#endif
#endif