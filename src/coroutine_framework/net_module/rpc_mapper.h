#pragma once
#include "rpc_register.h"
#include "error_define/error_struct.h"

namespace ToE {

class PackageHeader;

template<typename Func>
struct FunctionTraits;

template <typename R, typename... Args>
struct FunctionTraits<R(Args...)> {
  using return_type = R;
  using args_tuple = std::tuple<Args...>;
  using const_reference_args_tuple = std::tuple<const Args&...>;
  static constexpr size_t arg_count = sizeof...(Args);
  static const_reference_args_tuple get_const_ref_tuple(const Args &...args) {
    return std::make_tuple(args...);
  }
};

template <auto = nullptr>
struct FunctionToID;

template <int64_t ID>
struct IDToFunction;

#define RPC_REGISTER(ID, FUNC) \
  template <> \
  struct FunctionToID<&FUNC> { \
    static constexpr size_t value = ID; \
    using FunctionTraits = ToE::FunctionTraits<decltype(FUNC)>; \
  }; \
  template <> struct IDToFunction<ID> { \
    static constexpr auto func_ptr = &FUNC; \
    using FunctionTraits = ToE::FunctionTraits<decltype(FUNC)>; \
  };
  __RPC_REGISTER__
#undef RPC_REGISTER

Expected<void> reflect_commit_function(const PackageHeader &header,
                                       const EndPoint &endpoint,
                                       std::byte *serialized_data,
                                       const uint64_t len);

}