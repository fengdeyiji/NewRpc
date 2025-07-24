#include "scheduler.h"

namespace G {

thread_local CommonExecuteModule *TLS_SCHEDULER = nullptr;
thread_local CoroFrameWork *TLS_FRAMEWORK = nullptr;

template <typename TimeModule,  typename LockModule,  typename NetModule,  typename DiskModule>
CoroScheduler<TimeModule, LockModule, NetModule, DiskModule>::~CoroScheduler() {
  stop();
  wait();
}

template <typename TimeModule,  typename LockModule,  typename NetModule,  typename DiskModule>
void CoroScheduler<TimeModule, LockModule, NetModule, DiskModule>::start() {
  try {
    TLS_SCHEDULER = this;
    TLS_FRAMEWORK = this;
    time_module_.start();
    net_module_.start();
    stop_flag_.store(false, std::memory_order_release);
    for (int64_t idx = 0; idx < worker_thread_num_; ++idx) {
      workers_.emplace_back([this,
                             scheduler = TLS_SCHEDULER,
                             framework = TLS_FRAMEWORK] {
        TLS_SCHEDULER = scheduler;
        TLS_FRAMEWORK = framework;
        this->loop_();
      });
    }
    TLS_SCHEDULER = nullptr;
    TLS_FRAMEWORK = nullptr;
    DEBUG_LOG("CoroScheduler started");
  } catch (const std::exception &e) {
    stop();
    wait();
    TLS_SCHEDULER = nullptr;
    TLS_FRAMEWORK = nullptr;
    throw e;
  }
}

template <typename TimeModule,  typename LockModule,  typename NetModule,  typename DiskModule>
void CoroScheduler<TimeModule, LockModule, NetModule, DiskModule>::stop() noexcept {
  stop_flag_.store(true, std::memory_order_release);
  time_module_.stop();
  net_module_.stop();
  cv_.notify_all();
  DEBUG_LOG("CoroScheduler stopped");
}

template <typename TimeModule,  typename LockModule,  typename NetModule,  typename DiskModule>
void CoroScheduler<TimeModule, LockModule, NetModule, DiskModule>::wait() noexcept {
  time_module_.wait();
  net_module_.wait();
  for (auto &thread : workers_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
  DEBUG_LOG("CoroScheduler joined");
}

template <typename TimeModule,  typename LockModule,  typename NetModule,  typename DiskModule>
void CoroScheduler<TimeModule, LockModule, NetModule, DiskModule>::loop_() noexcept {
  CoroutineQueue ready_coroutine_queue;
  while (!stop_flag_.load(std::memory_order_acquire) || running_coro_cnt_ != 0) [[likely]] {
    {
      std::unique_lock lk(lock_);
      cv_.wait_for(lk,
        std::chrono::milliseconds(2000 + random_gen_.gen()/*0-1000*/),
        [this] noexcept { return !coroutine_queue_.empty() || stop_flag_.load(std::memory_order_acquire); });
      // while (!coroutine_queue_.empty()) [[likely]]
      if (!coroutine_queue_.empty()) [[likely]] {
        ready_coroutine_queue.append_to_tail(coroutine_queue_.pop_from_head());
      }
    }
    consume_ready_coroutine_(ready_coroutine_queue);
  }
}

template <typename TimeModule,  typename LockModule,  typename NetModule,  typename DiskModule>
void CoroScheduler<TimeModule, LockModule, NetModule, DiskModule>::consume_ready_coroutine_(CoroutineQueue &ready_queue) noexcept {
  LinkedCoroutine *fetched_ready_coro;
  while (!ready_queue.empty()) [[likely]] {
    DEBUG_LOG("schedule one");
    fetched_ready_coro = ready_queue.pop_from_head();
    fetched_ready_coro->sync_acquire();
    fetched_ready_coro->handle_.resume(); // do coroutine logic
  }
}

template struct CoroScheduler<UsedTimeModule, UsedLockModule, UsedNetModule, UsedDiskModule>;

}