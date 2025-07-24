#pragma once
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <format>
#include "boost/preprocessor.hpp"

namespace G
{
template <typename Value>
void value_to_string(const Value &value, char *buffer, const int64_t buffer_len, int64_t &pos, bool with_key = false);
}

template <typename> 
struct REFLECT;

template <typename Head, typename ...Args>
struct __DropFirst__ {
  using type = std::tuple<Args...>;
};
#define STATIC_REFLECT(TYPE, ...) \
template <> \
struct REFLECT<TYPE> { \
  static constexpr size_t MemberCount = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
  static constexpr const char* MemberName[] = { \
    BOOST_PP_SEQ_FOR_EACH_I(__MEMBER_NAME__, TYPE, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
  }; \
  static constexpr size_t MemberOffset[] = { \
    BOOST_PP_SEQ_FOR_EACH_I(__MEMBER_OFFSET__, TYPE, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
  }; \
  using MemberType = typename __DropFirst__<void BOOST_PP_SEQ_FOR_EACH_I(__MEMBER_TYPE__, TYPE, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))>::type; \
}; \
namespace std {\
template <> \
struct formatter<TYPE> { \
  constexpr auto parse(auto& ctx) {\
    return ctx.begin(); \
  }\
  auto format(const TYPE& val, auto& ctx) const {\
    constexpr int64_t buffer_len = 512; \
    char buffer[buffer_len];\
    int64_t pos = 0;\
    G::value_to_string(val, buffer, buffer_len, pos);\
    return std::format_to(ctx.out(), "{}", std::string_view(buffer, pos));\
  }\
};\
}

#define __MEMBER_NAME__(r, data, i, elem) BOOST_PP_STRINGIZE(elem),
#define __MEMBER_OFFSET__(r, data, i, elem) offsetof(data, elem),
#define __MEMBER_TYPE__(r, data, i, elem) , decltype(std::declval<data>().elem)

namespace G
{

template <typename T>
concept Reflectable = requires {
  typename ::REFLECT<T>::MemberType;
};

template <typename T, typename FUNC, int idx = 0>
constexpr void reflect_for_each(T &obj, FUNC &&func) {
  using ORIGIN_TYPE = std::decay_t<T>;
  if constexpr (idx == REFLECT<ORIGIN_TYPE>::MemberCount) {
    return;
  } else {
    using MemberType = std::tuple_element_t<idx, typename REFLECT<ORIGIN_TYPE>::MemberType>;
    auto &member = *reinterpret_cast<MemberType *>((char *)&obj + REFLECT<ORIGIN_TYPE>::MemberOffset[idx]);
    func(REFLECT<ORIGIN_TYPE>::MemberName[idx], member);
    reflect_for_each<T, FUNC, idx + 1>(obj, std::forward<FUNC>(func));
  }
}

}