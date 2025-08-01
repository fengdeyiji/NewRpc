#ifndef SRC_COROUTINE_FRAMEWORK_QUEUE_IPP
#define SRC_COROUTINE_FRAMEWORK_QUEUE_IPP

#ifndef SRC_COROUTINE_FRAMEWORK_QUEUE_H_IPP
#define SRC_COROUTINE_FRAMEWORK_QUEUE_H_IPP
#include "queue.h"
#endif

namespace ToE
{

inline LinkedCoroutine::~LinkedCoroutine() {
  prev_ = nullptr;
  next_ = nullptr;
  assert(in_queue_link_next_ == nullptr);
}

inline void LinkedCoroutine::link_next(LinkedCoroutine &new_coroutine) {
  LinkedCoroutine *tmp_next = next_;
  next_ = &new_coroutine;
  new_coroutine.next_ = tmp_next;
  new_coroutine.prev_ = this;
  tmp_next->prev_ = &new_coroutine;
}

inline void LinkedCoroutine::remove_self() {
  LinkedCoroutine *tmp_prev = prev_;
  LinkedCoroutine *tmp_next = next_;
  if (!empty()) {
    tmp_prev->next_ = tmp_next;
    tmp_next->prev_ = tmp_prev;
    next_ = nullptr;
    prev_ = nullptr;
  }
}

inline void CoroutineQueue::append_to_tail(LinkedCoroutine *coro_frame) noexcept {
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
void CoroutineQueue::fetch_nodes(CONDITION &&condition, CoroutineQueue *target_queue) noexcept {
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

inline LinkedCoroutine *CoroutineQueue::pop_from_head() noexcept {
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

}
#endif