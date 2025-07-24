#pragma once
#include <tuple>
#include <atomic>

namespace G
{

template <int IDX, typename FUNC, typename TUPLE>
constexpr void for_each_tuple_helper(FUNC &&func, TUPLE &&tuple) {
  if constexpr (IDX != std::tuple_size<std::decay_t<TUPLE>>::value) {
    func(std::get<IDX>(tuple));
    for_each_tuple_helper<IDX + 1>(std::forward<FUNC>(func), std::forward<TUPLE>(tuple));
    return;
  }
}
template <typename FUNC, typename TUPLE>
constexpr void for_each_tuple(FUNC &&func, TUPLE &&tuple) {
  return for_each_tuple_helper<0>(std::forward<FUNC>(func), std::forward<TUPLE>(tuple));
}

struct RefCount {
  RefCount() : shared_cnt_{1} {}
  void inc() { shared_cnt_.fetch_add(1, std::memory_order_relaxed); }
  uint64_t dec() { return shared_cnt_.fetch_sub(1, std::memory_order_acq_rel); }
  std::atomic<uint64_t> shared_cnt_;
};

struct ByteSpinLock {
  ByteSpinLock() : lock_{} {}
  void lock() noexcept { while (lock_.test_and_set(std::memory_order_acquire)); }
  bool try_lock() noexcept { return !lock_.test_and_set(std::memory_order_acquire); }
  void unlock() noexcept { lock_.clear(std::memory_order_release); }
private:
  std::atomic_flag lock_;
};

}