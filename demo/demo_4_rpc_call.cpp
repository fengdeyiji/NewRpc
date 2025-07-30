#include "coroutine_framework/framework.hpp"
using namespace G;

CoroTask<Expected<int64_t>> test_rpc(EndPoint peer) {
  auto ret = co_await co_rpc<example_add>().with_args(1, 2).on(peer);
  co_return ret;
} 

int main() {
  GlobalInit(LogLevel::info);
  INFO_LOG("input binding port:");
  uint16_t local_port, peer_port;
  std::cin >> local_port;
  CoroFrameWork framework{1, local_port};
  INFO_LOG("input execute rpc port:");
  std::cin >> peer_port;
  auto future = test_rpc(EndPoint{{127, 0, 0, 1}, peer_port});
  Expected<void> ret = framework.commit(future);
  assert(ret);
  future.wait();
  INFO_LOG("receive result:{}", future.get_result());
  return 0;
}