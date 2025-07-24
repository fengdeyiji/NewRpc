#include "coroutine_framework/framework.hpp"

namespace G
{

CoroTask<int64_t> example_add(int64_t a, int64_t b) {
  int64_t ret = a + b;
  INFO_LOG("execute example_add() with a:{}, b:{}", a, b);
  co_await co_sleep{1_s};
  INFO_LOG("return:{}", ret);
  co_return ret;
}

CoroTask<int64_t> example_echo() {
  RandomGenerator random{0, 1000};
  auto rand = random.gen();
  INFO_LOG("call echo, random sleep:{}ms", rand);
  co_await co_sleep(rand * 1_ms);
  co_return 0;
}

}