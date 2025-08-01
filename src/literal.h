#pragma once
#include <cstdint>
#include <format>

constexpr uint64_t operator""_B(unsigned long long val) { return val; }
constexpr uint64_t operator""_KiB(unsigned long long val) { return val * 1024_B; }
constexpr uint64_t operator""_MiB(unsigned long long val) { return val * 1024_KiB; }
constexpr uint64_t operator""_GiB(unsigned long long val) { return val * 1024_MiB; }
constexpr uint64_t operator""_TiB(unsigned long long val) { return val * 1024_GiB; }

constexpr uint64_t operator""_ns(unsigned long long val) { return val; }
constexpr uint64_t operator""_us(unsigned long long val) { return val * 1000_ns; }
constexpr uint64_t operator""_ms(unsigned long long val) { return val * 1000_us; }
constexpr uint64_t operator""_s(unsigned long long val) { return val * 1000_ms; }
constexpr uint64_t operator""_min(unsigned long long val) { return val * 60_s; }
constexpr uint64_t operator""_hour(unsigned long long val) { return val * 60_min; }
constexpr uint64_t operator""_day(unsigned long long val) { return val * 24_hour; }

namespace ToE
{
struct TS {
  constexpr TS(uint64_t ts)
  : ts_{ts} {}
  uint64_t ts_;
};
}
template <> struct std::formatter<ToE::TS> {
  constexpr auto parse(auto &ctx) { return ctx.begin(); }
  auto format(const ToE::TS &val, auto &ctx) const {
    double num = 0.0;
    const char *literal = "";
    if (val.ts_ > 1_hour) {
      num = val.ts_ * 1.0 / 1_hour;
      literal = "hour";
    } else if (val.ts_ > 1_min) {
      num = val.ts_ * 1.0 / 1_min;
      literal = "min";
    } else if (val.ts_ > 1_s) {
      num = val.ts_ * 1.0 / 1_s;
      literal = "s";
    } else if (val.ts_ > 1_ms) {
      num = val.ts_ * 1.0 / 1_ms;
      literal = "ms";
    } else if (val.ts_ > 1_us) {
      num = val.ts_ * 1.0 / 1_us;
      literal = "us";
    } else if (val.ts_ > 1_ns) {
      num = val.ts_ * 1.0 / 1_ns;
      literal = "ns";
    }
    return std ::format_to(ctx.out(), "{:.2f}{}", num, literal);
  }
};