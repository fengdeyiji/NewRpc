#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include <common.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <assert.h>
#include <source_location>
#include "spdlog/spdlog.h"
#include <spdlog/sinks/stdout_color_sinks.h> // 控制台彩色输出

namespace ToE {
enum class LogLevel { trace, debug, info, warn, error, critical };

inline void print_memory_content(const std::byte *ptr, const uint64_t len, const std::source_location loc = std::source_location::current()) {
  std::stringstream ss;
  using word = int32_t;
  word *p_address = (word *)(ptr);
  int word_each_line = 4;
  int lines = (len - 1) / (word_each_line * sizeof(word)) + 1;
  for (int line_idx = 0; line_idx < lines; ++line_idx) {
    ss << "\n" << std::setfill('0') << std::setw(8) << std::hex << p_address << ":";
    for (int word_idx = 0; word_idx < word_each_line && ((char *)p_address < (char *)ptr + len); ++word_idx) {
      ss << "\t0x" << std::setfill('0') << std::setw(8) << std::hex << *p_address;
      ++p_address;
    }
  }
  std::cout << "print at: " << loc.function_name()
            << ":line " << loc.line()
            << ", addr:" << ptr << ", size:" << len
            << ss.str() << std::endl;
}

class Logger {
public:
  static void init(LogLevel level) {
    g_logger_ = spdlog::stdout_color_mt("console");
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%#:%!()] %v");
    switch (level) {
      case LogLevel::trace:
        spdlog::set_level(spdlog::level::trace);
        break;
      case LogLevel::debug:
        spdlog::set_level(spdlog::level::debug);
        break;
      case LogLevel::info:
        spdlog::set_level(spdlog::level::info);
        break;
      case LogLevel::warn:
        spdlog::set_level(spdlog::level::warn);
        break;
      case LogLevel::error:
        spdlog::set_level(spdlog::level::err);
        break;
      case LogLevel::critical:
        spdlog::set_level(spdlog::level::critical);
        break;
    }
  }
  static std::shared_ptr<spdlog::logger> g_logger_;
};

// #define __ARG_TO_STRING__(r, fmt, i, elem) \
//   before_pos = pos; \
//   ToE::value_to_string(elem, buffer, buffer_len, pos); \
//   std::string_view __arg_##i##__(buffer + before_pos, pos - before_pos);
// #define __ARG_TO_FMT__(r, fmt, i, elem) \
//   __arg_##i##__,

#define __ARG_TO_STRING__(r, data, elem) \
value_to_string(elem, buffer, buffer_len, pos, false); \
std::string_view __to_string_arg__##r (buffer + before_pos, pos - before_pos); \
before_pos = pos;
#define __ARG_TO_LIST__(r, data, elem) \
, __to_string_arg__##r


  
#define DEBUG_LOG(...)
#define INFO_LOG(fmt, ...) \
do {\
  constexpr int64_t buffer_len = 1_KiB; \
  char buffer[buffer_len]; \
  int64_t before_pos = 0; \
  int64_t pos = 0; \
  __VA_OPT__(BOOST_PP_SEQ_FOR_EACH(__ARG_TO_STRING__, fmt, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))\
  SPDLOG_LOGGER_INFO(Logger::g_logger_, fmt __VA_OPT__(BOOST_PP_SEQ_FOR_EACH(__ARG_TO_LIST__, fmt, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))); \
} while(0)


#define WARN_LOG(...)
#define ERROR_LOG(...)

}