#include "coroutine_framework/framework.hpp"
using namespace ToE;
using namespace std;

void busy_loop(uint64_t ns) {
  uint64_t end = SteadyClockTime::now() + ns;
  while (SteadyClockTime::now() < end);
}

CoroTask<void> test_sleep(int idx) {
  INFO_LOG("[{}]start loop", idx);
  busy_loop(500_ms);
  INFO_LOG("[{}]start sleep", idx);
  co_await co_sleep(1_s);
  INFO_LOG("[{}]end call", idx);
  co_return;
}

int main() {
  GlobalInit(LogLevel::info);
  CoroFrameWork framework;
  auto future1 = test_sleep(1);
  auto future2 = test_sleep(2);
  auto ret1 = framework.commit(future1);
  auto ret2 = framework.commit(future2);
  future1.wait();
  future2.wait();
  return 0;
}