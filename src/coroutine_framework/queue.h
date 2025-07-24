#pragma once
#include <atomic>
#include <coroutine>
#include <assert.h>
#include <stdlib.h>
#include "local_var.h"

namespace G
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
  LinkedCoroutine &operator=(const LinkedCoroutine &) = delete;
  ~LinkedCoroutine() {
    prev_ = nullptr;
    next_ = nullptr;
    assert(in_queue_link_next_ == nullptr);
  }
  bool empty() { return prev_ == this; }
  void link_next(LinkedCoroutine &new_coroutine) {
    LinkedCoroutine *tmp_next = next_;
    next_ = &new_coroutine;
    new_coroutine.next_ = tmp_next;
    new_coroutine.prev_ = this;
    tmp_next->prev_ = &new_coroutine;
  }
  void remove_self() {
    LinkedCoroutine *tmp_prev = prev_;
    LinkedCoroutine *tmp_next = next_;
    if (!empty()) {
      tmp_prev->next_ = tmp_next;
      tmp_next->prev_ = tmp_prev;
      next_ = nullptr;
      prev_ = nullptr;
    }
  }
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
  void append_to_tail(LinkedCoroutine *coro_frame) noexcept {
    assert(coro_frame->in_queue_link_next_ == nullptr);
    if (empty()) {
      head_ = coro_frame;
      tail_ = coro_frame;
    } else {
      tail_->in_queue_link_next_ = coro_frame;
      tail_ = coro_frame;
    }
    size_++;
  }
  template <typename CONDITION>
  void fetch_nodes(CONDITION &&condition, CoroutineQueue *target_queue = nullptr) noexcept {
    LinkedCoroutine **last_node_next_ptr_ = &head_;
    LinkedCoroutine *iter = head_;
    while (iter) [[likely]] {
      auto tmp_next = iter->in_queue_link_next_;
      if (condition(iter)) [[likely]] {
        *last_node_next_ptr_ = iter->in_queue_link_next_;
        iter->in_queue_link_next_ = nullptr; // unlink from queue
        size_--;
        if (target_queue) {
          target_queue->append_to_tail(iter);
        }
      } else {
        last_node_next_ptr_ = &iter->in_queue_link_next_;
      }
      iter = tmp_next;
    }
    if (empty()) {
      head_ = nullptr;
      tail_ = nullptr;
    }
  }
  LinkedCoroutine *pop_from_head() noexcept {
    LinkedCoroutine *ret = head_;
    if (head_ == tail_) {
      assert(head_->in_queue_link_next_ == nullptr);
      head_ = nullptr;
      tail_ = nullptr;
    } else {
      head_ = head_->in_queue_link_next_;
    }
    size_--;
    ret->in_queue_link_next_ = nullptr;
    return ret;
  }
  LinkedCoroutine *head_;
  LinkedCoroutine *tail_;
  uint64_t size_;
};

}