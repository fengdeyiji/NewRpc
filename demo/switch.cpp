#include "coroutine_framework/framework.hpp"
using namespace ToE;

static constexpr int64_t LOOP_SIZE = 1'000'000'00;

CoroTask<double> test_switch() {
  volatile double sum = 0.0; // 避免编译器优化
  for (int i = 0; i < LOOP_SIZE; ++i) {
    sum += std::sin(i) * std::cos(i); // 三角函数组合
    sum = std::exp(sum); // 指数运算
    co_await co_suspend();
  }
  co_return sum;
}

// libfs, hirpc
int main() {
  GlobalInit(LogLevel::info);
  CoroFrameWork framework{1};
  auto start = std::chrono::high_resolution_clock::now();
  auto future1 = test_switch();
  auto future2 = test_switch();
  Expected<void> ret1 = framework.commit(future1);
  Expected<void> ret2 = framework.commit(future2);
  assert(ret1 && ret2);
  future1.wait();
  future2.wait();
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  INFO_LOG("cost:{}ms", duration.count());
  return 0;
}