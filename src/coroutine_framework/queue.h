#ifndef SRC_COROUTINE_FRAMEWORK_QUEUE_H
#define SRC_COROUTINE_FRAMEWORK_QUEUE_H
#include <atomic>
#include <coroutine>
#include <assert.h>
#include <stdlib.h>
#include "local_var.h"

namespace ToE
{

struct LinkedCoroutine {
  LinkedCoroutine()
  : prev_{this},
  next_{this},
  in_queue_link_next_{nullptr},
  sync_cnt_{0},
  handle_{},
  coro_local_var_{nullptr},
  ref_cnt_{},
  lock_{},
  done_flag_{false},
  cv_{},
  frame_running_cnt_{nullptr} {};
  LinkedCoroutine(const LinkedCoroutine &) = delete;
  LinkedCoroutine(LinkedCoroutine &&) = delete;
  LinkedCoroutine &operator=(const LinkedCoroutine &) = delete;
  LinkedCoroutine &operator=(LinkedCoroutine &&) = delete;
  ~LinkedCoroutine();
  bool empty() { return prev_ == this; }
  void link_next(LinkedCoroutine &new_coroutine);
  void remove_self();
  void sync_release() noexcept { sync_cnt_.fetch_add(1, std::memory_order_release); }
  // return value for tell compiler not optimize load operation, but not used
  uint64_t sync_acquire() noexcept { return sync_cnt_.load(std::memory_order_acquire); }
  LinkedCoroutine *prev_;
  LinkedCoroutine *next_;
  LinkedCoroutine *in_queue_link_next_;
  std::atomic<uint64_t> sync_cnt_;
  std::coroutine_handle<> handle_;
  CoroLocalVar *coro_local_var_; // CAUTIONS: can not be accessed directly!
  RefCount ref_cnt_;
  std::mutex lock_;
  bool done_flag_;
  std::condition_variable cv_;
  std::atomic<uint64_t> *frame_running_cnt_;
};

struct CoroutineQueue {
  CoroutineQueue() : head_{nullptr}, tail_{nullptr}, size_{0} {}
  uint64_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0; }
  void append_to_tail(LinkedCoroutine *coro_frame) noexcept;
  template <typename CONDITION>
  void fetch_nodes(CONDITION &&condition, CoroutineQueue *target_queue = nullptr) noexcept;
  LinkedCoroutine *pop_from_head() noexcept;
  LinkedCoroutine *head_;
  LinkedCoroutine *tail_;
  uint64_t size_;
};
}

#ifndef SRC_COROUTINE_FRAMEWORK_QUEUE_H_IPP
#define SRC_COROUTINE_FRAMEWORK_QUEUE_H_IPP
#include "queue.ipp"
#endif

#endif