#pragma once
#include "common.h"
#include <cstring>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <concepts>
#include <ranges>
#include <format>
#include "static_reflection.hpp"

namespace G {

template <typename T>
concept ToStringAble = requires {
  requires std::same_as<decltype(&T::to_string), void (T::*)(char*, const int64_t, int64_t&) const>;
};

template <typename T>
concept is_pointer_like = requires(T ptr) {
    { *ptr } -> std::same_as<std::remove_reference_t<decltype(*ptr)>&>; // 解引用操作
    { ptr.operator->() } -> std::convertible_to<std::add_pointer_t<decltype(*ptr)>>; // 箭头操作
} || std::is_pointer_v<T>; // 兼容原始指针

template <typename T>
concept string_view_constrctable = requires(T obj) {
  std::string_view(obj);
};

template <typename PointerLike>
auto get_obj_addr(PointerLike &pointer) {
  if constexpr (std::is_pointer_v<PointerLike>) { // 原始指针直接返回指针的值，无需解引用(因为void *类型不支持解引用)
    return (int64_t)pointer;
  } else if constexpr (is_pointer_like<PointerLike>) {
    return (int64_t)&(*pointer);
  } else {
    static_assert(std::false_type::value, "not support pointer type");
  }
}

template <typename Value>
void kv_to_string(const char *key, const Value &value, char *buffer, const int64_t buffer_len, int64_t &pos);

template <typename Value>
void value_to_string(const Value &value, char *buffer, const int64_t buffer_len, int64_t &pos, bool with_key) {
  #define FORMAT_TO(fmt, ...) \
    if (pos < buffer_len) [[likely]] { \
      auto result = std::format_to_n(buffer + pos, buffer_len - pos, fmt __VA_OPT__(,) __VA_ARGS__); \
      if (pos + result.size <= buffer_len) [[likely]] { \
        pos += result.size; \
      } else { \
        pos = buffer_len; \
      } \
    }
  if constexpr (std::same_as<Value, char *> ||
                std::same_as<Value, const char *> ||
                std::same_as<Value, std::string> ||
                std::same_as<Value, std::string_view>) { // 字符串类型直接格式化处理
    FORMAT_TO("{}", value);
  } else if constexpr (is_pointer_like<Value>) { // 泛指针类型
    if (nullptr == value) [[unlikely]] {
      FORMAT_TO("NULL");
    } else {
      int64_t pointer_addr = get_obj_addr(value);
      if constexpr (requires { (*value); }) { // 支持解引用的泛指针类型
        FORMAT_TO("({:#x}):", pointer_addr);
        value_to_string(*value, buffer, buffer_len, pos, with_key);
      } else { // 不支持解引用，仅打印指针地址，如void *
        FORMAT_TO("{:#x}", pointer_addr);
      }
    }
  } else if constexpr (std::is_enum_v<Value>) { // 枚举类型转换为整型处理
    value_to_string(static_cast<int64_t>(value), buffer, buffer_len, pos, with_key);
  } else if constexpr (std::is_same_v<bool, Value>) {
    FORMAT_TO("{}", value);
  } else if constexpr (std::is_integral_v<Value> && !std::same_as<Value, char>) { // 整形
    FORMAT_TO("{:d}", value);
  } else if constexpr (std::same_as<Value, char>) { // 字符形
    FORMAT_TO("'{:c}'", value);
  } else if constexpr (std::is_floating_point_v<Value>) { // 浮点数类型: 保留两位小数
    FORMAT_TO("{:.2f}", value);
  } else if constexpr (ToStringAble<Value>) { // 定制字符串化类型，调用定制方法
    if (with_key) {
      FORMAT_TO("{{");
      value.to_string(buffer, buffer_len, pos);
      FORMAT_TO("}}");
    } else {
      value.to_string(buffer, buffer_len, pos);
    }
  } else if constexpr (std::formattable<Value, char>) { // 可format类型
    if (with_key) {
      FORMAT_TO("{{");
      FORMAT_TO("{}", value);
      FORMAT_TO("}}");
    } else {
      FORMAT_TO("{}", value);
    }
  } else if constexpr (std::ranges::range<Value>) { // 可迭代的容器类，例如: std::vector, std::list, std::map等
    constexpr int64_t MAX_PRINT_SIZE = 8; // 打印前8项
    int64_t size = std::ranges::distance(std::begin(value), std::end(value));
    if (with_key) {
      FORMAT_TO("{{");
    }
    FORMAT_TO("[");
    bool need_back = false;
    int64_t idx = 0;
    int64_t extra_size = 0;
    auto iter = std::begin(value);
    while (iter != std::end(value) && idx < MAX_PRINT_SIZE) {
      value_to_string(*iter, buffer, buffer_len, pos, true);
      if (idx < MAX_PRINT_SIZE - 1 && std::next(iter) != std::end(value)) {
        FORMAT_TO(", ");
      }
      ++idx;
      ++iter;
    }
    if (size > MAX_PRINT_SIZE) {
      extra_size = size - MAX_PRINT_SIZE;
      FORMAT_TO("..({:d} more)", extra_size);
    }
    FORMAT_TO("]");
    if (with_key) {
      FORMAT_TO("}}");
    }
  } else if constexpr (Reflectable<Value>) { // 其他类型: 要求该类型可被boost::hana静态反射
    if (with_key) {
      FORMAT_TO("{{");
    }
    bool need_back = false;
    reflect_for_each(value, [buffer, buffer_len, &pos, &need_back](const char *name, const auto &value) {
      kv_to_string(name, value, buffer, buffer_len, pos);
      FORMAT_TO(", ");
      need_back = true;
    });
    if (need_back) [[likely]] {
      pos -= 2;
    }
    if (with_key) {
      FORMAT_TO("}}");
    }
  } else {
    static_assert(false, "Unsupported type for stringification.");
  }
  #undef FORMAT_TO
}

template <typename Value>
void kv_to_string(const char *key, const Value &value, char *buffer, const int64_t buffer_len, int64_t &pos) {
  #define FORMAT_TO(fmt...) pos += std::format_to_n(buffer + pos, buffer_len, fmt).size
  FORMAT_TO("{}:", key);
  value_to_string(value, buffer, buffer_len, pos, true);
  #undef FORMAT_TO
}

}