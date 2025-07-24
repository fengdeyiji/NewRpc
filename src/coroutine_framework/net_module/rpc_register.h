#include <cstdint>
#include <cstddef>
#include <tuple>
#include "coroutine_framework/task.h"

namespace G {

// declare Function
CoroTask<int64_t> example_add(int64_t a, int64_t b);
CoroTask<int64_t> example_echo();

#define __RPC_REGISTER__ \
  RPC_REGISTER(1, example_add) \
  RPC_REGISTER(2, example_echo)
}