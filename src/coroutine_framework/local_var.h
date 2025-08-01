#pragma once
#include <cstdint>
#include <atomic>
#include "coroutine_framework/net_module/net_define.h"
#include "utils.h"

namespace ToE
{

struct CommonExecuteModule;

struct ResponseInfo {
  ResponseInfo(const EndPoint &endpoint, uint64_t coro_id, uint16_t rpc_type)
  : response_to_end_point_{endpoint},
  response_to_coro_id_{coro_id},
  response_rpc_type_{rpc_type} {}
  EndPoint response_to_end_point_;
  uint64_t response_to_coro_id_;
  uint16_t response_rpc_type_;
};

struct CoroLocalVar {
  CoroLocalVar(RefCount &ref_cnt)
  : coro_id_{ID.fetch_add(1)},
  wake_up_ts_{0},
  response_info_{nullptr} {}
  ~CoroLocalVar();
  uint64_t coro_id_; // in-process uniq monotonic id
  uint64_t wake_up_ts_; // for timer module
  ResponseInfo *response_info_;
private:
  static std::atomic<uint64_t> ID;
};

}