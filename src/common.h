#pragma once
#include <stddef.h>
#include <cstddef>
#include <boost/hana.hpp>
#include <vector>
#include <memory>

template<typename Test, template<typename...> class Ref>
struct is_specialization_of : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct is_specialization_of<Ref<Args...>, Ref>: std::true_type {};

template< class T, template<class...> class Primary >
inline constexpr bool is_specialization_of_v = is_specialization_of<T, Primary>::value;

template <typename T>
concept hana_reflectable = boost::hana::Struct<T>::value;