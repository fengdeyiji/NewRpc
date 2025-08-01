#include <cstdint>
#include <cstddef>
#include <tuple>
#include "coroutine_framework/task.h"
#include "demo/point.h"

namespace ToE {

// declare Function
CoroTask<int64_t> example_add(int64_t a, int64_t b);
CoroTask<int64_t> example_echo();
CoroTask<Point> example_add_point(Point a, Point b);

#define __RPC_REGISTER__ \
  RPC_REGISTER(1, example_add) \
  RPC_REGISTER(2, example_echo) \
  RPC_REGISTER(3, example_add_point)
}