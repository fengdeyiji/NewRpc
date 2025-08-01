#ifndef SRC_COROUTINE_FRAMEWORK_TASK_IPP
#define SRC_COROUTINE_FRAMEWORK_TASK_IPP

#ifndef SRC_COROUTINE_FRAMEWORK_TASK_H_IPP
#define SRC_COROUTINE_FRAMEWORK_TASK_H_IPP
#include "task.h"
#include <condition_variable>
#include <mutex>
#include "mechanism/serialization.hpp"
#include "log/logger.h"
#endif

namespace ToE
{

template <typename Ret>
CoroTask<Ret>::CoroTask(const CoroTask<Ret> &rhs) {
  promise_ = rhs.promise_;
  ((LinkedCoroutine *)promise_)->ref_cnt_.inc();
}

template <typename Ret>
CoroTask<Ret> &CoroTask<Ret>::operator=(const CoroTask<Ret> &rhs) {
  if (this != &rhs) [[likely]] {
    promise_ = rhs.promise_;
    ((LinkedCoroutine *)promise_)->ref_cnt_.inc();
  }
  return *this;
}

template <typename Ret>
CoroTask<Ret>::CoroTask(CoroTask<Ret> &&rhs) : promise_{rhs.promise_} { rhs.promise_ = nullptr; }

template <typename Ret>
CoroTask<Ret> &CoroTask<Ret>::operator=(CoroTask<Ret> &&rhs) {
  if (this != &rhs) [[likely]] {
    rhs.promise_ = nullptr;
  }
  return *this;
}

template <typename Ret>
void CoroTask<Ret>::wait() {
  LinkedCoroutine *coro = promise_;
  std::unique_lock<std::mutex> lg(coro->lock_);
  coro->cv_.wait(lg, [coro]() { return coro->done_flag_; });
}

template <typename Ret>
void CoroTask<Ret>::reset() noexcept { promise_ = nullptr; }

template <typename Ret>
constexpr bool CoroTask<Ret>::GetPromiseOp::await_ready() noexcept { return false; }

template <typename Ret>
bool CoroTask<Ret>::GetPromiseOp::await_suspend(std::coroutine_handle<promise_type> h) noexcept {
  p_ = &h.promise();
  return false;
}

template <typename Ret>
CoroTask<Ret>::promise_type* CoroTask<Ret>::GetPromiseOp::await_resume() noexcept { return p_; }

template <typename Ret>
CoroTask<Ret>::~CoroTask() {
  if (promise_ && promise_->ref_cnt_.dec() == 0) {
    promise_->handle_.destroy();
  }
}

template <typename Ret>
bool CoroTask<Ret>::done() {
  return promise_->handle_.done();
}

template <typename Ret>
template <typename U>
auto CoroTask<Ret>::get_result() -> std::enable_if_t<!std::is_void_v<U>, U&> {
  assert(done());
  return promise_->result_;
}

template <typename Ret>
void InitialAwaitable<Ret>::await_resume() const noexcept {
  if (nullptr == promise_->coro_local_var_) [[unlikely]] {
    promise_->coro_local_var_ = new CoroLocalVar{promise_->ref_cnt_};
  }
}

template <typename Ret>
std::coroutine_handle<> FinalAwaitable<Ret>::await_suspend(std::coroutine_handle<> this_coro) const noexcept {
  std::coroutine_handle<> ret = std::noop_coroutine(); // noop协程的resume动作不会导致未定义行为，线程控制权将直接返回caller
  if (!promise_.empty()) { // 子协程在最后一次挂起时，将线程转移到母协程(如存在)的挂起点上
    ret = promise_.prev_->handle_;
    promise_.remove_self();
  } else {
    if (promise_.frame_running_cnt_) [[likely]] {
      (*promise_.frame_running_cnt_)--;
    }
    if (promise_.coro_local_var_ && promise_.coro_local_var_->response_info_) [[unlikely]] {
      const ResponseInfo &response_info = *promise_.coro_local_var_->response_info_;
      if constexpr (!std::is_void_v<Ret>) {
        int64_t serialize_size = Serializer<Ret>::get_serialize_size(promise_.result_);
        NetBuffer buffer;
        int64_t pos = 0;
        prepare_buffer(response_info, serialize_size, buffer, pos);
        Serializer<Ret>::serialize(promise_.result_, buffer.buffer_, buffer.buffer_len_, pos);
        send_result_to_caller(response_info.response_to_end_point_, std::move(buffer));
      }
    } else {
      std::unique_lock<std::mutex> lg(promise_.lock_);
      promise_.done_flag_ = true;
      promise_.cv_.notify_all();
      DEBUG_LOG("notify");
    }
    delete promise_.coro_local_var_;
    promise_.coro_local_var_ = nullptr;
    assert(this_coro.done() == true);
    if (promise_.ref_cnt_.dec() == 0) {
      promise_.handle_.destroy();
    }
  }
  return ret;
}

}

#endif