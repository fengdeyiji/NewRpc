#pragma once
#include <condition_variable>
#include <coroutine>
#include <mutex>
#include <type_traits>
#include <utility>
#include <stdlib.h>
#include "coroutine_framework/local_var.h"
#include "coroutine_framework/net_module/net_define.h"
#include "literal.h"
#include "mechanism/serialization.hpp"
#include "queue.h"
#include "log/logger.h"

namespace G {

void send_result_to_caller(const EndPoint &, NetBuffer &&);
void prepare_buffer(const ResponseInfo &response_info,
                    const uint64_t payload_len,
                    NetBuffer &net_buffer,
                    int64_t &pos);

struct StackAreaAllocator {
  static StackAreaAllocator &get_instance() {
    static StackAreaAllocator alloc{1_MiB};
    return alloc;
  }
  StackAreaAllocator(const uint64_t size)
  : stack_buffer_{nullptr}, allocated_location_{0}, size_{size}, alloc_times_{0} {
    stack_buffer_ = new std::byte[size];
  }
  ~StackAreaAllocator() {
    if (nullptr != stack_buffer_) [[likely]] {
      delete[] stack_buffer_;
      stack_buffer_ = nullptr;
    }
  }
  StackAreaAllocator(const StackAreaAllocator &) = delete;
  StackAreaAllocator(StackAreaAllocator &&) = delete;
  auto &operator=(const StackAreaAllocator &) = delete;
  auto &operator=(StackAreaAllocator &&) = delete;
  void *alloc(const uint64_t size) {
    // void *ret = stack_buffer_ + allocated_location_;
    // allocated_location_ += size;
    // alloc_times_++;
    // return ret;
    return ::operator new(size);
  }
  void free(void* ptr, size_t size) {
    // assert(ptr == stack_buffer_ + allocated_location_ - size);
    // allocated_location_ -= size;
    ::operator delete(ptr);
  }
  std::byte *stack_buffer_;
  uint64_t allocated_location_;
  uint64_t size_;
  uint64_t alloc_times_;
};

template  <typename Ret, typename = void>
class CoroPromise;

template <typename CoroTask>
concept ValidCoroTask = requires {
  typename CoroTask::return_type;
  typename CoroTask::promise_type;
};

template <typename Ret>
struct CoroTask {
  using return_type = Ret;
  using promise_type = CoroPromise<Ret>;
  CoroTask(std::coroutine_handle<promise_type> h) : promise_{&h.promise()} {}
  CoroTask(const CoroTask<Ret> &rhs) {
    promise_ = rhs.promise_;
    ((LinkedCoroutine *)promise_)->ref_cnt_.inc();
  }
  CoroTask<Ret> &operator=(const CoroTask<Ret> &rhs) {
    if (this != &rhs) [[likely]] {
      promise_ = rhs.promise_;
      ((LinkedCoroutine *)promise_)->ref_cnt_.inc();
    }
    return *this;
  }
  CoroTask(CoroTask<Ret> &&rhs) : promise_{rhs.promise_} { rhs.promise_ = nullptr; }
  CoroTask<Ret> &operator=(CoroTask<Ret> &&rhs) {
    if (this != &rhs) [[likely]] {
      rhs.promise_ = nullptr;
    }
    return *this;
  }
  ~CoroTask();
  bool done();
  void wait() {
    LinkedCoroutine *coro = promise_;
    std::unique_lock<std::mutex> lg(coro->lock_);
    coro->cv_.wait(lg, [coro]() { return coro->done_flag_; });
  }
  void reset() noexcept { promise_ = nullptr; }
  template <typename U = Ret>
  auto get_result() -> std::enable_if_t<!std::is_void_v<U>, U&>;
  struct GetPromiseOp {
    promise_type* p_;
    constexpr bool await_ready() noexcept { return false; }
    bool await_suspend(std::coroutine_handle<promise_type> h) noexcept {
      p_ = &h.promise();
      return false;
    }
    promise_type* await_resume() noexcept { return p_; } // 返回 promise
  };
  promise_type *promise_;
};

template <typename Ret>
struct InitialAwaitable {
  constexpr bool await_ready() const noexcept { return false; }
  constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
  void await_resume() const noexcept {
    if (nullptr == promise_->coro_local_var_) [[unlikely]] {
      promise_->coro_local_var_ = new CoroLocalVar{promise_->ref_cnt_};
    }
  }
  CoroPromise<Ret> *promise_;
};

template <typename MiddleResult>
struct MiddleAwaitable { // 用于协程函数在中间挂起的处理
  MiddleAwaitable(CoroTask<MiddleResult> &&task) : task_{std::move(task)} {}
  constexpr bool await_ready() noexcept { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
    return task_.promise_->handle_;
  }
  MiddleResult await_resume() noexcept {
    return task_.promise_->result_;
  }
  CoroTask<MiddleResult> task_;
};

template <typename Ret>
struct FinalAwaitable { // 用于协程函数在最终挂起的处理
  constexpr bool await_ready() const noexcept { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> this_coro) const noexcept {
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
  void await_resume() const noexcept { std::abort(); }// final suspend挂起后，await_resume便不可再被调用
  CoroPromise<Ret> &promise_;
};

template <typename Ret>
struct CoroPromise<Ret, std::enable_if_t<std::is_same_v<Ret, void>, void>> : public LinkedCoroutine {
  CoroPromise() : LinkedCoroutine{} { handle_ = std::coroutine_handle<CoroPromise<Ret>>::from_promise(*this); }
  CoroPromise(const CoroPromise<Ret> &) = delete; // disallow copy
  CoroPromise<Ret> &operator=(const CoroPromise<Ret> &) = delete; // disallow copy
  CoroPromise(CoroPromise<Ret> &&) = delete; // disallow move
  CoroPromise<Ret> &operator=(CoroPromise<Ret> &&) = delete; // disallow move
  ~CoroPromise() {}
  InitialAwaitable<Ret> initial_suspend() noexcept { return {this}; }
  FinalAwaitable<Ret> final_suspend() noexcept { return {*this}; }
  void unhandled_exception() { std::abort(); }
  auto get_return_object() { return CoroTask{std::coroutine_handle<CoroPromise<Ret>>::from_promise(*this)}; }
  template <typename CoroTask>
  auto await_transform(CoroTask &&task) {
    if constexpr (ValidCoroTask<CoroTask>) {
      link_next(*task.promise_);
      task.promise_->coro_local_var_ = coro_local_var_;
      return MiddleAwaitable<typename CoroTask::return_type>{std::move(task)};
    } else {
      return std::forward<CoroTask>(task);
    }
  }
  void return_void() {}
};

template <typename Ret>
struct CoroPromise<Ret, std::enable_if_t<!std::is_same_v<Ret, void>, void>> : public LinkedCoroutine {
  CoroPromise() : LinkedCoroutine{} { handle_ = std::coroutine_handle<CoroPromise<Ret>>::from_promise(*this); }
  CoroPromise(const CoroPromise<Ret> &) = delete; // disallow copy
  CoroPromise<Ret> &operator=(const CoroPromise<Ret> &) = delete; // disallow copy
  CoroPromise(CoroPromise<Ret> &&) = delete; // disallow move
  CoroPromise<Ret> &operator=(CoroPromise<Ret> &&) = delete; // disallow move
  ~CoroPromise() {}
  InitialAwaitable<Ret> initial_suspend() noexcept { return {this}; }
  FinalAwaitable<Ret> final_suspend() noexcept { return {*this}; }
  void unhandled_exception() { std::abort(); }
  auto get_return_object() { return CoroTask{std::coroutine_handle<CoroPromise<Ret>>::from_promise(*this)}; }
  template <typename CoroTask>
  auto await_transform(CoroTask &&task) {
    if constexpr (ValidCoroTask<CoroTask>) {
      link_next(*task.promise_);
      task.promise_->coro_local_var_ = coro_local_var_;
      return MiddleAwaitable<typename CoroTask::return_type>{std::move(task)};
    } else {
      return std::forward<CoroTask>(task);
    }
  }
  void return_value(Ret result) { result_ = std::move(result); }
  Ret result_;
};

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
  return promise_->result_;
}



}