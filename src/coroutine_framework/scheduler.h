#pragma once
#include "task.h"
#include <thread>
#include <vector>
#include <random>
#include "common_execute_module.h"
#include "time_module/time_service.h"
#include "net_module/net_service.h"

namespace ToE
{

struct RandomGenerator {
  RandomGenerator(int lower_bound, int upper_bound)
  : gen_{std::random_device{}()},
  dis_{lower_bound, upper_bound} {}
  int64_t gen() { return dis_(gen_); }
  std::mt19937 gen_;
  std::uniform_int_distribution<> dis_;
};

template <typename TimeModule, // for async sleep operatoin
          typename LockModule, // for async lock operation
          typename NetModule, // for async network operation
          typename DiskModule> // for async disk operation
struct CoroScheduler : public CommonExecuteModule {
  CoroScheduler(const uint32_t worker_thread_num = 1)
  : CoroScheduler{worker_thread_num, 8888} {}
  CoroScheduler(const uint32_t worker_thread_num, uint16_t port)
  : CommonExecuteModule{},
  time_module_{1_ms},
  net_module_{port},
  workers_{},
  worker_thread_num_{worker_thread_num},
  stop_flag_{true},
  running_coro_cnt_{0},
  random_gen_{0, 1000} { start(); }
  ~CoroScheduler();
  void start();
  void stop() noexcept;
  void wait() noexcept;
  template <typename Ret>
  Expected<void> commit(CoroTask<Ret> &&new_task) noexcept; // for first time schedule root frame
  template <typename Ret>
  Expected<void> commit(CoroTask<Ret> &new_task) noexcept; // for first time schedule root frame
  TimeModule &get_time_module() noexcept { return time_module_; }
  NetModule &get_net_module() noexcept { return net_module_; }
private:
  void loop_() noexcept;
  void consume_ready_coroutine_(CoroutineQueue &ready_queue) noexcept;
  TimeModule time_module_;
  NetModule net_module_;
  std::vector<std::jthread> workers_;
  uint32_t worker_thread_num_;
  std::atomic<bool> stop_flag_;
  std::atomic<uint64_t> running_coro_cnt_;
  RandomGenerator random_gen_;
};

template <typename TimeModule,  typename LockModule,  typename NetModule,  typename DiskModule>
template <typename Ret>
Expected<void> CoroScheduler<TimeModule, LockModule, NetModule, DiskModule>::commit(CoroTask<Ret> &&new_task) noexcept {
  if (stop_flag_.load(std::memory_order_acquire)) [[unlikely]] {
    return UnExpected{Error::HAS_BEEN_STOPPED};
  } else {
    new_task.promise_->ref_cnt_.inc();
    running_coro_cnt_++;
    new_task.promise_->frame_running_cnt_ = &running_coro_cnt_;
    CommonExecuteModule::commit(new_task.promise_);
  }
  return {};
}

template <typename TimeModule,  typename LockModule,  typename NetModule,  typename DiskModule>
template <typename Ret>
Expected<void> CoroScheduler<TimeModule, LockModule, NetModule, DiskModule>::commit(CoroTask<Ret> &new_task) noexcept {
  if (stop_flag_.load(std::memory_order_acquire)) [[unlikely]] {
    return UnExpected{Error::HAS_BEEN_STOPPED};
  } else {
    new_task.promise_->ref_cnt_.inc();
    running_coro_cnt_++;
    new_task.promise_->frame_running_cnt_ = &running_coro_cnt_;
    CommonExecuteModule::commit(new_task.promise_);
  }
  return {};
}

inline void GlobalInit(LogLevel level) {
  Logger::init(level);
  Error::init();
}

using UsedTimeModule = TimeService;
using UsedLockModule = int;
using UsedNetModule = NetService;
using UsedDiskModule = int;
extern template struct CoroScheduler<UsedTimeModule, UsedLockModule, UsedNetModule, UsedDiskModule>;
using CoroFrameWork = CoroScheduler<UsedTimeModule, UsedLockModule, UsedNetModule, UsedDiskModule>;

extern thread_local CoroFrameWork *TLS_FRAMEWORK;

}