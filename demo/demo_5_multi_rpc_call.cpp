#include "coroutine_framework/framework.hpp"
using namespace G;

CoroTask<void> multi_rpc_call(std::vector<EndPoint> endpoints) {
  while (true) {
    uint64_t before_call_ts = SteadyClockTime::now();
    auto results = co_await co_rpc<example_echo>().with_args() // 函数调用参数
                                                  .on(endpoints) // 广播rpc的目的端列表
                                                  .when_all() // 等待所有结果收集完毕
                                                  .timeout(1_s) // 若超过1s未收集到所有结果，直接恢复协程
                                                  .on_each_result( // 每次都到rpc响应时触发回调
    [&before_call_ts](const EndPoint &peer_addr, int64_t &result, bool &need_resume) {
      need_resume = false; // 可以根据结果提前唤醒协程，但这里无需提前唤醒
      result = SteadyClockTime::now() - before_call_ts; // 记录针对每台server的rpc调用延迟
    });
    INFO_LOG("=======================");
    for (auto &result : results) {
      if (result.second) {
        INFO_LOG("print result from:[{}], delay:{}", result.first, TS(*result.second));
      } else {
        INFO_LOG("error from:[{}], err:{}", result.first, result.second);
      }
    }
    co_await co_sleep(before_call_ts + 3_s - SteadyClockTime::now()); // 每3s进行一次echo探测
  }
  co_return;
}

int main() {
  GlobalInit(LogLevel::info);
  INFO_LOG("input binding port:");
  uint16_t local_port;
  std::cin >> local_port;
  CoroFrameWork framework{1, local_port};
  INFO_LOG("input execute rpc ports(multi):");
  std::vector<EndPoint> endpoints;
  uint16_t peer_port;
  while (std::cin >> peer_port) {
    endpoints.push_back(EndPoint{{127,0,0,1}, peer_port});
  }
  auto future = multi_rpc_call(endpoints);
  Expected<void> ret = framework.commit(future);
  assert(ret);
  future.wait();
  return 0;
}