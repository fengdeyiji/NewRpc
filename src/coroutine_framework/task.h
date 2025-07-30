#ifndef SRC_COROUTINE_FRAMEWORK_TASK_H
#define SRC_COROUTINE_FRAMEWORK_TASK_H

#include <coroutine>
#include <type_traits>
#include <utility>
#include <stdlib.h>
#include "coroutine_framework/local_var.h"
#include "coroutine_framework/net_module/net_define.h"
#include "queue.h"

namespace G {

void send_result_to_caller(const EndPoint &, NetBuffer &&);
void prepare_buffer(const ResponseInfo &response_info,
                    const uint64_t payload_len,
                    NetBuffer &net_buffer,
                    int64_t &pos);

template  <typename Ret, typename = void>
class CoroPromise;

template <typename CoroTask>
concept ValidCoroTask = requires {
  typename CoroTask::return_type;
  typename CoroTask::promise_type;
};

/**
 * @brief CoroTask is a kind of handle type of a coroutine.
 * 1. CoroTask has value sematics, support copy and move operation.(with very light action)
 * 2. CoroTask owned shared ref count of coroutine, when coroutine executed done, it's memory still remain if there is 
 * any CoroTask exists, or coroutine's memory will be destoryed immediately.
 * 3. CoroTask can be co_awit to monitor function call in coroutine body. 
 *
 * @tparam Ret - coroutine return type
 */
template <typename Ret>
struct CoroTask {
  using return_type = Ret;
  using promise_type = CoroPromise<Ret>;
  CoroTask(std::coroutine_handle<promise_type> h) : promise_{&h.promise()} {}
  CoroTask(const CoroTask<Ret> &rhs);
  CoroTask<Ret> &operator=(const CoroTask<Ret> &rhs);
  CoroTask(CoroTask<Ret> &&rhs);
  CoroTask<Ret> &operator=(CoroTask<Ret> &&rhs);
  ~CoroTask();
  bool done();
  void wait();
  void reset() noexcept;
  template <typename U = Ret>
  auto get_result() -> std::enable_if_t<!std::is_void_v<U>, U&>;
  struct GetPromiseOp {
    promise_type* p_;
    constexpr bool await_ready() noexcept;
    bool await_suspend(std::coroutine_handle<promise_type> h) noexcept;
    promise_type* await_resume() noexcept;
  };
  promise_type *promise_;
};

template <typename Ret>
struct InitialAwaitable {// 用于协程函数互调用时在初始挂起的处理
  constexpr bool await_ready() const noexcept { return false; }
  constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
  void await_resume() const noexcept;
  CoroPromise<Ret> *promise_;
};

template <typename MiddleResult>
struct MiddleAwaitable { // 用于协程函数在中间挂起的处理
  MiddleAwaitable(CoroTask<MiddleResult> &&task) : task_{std::move(task)} {}
  constexpr bool await_ready() noexcept { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept { return task_.promise_->handle_; }
  MiddleResult await_resume() noexcept { return task_.promise_->result_; }
  CoroTask<MiddleResult> task_;
};

template <typename Ret>
struct FinalAwaitable { // 用于协程函数在最终挂起的处理
  constexpr bool await_ready() const noexcept { return false; }
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> this_coro) const noexcept;
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
  template <typename ASYNC>
  auto await_transform(ASYNC &&task) {
    if constexpr (ValidCoroTask<ASYNC>) { // 协程链式调用
      link_next(*task.promise_);
      task.promise_->coro_local_var_ = coro_local_var_;
      return MiddleAwaitable<typename ASYNC::return_type>{std::move(task)};
    } else { // 异步动作调用
      return std::forward<ASYNC>(task);
    }
  }
  void return_value(Ret result) { result_ = std::move(result); }
  Ret result_;
};

}

#ifndef SRC_COROUTINE_FRAMEWORK_TASK_H_IPP
#define SRC_COROUTINE_FRAMEWORK_TASK_H_IPP
#include "task.ipp"
#endif

#endif