#include "coroutine_framework/framework.hpp"
using namespace G;
using namespace std;

CoroTask<void> test_sleep() {
  co_await co_sleep(30_s);
  co_return;
}

int main() {
  GlobalInit(LogLevel::info);
  INFO_LOG("start");
  int64_t current_size = 1'000'000;
  CoroFrameWork framework;
  std::vector<CoroTask<void>> v_futures;
  v_futures.reserve(current_size);
  for (int64_t idx = 0; idx < current_size; ++idx) {
    v_futures.push_back(test_sleep());
    auto ret = framework.commit(v_futures.back());
    assert(ret);
  }
  for (auto &future : v_futures) {
    future.wait();
  }
  INFO_LOG("end");
  return 0;
}