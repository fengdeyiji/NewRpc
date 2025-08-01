#pragma once
#include "coroutine_framework/queue.h"
#include "queue.h"
#include <condition_variable>

namespace ToE
{

struct CommonExecuteModule {
  CommonExecuteModule()
  : lock_{}, cv_{}, coroutine_queue_{} {}
  void commit(LinkedCoroutine *coro_frame) noexcept {
    {
      std::unique_lock lk(lock_);
      coroutine_queue_.append_to_tail(coro_frame);
    }
    cv_.notify_one();
  }
protected:
  mutable std::mutex lock_;
  std::condition_variable cv_;
  CoroutineQueue coroutine_queue_;
};

extern thread_local CommonExecuteModule *TLS_SCHEDULER;

}