#include "coroutine_framework/framework.hpp"
using namespace ToE;

CoroTask<Point> test_add_point(EndPoint peer) {
  auto ret = co_await co_rpc<example_add_point>().with_args(Point{1,1}, Point{2,2})
                                                 .on(peer)
                                                 .timeout(2_s);
  co_return *ret;
}

CoroTask<int64_t> coro_fibo(int idx) {
  int64_t ret = 0;
  if (idx == 0) {
    ret = 1;
  } else if (idx == 1) {
    ret = 1;
  } else {
    auto ret1 = co_await coro_fibo(idx - 1);
    auto ret2 = co_await coro_fibo(idx - 2);
    ret = ret1 + ret2;
    // ret = co_await coro_fibo(idx - 1) + co_await coro_fibo(idx - 2);
  }
  co_return ret;
}

int main() {
  GlobalInit(LogLevel::info);
  auto task = coro_fibo(20);
  task.promise_->handle_.resume();
  INFO_LOG("co_fib:{}", task.get_result());
  return 0;
}