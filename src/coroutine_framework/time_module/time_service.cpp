#include "time_service.h"
#include "coroutine_framework/scheduler.h"
#include "log/logger.h"

namespace ToE
{

uint64_t WallClockTime::now() noexcept {
  auto now = std::chrono::system_clock::now();
  auto ns_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
  return ns_timestamp;
}

uint64_t SteadyClockTime::now() noexcept {
  auto now = std::chrono::steady_clock::now();
  auto ns_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
  return ns_timestamp;
}

void TimeService::start() {
  stop_flag_.store(false, std::memory_order_release);
  loop_thread_ = std::jthread([this,
                                  scheduler = TLS_SCHEDULER,
                                  framework = TLS_FRAMEWORK] {
    TLS_SCHEDULER = scheduler;
    TLS_FRAMEWORK = framework;
    this->loop_();
  });
  DEBUG_LOG("TimeService started");
}

void TimeService::stop() noexcept {
  stop_flag_.store(true, std::memory_order_release);
  cv_.notify_all();
  DEBUG_LOG("TimeService stopped");
}

void TimeService::wait() noexcept {
  if (loop_thread_.joinable()) [[likely]] {
    loop_thread_.join();
  }
  DEBUG_LOG("TimeService joinded");
}

void TimeService::register_frame(LinkedCoroutine *frame) noexcept {
  const uint64_t wake_up_ts = frame->coro_local_var_->wake_up_ts_;
  const uint64_t start_bukcet_run_ts = start_bukcet_run_ts_.load(std::memory_order_acquire);
  bool miss_awaken = false;
  if (start_bukcet_run_ts >= wake_up_ts) [[unlikely]] {
    miss_awaken = true;
  } else {
    const uint64_t calculated_bucket_idx = ((wake_up_ts - start_bukcet_run_ts) - 1) / precision_ + 1; // ceil
    const uint64_t real_bucket_idx = (calculated_bucket_idx & (BUCKET_NUM - 1));
    TimeWheelBucket &bucket = buckets_[real_bucket_idx];
    {
      std::lock_guard<ByteSpinLock> lg(bucket.lock_);
      if (bucket.scan_over_ts_ >= wake_up_ts) [[unlikely]] {
        miss_awaken = true;
      } else {
        bucket.waiting_queue_.append_to_tail(frame);
      }
    }
  }
  if (miss_awaken) [[unlikely]] {
    frame->sync_acquire();
    TLS_SCHEDULER->commit(frame);
  }
}

void TimeService::loop_() noexcept {
  uint64_t idx = 0;
  do {
    idx = ((idx + 1) & (BUCKET_NUM - 1)); // round-robin
    if (idx == 0) [[unlikely]] {
      start_bukcet_run_ts_.store(SteadyClockTime::now(), std::memory_order_release);
    }
    wakeup_frame_on_bucket_(idx);
    uint64_t now = SteadyClockTime::now();
    uint64_t next_run_ts = start_bukcet_run_ts_ + (precision_ * (idx + 1));
    if (now >= next_run_ts) {
      continue;
    } else {
      std::unique_lock<std::mutex> lock(lock_);
      cv_.wait_for(lock, std::chrono::nanoseconds(next_run_ts - now),
                   [this, next_run_ts] noexcept { return stop_flag_.load(std::memory_order_acquire) || SteadyClockTime::now() > next_run_ts; });
    }
  } while (!stop_flag_.load(std::memory_order_acquire));
  for (uint64_t idx = 0; idx < BUCKET_NUM; ++idx) {
    wakeup_frame_on_bucket_(idx, true); // force awake all frames on bucket
  }
}

void TimeService::wakeup_frame_on_bucket_(const uint64_t idx, const bool force_awake) noexcept {
  CoroutineQueue ready_queue;
  {
    auto &bucket = buckets_[idx];
    std::lock_guard<ByteSpinLock> lg(bucket.lock_);
    bucket.scan_over_ts_ = SteadyClockTime::now();
    bucket.waiting_queue_.fetch_nodes(
      [force_awake, &bucket](LinkedCoroutine *coro_frame) noexcept {
        coro_frame->sync_acquire();
        return coro_frame->coro_local_var_->wake_up_ts_ <= bucket.scan_over_ts_ || force_awake;
      },
    &ready_queue);
  }
  while (!ready_queue.empty()) [[likely]] {
    DEBUG_LOG("wakeup one");
    LinkedCoroutine *coro_frame = ready_queue.pop_from_head();
    TLS_SCHEDULER->commit(coro_frame);
  }
}

}