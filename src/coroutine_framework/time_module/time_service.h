#pragma once
#include <array>
#include <cstdlib>
#include <thread>
#include <condition_variable>
#include "coroutine_framework/queue.h"

namespace G
{

struct WallClockTime {
  static uint64_t now() noexcept;
};

struct SteadyClockTime {
  static uint64_t now() noexcept;
};

struct TimeService {
  static constexpr int64_t BUCKET_NUM = 1LL << 12;
  struct TimeWheelBucket {
    TimeWheelBucket() : lock_{}, scan_over_ts_(0), waiting_queue_{} {}
    ByteSpinLock lock_;
    uint64_t scan_over_ts_;
    CoroutineQueue waiting_queue_;
  } cache_aligned;
  TimeService(const uint64_t precision)
  : loop_thread_{},
  stop_flag_{false},
  lock_{},
  precision_{precision},
  start_bukcet_run_ts_{SteadyClockTime::now()},
  buckets_{} {}
  TimeService(const TimeService &) = delete;
  TimeService(TimeService &&) = delete;
  TimeService &operator=(const TimeService &) = delete;
  TimeService &operator=(TimeService &&) = delete;
  void start();
  void stop() noexcept;
  void wait() noexcept;
  void register_frame(LinkedCoroutine *frame) noexcept;
private:
  void loop_() noexcept;
  void wakeup_frame_on_bucket_(const uint64_t idx, const bool force_awake = false) noexcept;
  std::jthread loop_thread_;
  std::atomic<bool> stop_flag_;
  std::mutex lock_;
  std::condition_variable cv_;
  const uint64_t precision_;
  std::atomic<uint64_t> start_bukcet_run_ts_;
  std::array<TimeWheelBucket, BUCKET_NUM> buckets_;
};

}