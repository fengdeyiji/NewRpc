#include "coroutine_framework/framework.hpp"
using namespace G;

CoroTask<int64_t> coro_fibo(int idx) {
  int64_t ret = 0;
  if (idx == 0) {
    ret = 1;
  } else if (idx == 1) {
    ret = 1;
  } else {
    // int64_t ret1 = co_await coro_fibo(idx - 1);
    // int64_t ret2 = co_await coro_fibo(idx - 2);
    // ret = ret1 + ret2;
    ret = co_await coro_fibo(idx - 1) + co_await coro_fibo(idx - 2);
  }
  co_return ret;
}

int fun1(int input, int &output) {
  output = input;
  return 0;
}

CoroTask<int> fun2(int &c) {
  int a = 1;
  int b = 0;
  fun1(a, b);
  INFO_LOG("print b:{}", b);
  co_return 0;
}

int main() {
  GlobalInit(LogLevel::info);
  // auto task = coro_fibo(20);
  int c = 0;
  auto task = fun2(c);
  task.promise_->handle_.resume();
  return 0;
}