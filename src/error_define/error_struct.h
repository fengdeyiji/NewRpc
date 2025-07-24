#pragma once
#include "stdlib.h"
#include <expected>
#include <format>
#include <unordered_map>
#include "mechanism/serialization.hpp"

namespace G {

struct Error {
  #define __DEF_ERROR__ \
    DEF_ERROR(RPC_TIMEOUT, -1001, "rpc response not returned at specified time span.") \
    DEF_ERROR(HAS_BEEN_STOPPED, -1002, "module has been stopped.") \
    DEF_ERROR(FUNCTION_NOT_REFLECTED, -1003, "deserialize meet not reflected function.") 
  #define DEF_ERROR(error_name, error_value, message) \
  static constexpr int32_t error_name = error_value;
  __DEF_ERROR__
  #undef DEF_ERROR
  static void init() {
    new (&ERR_MAP_) std::unordered_map<int32_t, const char *> {
      #define DEF_ERROR(error_name, error_value, message) \
      {error_value, message},
      __DEF_ERROR__
      #undef DEF_ERROR
    };
  }
  static const char *msg(int32_t error) {
    const char *ret = "unknown error";
    auto iter = ERR_MAP_.find(error);
    if (iter != ERR_MAP_.end()) [[likely]] {
      ret = iter->second;
    }
    return ret;
  }
  static std::unordered_map<int32_t, const char *> ERR_MAP_;
  int32_t error_no_;
};

template <typename T>
using Expected = std::expected<T, Error>;
using UnExpected = std::unexpected<Error>;


template <typename T>
struct Serializer<Expected<T>> {
  static void serialize(const Expected<T> &data, std::byte *buffer, const int64_t buffer_len, int64_t &pos);
  static void deserialize(Expected<T> &data, const std::byte *buffer, const int64_t buffer_len, int64_t &pos);
  static int64_t get_serialize_size(const Expected<T> &data);
};
template <>
struct Serializer<Error> {
  static void serialize(const Error &data, std::byte *buffer, const int64_t buffer_len, int64_t &pos) {
    return Serializer<int32_t>::serialize(data.error_no_, buffer, buffer_len, pos);
  }
  static void deserialize(Error &data, const std::byte *buffer, const int64_t buffer_len, int64_t &pos) {
    return Serializer<int32_t>::deserialize(data.error_no_, buffer, buffer_len, pos);
  }
  static int64_t get_serialize_size(const Error &data) {
    return Serializer<int32_t>::get_serialize_size(data.error_no_);
  }
};

template <typename T>
void Serializer<Expected<T>>::serialize(const Expected<T> &data, std::byte *buffer, const int64_t buffer_len, int64_t &pos) {
  if (data) [[likely]] {
    Serializer<bool>::serialize(true, buffer, buffer_len, pos);
    Serializer<T>::serialize(data.value(), buffer, buffer_len, pos);
  } else {
    Serializer<bool>::serialize(false, buffer, buffer_len, pos);
    Serializer<Error>::serialize(data.error(), buffer, buffer_len, pos);
  }
}

template <typename T>
void Serializer<Expected<T>>::deserialize(Expected<T> &data, const std::byte *buffer, const int64_t buffer_len, int64_t &pos) {
  bool valid = false;
  Serializer<bool>::deserialize(valid, buffer, buffer_len, pos);
  if (valid) [[likely]] {
    Serializer<T>::deserialize(data.value(), buffer, buffer_len, pos);
  } else {
    Serializer<Error>::deserialize(data.error(), buffer, buffer_len, pos);
  }
}

template <typename T>
int64_t Serializer<Expected<T>>::get_serialize_size(const Expected<T> &data) {
  int64_t total_size = 0;
  bool valid = false;
  total_size += Serializer<bool>::get_serialize_size(valid);
  if (valid) [[likely]] {
    total_size += Serializer<T>::get_serialize_size(data.value());
  } else {
    total_size += Serializer<Error>::get_serialize_size(data.error());
  }
  return total_size;
}
};
namespace std
{
template <> struct formatter<G ::Error> {
  constexpr auto parse(auto &ctx) { return ctx.begin(); }
  auto format(const G ::Error &val, auto &ctx) const {
    return std ::format_to(ctx.out(), "error:{}, message:{}", val.error_no_, G::Error::msg(val.error_no_));
  }
};
template <typename T> struct formatter<G ::Expected<T>> {
  constexpr auto parse(auto &ctx) { return ctx.begin(); }
  auto format(const G ::Expected<T> &val, auto &ctx) const {
    if (val) {
      return std ::format_to(ctx.out(), "{}", *val);
    } else {
      return std ::format_to(ctx.out(), "{}", val.error());
    }
  }
};
}