#include "coroutine_framework/net_module/net_define.h"
#include "coroutine_framework/local_var.h"
#include "rpc_mapper.h"
#include "coroutine_framework/net_module/rpc_struct.h"
#include "coroutine_framework/scheduler.h"
#include "coroutine_framework/utils.h"
#include "error_define/error_struct.h"
#include "mechanism/serialization.hpp"
#include "coroutine_framework/framework.hpp"
#include <utility>

namespace ToE
{

Expected<void> reflect_commit_function(const PackageHeader &header,
                                       const EndPoint &endpoint,
                                       std::byte *serialized_data,
                                       const uint64_t len) {
  Expected<void> ret = {};
  switch (header.rpc_type_) {
    #define RPC_REGISTER(ID, FUNC) \
    case FunctionToID<FUNC>::value: \
      { \
        EndPoint server_endpoint = endpoint; \
        server_endpoint.set_port(header.server_port_); \
        using ArgsTupleType = FunctionToID<FUNC>::FunctionTraits::args_tuple; \
        ArgsTupleType args_tuple; \
        int64_t pos = 0; \
        for_each_tuple([serialized_data, len, &pos](auto &arg) { \
          Serializer<DECAY_T(arg)>::deserialize(arg, serialized_data, len, pos); \
        }, args_tuple); \
        auto task = std::apply([](auto&&... args) -> auto { \
          return FUNC(std::move(args)...); \
        }, args_tuple); \
        task.promise_->coro_local_var_ = new CoroLocalVar{task.promise_->ref_cnt_}; \
        task.promise_->coro_local_var_->response_info_ = new ResponseInfo{server_endpoint, header.rpc_id_, header.rpc_type_}; \
        ret = TLS_FRAMEWORK->commit(std::move(task)); \
      } \
      break;
    __RPC_REGISTER__
    default:
      ret = UnExpected{Error::FUNCTION_NOT_REFLECTED};
    #undef RPC_REGISTER
  }
  return ret;
}

}