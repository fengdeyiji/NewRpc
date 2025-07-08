#pragma once
#include "common.h"
#include <limits>
#include <cstring>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <stdexcept>
#include <ranges>
#include <boost/hana.hpp>

namespace new_rpc {

static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<long double>::is_iec559,
              "serialization/deserialization only support platform with IEEE-754 standard");

template <typename T>
concept Serializable = requires {
  requires std::same_as<decltype(&T::serialize), void (T::*)(std::byte*, const int64_t, int64_t&) const>;
  requires std::same_as<decltype(&T::deserialize), void (T::*)(const std::byte*, const int64_t, int64_t&)>;
  requires std::same_as<decltype(&T::get_serialize_size), int64_t (T::*)() const>;
};

template <typename Container>
void deserialize_init_container(Container &container, const int64_t size) {
  if constexpr (is_specialization_of_v<Container, std::vector>) {
    container.reserve(size);
  } else {
    static_assert(false, "Unsupported type.");
  }
}

template <typename Container, typename Element>
void deserialize_insert_element_to_container(Container &container, Element &&element) {
  if constexpr (is_specialization_of_v<Container, std::vector>) {
    container.push_back(std::forward<Element>(element));
  } else {
    static_assert(false, "Unsupported type.");
  }
}

template <typename T>
void serialize(const T &data, std::byte *buffer, const int64_t buffer_len, int64_t &pos) {
  #define ARGS buffer + pos, buffer_len, pos
  if constexpr (std::is_pointer_v<T>) { // 指针类型
    bool is_null_ptr = (data == nullptr);
    serialize(is_null_ptr, ARGS);
    if (!is_null_ptr) [[likely]] {
      serialize(*data, ARGS);
    }
  } else if constexpr (std::is_enum_v<T>) { // 枚举类型
    serialize(static_cast<int64_t>(data), ARGS);
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
    serialize(str_len, ARGS);
    if (pos + str_len > buffer_len) [[unlikely]] {
      throw std::out_of_range("serization out of buffer range, which should not happen");
    }
    std::memcpy(buffer + pos, data.data(), str_len);
    pos += str_len;
  } else if constexpr (std::ranges::range<T>) { // 可迭代的容器类，例如: std::vector, std::list, std::map等
    int64_t size = std::ranges::distance(data.begin(), data.end());
    serialize(size, ARGS);
    for (const auto& element : data) {
      serialize(element, ARGS);
    }
  } else if constexpr (hana_reflectable<T>) { // 其他类型: 要求该类型可被boost::hana静态反射
    auto members = boost::hana::members(data);
    boost::hana::for_each(members, [buffer, buffer_len, &pos] (const auto& member) {
      const auto& value = boost::hana::second(member);
      serialize(value, ARGS);
    });
  } else {
    static_assert(false, "Unsupported type for serialization.");
  }
  #undef ARGS
}

template <typename T>
void deserialize(T &data, std::byte *buffer, const int64_t buffer_len, int64_t &pos) {
  #define ARGS buffer + pos, buffer_len, pos
  if constexpr (std::is_pointer_v<T>) { // 指针类型
    bool is_null_ptr;
    deserialize(is_null_ptr, ARGS);
    if (is_null_ptr) {
      data = nullptr;
    } else {
      using ElementType = std::remove_pointer_t<T>;
      data = new ElementType();
      deserialize(*data, ARGS);
    }
  } else if constexpr (std::is_enum_v<T>) { // 枚举类型
    int64_t enum_value = 0;
    deserialize(enum_value, ARGS);
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
    deserialize(str_len, ARGS);
    data.resize(str_len);
    if (pos + str_len > buffer_len) [[unlikely]] {
      throw std::out_of_range("deserization out of buffer range, which should not happen");
    }
    std::memcpy(data.data(), buffer + pos, str_len);
    pos += str_len;
  } else if constexpr (std::ranges::range<T>) { // 可迭代的容器类，例如: std::vector, std::list, std::map等
    int64_t size;
    deserialize(size, ARGS);
    deserialize_init_container<T>(data, size);
    using ElementType = T::value_type;
    for (int64_t idx = 0; idx < size; ++idx) {
      ElementType element;
      deserialize(element, ARGS);
      deserialize_insert_element_to_container(data, std::move(element));
    }
  } else if constexpr (hana_reflectable<T>) { // 其他类型: 要求该类型可被boost::hana静态反射
    auto members = boost::hana::members(data);
    boost::hana::for_each(members, [buffer, buffer_len, &pos] (auto& member) {
      auto& value = boost::hana::second(member);
      deserialize(value, ARGS);
    });
  } else {
    static_assert(false, "Unsupported type for deserialization.");
  }
  #undef ARGS
}

template <typename T>
int64_t get_serialize_size(const T &data) noexcept {
  int64_t total_size = 0;
  if constexpr (std::is_pointer_v<T>) { // 指针类型
    total_size += sizeof(bool);
    if (nullptr != data) [[likely]] {
      total_size += get_serialize_size(*data);
    }
  } else if constexpr (std::is_enum_v<T>) { // 枚举类型
    total_size += get_serialize_size(static_cast<int64_t>(data));
  } else if constexpr (std::is_fundamental_v<T>) { // 基本类型
    total_size += sizeof(data);
  } else if constexpr (Serializable<T>) { // 定制序列化类型，调用定制方法
    total_size += data.get_serialize_size();
  } else if constexpr (std::is_same_v<T, std::string>) { // 特殊处理std::string类型
    int64_t str_len;
    total_size += get_serialize_size(str_len);
    total_size += data.size();
  } else if constexpr (std::ranges::range<T>) { // 可迭代的容器类，例如: std::vector, std::list, std::map等
    int64_t size;
    total_size += get_serialize_size(size);
    for (const auto& element : data) {
      total_size += get_serialize_size(element);
    }
  } else if constexpr (hana_reflectable<T>) { // 其他类型: 要求该类型可被boost::hana静态反射
    auto members = boost::hana::members(data);
    boost::hana::for_each(members, [&total_size] (const auto& member) {
      const auto& value = boost::hana::second(member);
      total_size += get_serialize_size(value);
    });
  } else {
    static_assert(false, "Unsupported type for get_serialize_size().");
  }
  return total_size;
}

}