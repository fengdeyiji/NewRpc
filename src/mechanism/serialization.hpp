#pragma once
#include <limits>
#include <cstring>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <stdexcept>
#include <ranges>
#include <vector>
#include <tuple>
#include <cstddef> // for offsetof
#include "boost/preprocessor.hpp" // 用于宏迭代
#include "static_reflection.hpp"

namespace G {

template<typename Test, template<typename...> class Ref>
struct is_specialization_of : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct is_specialization_of<Ref<Args...>, Ref>: std::true_type {};

template< class T, template<class...> class Primary >
inline constexpr bool is_specialization_of_v = is_specialization_of<T, Primary>::value;


static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<long double>::is_iec559,
              "mechanism/deserialization only support platform with IEEE-754 standard");

template <typename T>
concept Serializable = requires {
  requires std::same_as<decltype(&T::serialize), void (T::*)(std::byte*, const int64_t, int64_t&) const>;
  requires std::same_as<decltype(&T::deserialize), void (T::*)(const std::byte*, const int64_t, int64_t&)>;
  requires std::same_as<decltype(&T::get_serialize_size), int64_t (T::*)() const>;
};

#define DECAY_T(value) std::decay_t<decltype(value)>

template <typename T>
struct Serializer {

template <typename Container>
static void deserialize_init_container(Container &container, const int64_t size) {
  if constexpr (is_specialization_of_v<Container, std::vector>) {
    container.reserve(size);
  } else {
    static_assert(false, "Unsupported type.");
  }
}

template <typename Container, typename Element>
static void deserialize_insert_element_to_container(Container &container, Element &&element) {
  if constexpr (is_specialization_of_v<Container, std::vector>) {
    container.push_back(std::forward<Element>(element));
  } else {
    static_assert(false, "Unsupported type.");
  }
}

static void serialize(const T &data, std::byte *buffer, const int64_t buffer_len, int64_t &pos) {
  #define ARGS buffer, buffer_len, pos
  if constexpr (std::is_pointer_v<T>) { // 指针类型
    bool is_null_ptr = (data == nullptr);
    Serializer<bool>::serialize(is_null_ptr, ARGS);
    if (!is_null_ptr) [[likely]] {
      Serializer<DECAY_T(*data)>::serialize(*data, ARGS);
    }
  } else if constexpr (std::is_enum_v<T>) { // 枚举类型
    Serializer<int64_t>::serialize(static_cast<int64_t>(data), ARGS);
  } else if constexpr (std::is_fundamental_v<T>) { // 基本类型：按照小端序进行序列化
    if constexpr (std::endian::native == std::endian::big) {
      T data_copy = std::byteswap(data);
      if (pos + sizeof(data_copy) > buffer_len) [[unlikely]] {
        throw std::out_of_range("serization out of buffer range, which should not happen");
      }
      std::memcpy(buffer + pos, &data_copy, sizeof(data_copy));
      pos += sizeof(data_copy);
    } else {
      if (pos + sizeof(data) > buffer_len) [[unlikely]] {
        throw std::out_of_range("serization out of buffer range, which should not happen");
      }
      std::memcpy(buffer + pos, &data, sizeof(data));
      pos += sizeof(data);
    }
  } else if constexpr (Serializable<T>) { // 定制序列化类型，调用定制方法
    data.serialize(ARGS);
  } else if constexpr (std::is_same_v<T, std::string>) { // 特殊处理std::string类型
    int64_t str_len = static_cast<int64_t>(data.size());
    Serializer<int64_t>::serialize(str_len, ARGS);
    if (pos + str_len > buffer_len) [[unlikely]] {
      throw std::out_of_range("serization out of buffer range, which should not happen");
    }
    std::memcpy(buffer + pos, data.data(), str_len);
    pos += str_len;
  } else if constexpr (std::ranges::range<T>) { // 可迭代的容器类，例如: std::vector, std::list, std::map等
    int64_t size = std::ranges::distance(data.begin(), data.end());
    Serializer<int64_t>::serialize(size, ARGS);
    for (const auto& element : data) {
      Serializer<DECAY_T(element)>::serialize(element, ARGS);
    }
  } else if constexpr (Reflectable<T>) { // 其他类型: 要求该类型可被静态反射
    reflect_for_each(data, [buffer, buffer_len, &pos](const char *name, const auto &value) {
      Serializer<DECAY_T(value)>::serialize(value, ARGS);
    });
  } else {
    static_assert(false, "Unsupported type for serialization.");
  }
  #undef ARGS
}

static void deserialize(T &data, const std::byte *buffer, const int64_t buffer_len, int64_t &pos) {
  #define ARGS buffer, buffer_len, pos
  if constexpr (std::is_pointer_v<T>) { // 指针类型
    bool is_null_ptr;
    Serializer<bool>::deserialize(is_null_ptr, ARGS);
    if (is_null_ptr) {
      data = nullptr;
    } else {
      using ElementType = std::remove_pointer_t<T>;
      data = new ElementType();
      Serializer<DECAY_T(*data)>::deserialize(*data, ARGS);
    }
  } else if constexpr (std::is_enum_v<T>) { // 枚举类型
    int64_t enum_value = 0;
    Serializer<int64_t>::deserialize(enum_value, ARGS);
    data = static_cast<T>(enum_value);
  } else if constexpr (std::is_fundamental_v<T>) { // 基本类型：按照小端序进行反序列化
    if constexpr (std::endian::native == std::endian::big) {
      T data_copy;
      if (pos + sizeof(data_copy) > buffer_len) [[unlikely]] {
        throw std::out_of_range("deserization out of buffer range, which should not happen");
      }
      std::memcpy(&data_copy, buffer + pos, sizeof(data_copy));
      pos += sizeof(data_copy);
      data = std::byteswap(data_copy);
    } else {
      if (pos + sizeof(data) > buffer_len) [[unlikely]] {
        throw std::out_of_range("deserization out of buffer range, which should not happen");
      }
      std::memcpy(&data, buffer + pos, sizeof(data));
      pos += sizeof(data);
    }
  } else if constexpr (Serializable<T>) { // 定制反序列化类型，调用定制方法
    data.deserialize(ARGS);
  } else if constexpr (std::is_same_v<T, std::string>) { // 特殊处理std::string类型
    int64_t str_len;
    Serializer<int64_t>::deserialize(str_len, ARGS);
    data.resize(str_len);
    if (pos + str_len > buffer_len) [[unlikely]] {
      throw std::out_of_range("deserization out of buffer range, which should not happen");
    }
    std::memcpy(data.data(), buffer + pos, str_len);
    pos += str_len;
  } else if constexpr (std::ranges::range<T>) { // 可迭代的容器类，例如: std::vector, std::list, std::map等
    int64_t size;
    Serializer<int64_t>::deserialize(size, ARGS);
    deserialize_init_container<T>(data, size);
    using ElementType = T::value_type;
    for (int64_t idx = 0; idx < size; ++idx) {
      ElementType element;
      Serializer<ElementType>::deserialize(element, ARGS);
      deserialize_insert_element_to_container(data, std::move(element));
    }
  } else if constexpr (Reflectable<T>) { // 其他类型: 要求该类型可被boost::hana静态反射
    reflect_for_each(data, [buffer, buffer_len, &pos](const char *name, auto &value) {
      Serializer<DECAY_T(value)>::deserialize(value, ARGS);
    });
  } else {
    static_assert(false, "Unsupported type for deserialization.");
  }
  #undef ARGS
}

static int64_t get_serialize_size(const T &data) {
  int64_t total_size = 0;
  if constexpr (std::is_pointer_v<T>) { // 指针类型
    total_size += sizeof(bool);
    if (nullptr != data) [[likely]] {
      total_size += Serializer<DECAY_T(*data)>::get_serialize_size(*data);
    }
  } else if constexpr (std::is_enum_v<T>) { // 枚举类型
    total_size += Serializer<int64_t>::get_serialize_size(static_cast<int64_t>(data));
  } else if constexpr (std::is_fundamental_v<T>) { // 基本类型
    total_size += sizeof(data);
  } else if constexpr (Serializable<T>) { // 定制序列化类型，调用定制方法
    total_size += data.get_serialize_size();
  } else if constexpr (std::is_same_v<T, std::string>) { // 特殊处理std::string类型
    int64_t str_len;
    total_size += Serializer<int64_t>::get_serialize_size(str_len);
    total_size += data.size();
  } else if constexpr (std::ranges::range<T>) { // 可迭代的容器类，例如: std::vector, std::list, std::map等
    int64_t size;
    total_size += Serializer<int64_t>::get_serialize_size(size);
    for (const auto& element : data) {
      total_size += Serializer<DECAY_T(element)>::get_serialize_size(element);
    }
  } else if constexpr (Reflectable<T>) { // 其他类型: 要求该类型可被boost::hana静态反射
    reflect_for_each(data, [&total_size](const char *name, const auto &value) {
      total_size += Serializer<DECAY_T(value)>::get_serialize_size(value);
    });
  } else {
    static_assert(false, "Unsupported type for get_serialize_size().");
  }
  return total_size;
}

static void serialize(const T &data, std::byte *buffer, const int64_t buffer_len) {
  int64_t pos = 0;
  serialize(data, buffer, buffer_len, pos);
}
static void deserialize(T &data, std::byte *buffer, const int64_t buffer_len) {
  int64_t pos = 0;
  deserialize(data, buffer, buffer_len, pos);
}

};
}