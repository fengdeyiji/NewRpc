#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>
#include <mechanism/static_reflection.hpp>
#include "boost/asio.hpp"
#include "boost/asio/ip/address.hpp"
#include <format>
#include "common.h"
#include "mechanism/stringification.hpp"

namespace G
{

using ASIO_EndPoint = boost::asio::ip::tcp::endpoint;

struct EndPoint {
  enum class IPType : uint32_t {
    INVALID_0 = 0,
    IPV4      = 1,
    IPV6      = 2,
    INVALID_1 = 3,
  };
  static constexpr uint32_t IP_TYPE_MASK = 0x00000000000000000000000000000011b;
  struct IPV4 {
    IPV4() : addr_{0,0,0,0} {}
    bool operator==(const IPV4 &rhs) const {
      return addr_[0] == rhs.addr_[0] &&
             addr_[1] == rhs.addr_[1] &&
             addr_[2] == rhs.addr_[2] &&
             addr_[3] == rhs.addr_[3];
    }
    IPV4(const std::initializer_list<int64_t> &list) {
      assert(list.size() == 4);
      auto iter = list.begin();
      addr_[0] = *iter++;
      addr_[1] = *iter++;
      addr_[2] = *iter++;
      addr_[3] = *iter++;
    }
    IPV4(uint8_t addr[4]) { memcpy(addr_, addr, 4); }
    uint8_t addr_[4];
  };
  struct IPV6 {
    bool operator==(const IPV6 &rhs) const {
      return addr_[0] == rhs.addr_[0] &&
             addr_[1] == rhs.addr_[1] &&
             addr_[2] == rhs.addr_[2] &&
             addr_[3] == rhs.addr_[3] &&
             addr_[4] == rhs.addr_[4] &&
             addr_[5] == rhs.addr_[5] &&
             addr_[6] == rhs.addr_[6] &&
             addr_[7] == rhs.addr_[7];
    }
    uint16_t addr_[8];
  };
  friend class REFLECT<EndPoint>;
  EndPoint() : ip_{}, port_{0} {}
  EndPoint(const IPV4 &ipv4, uint16_t port)
  : port_{port}, flags_{0} { ip_.ipv4_ = ipv4; flags_ |= static_cast<uint32_t>(IPType::IPV4);}
  EndPoint(const EndPoint &endpoint) = default;
  EndPoint(EndPoint &&endpoint) = default;
  EndPoint &operator=(const EndPoint &endpoint) = default;
  EndPoint &operator=(EndPoint &&endpoint) = default;
  bool operator==(const EndPoint &rhs) const {
    bool ret = false;
    if (IPType(flags_ & IP_TYPE_MASK) == IPType::IPV4) {
      ret = (ip_.ipv4_ == rhs.ip_.ipv4_ && port_ == rhs.port_);
    } else if (IPType(flags_ & IP_TYPE_MASK) == IPType::IPV6) {
      ret = (ip_.ipv6_ == rhs.ip_.ipv6_ && port_ == rhs.port_);
    } else {
      ERROR_LOG("invalid compare, this:{}, rhs:{}", *this, rhs);
    }
    return ret;
  }
  void from_asio_end_point(const ASIO_EndPoint &end_point) {
    if (end_point.address().is_v4()) {
      boost::asio::ip::address_v4 v4 = end_point.address().to_v4();
      auto v4_addr = v4.to_bytes();
      ip_.ipv4_.addr_[0] = v4_addr[0];
      ip_.ipv4_.addr_[1] = v4_addr[1];
      ip_.ipv4_.addr_[2] = v4_addr[2];
      ip_.ipv4_.addr_[3] = v4_addr[3];
      flags_ = 0;
      flags_ |= static_cast<uint32_t>(IPType::IPV4);
    } else {
      std::abort(); // FIXME
    }
    port_ = end_point.port();
  }
  ASIO_EndPoint to_asio_endpoint() const {
    char ip_str[16];
    auto result = std::format_to_n(ip_str, sizeof(ip_str), "{}.{}.{}.{}",
                                   ip_.ipv4_.addr_[0],
                                   ip_.ipv4_.addr_[1],
                                   ip_.ipv4_.addr_[2],
                                   ip_.ipv4_.addr_[3]);
    *result.out++ = '\0';
    return {boost::asio::ip::make_address(ip_str), port_};
  }
  EndPoint::IPType ip_type() const {
    bool ret = false;
    uint32_t mask_value = flags_ & IP_TYPE_MASK;
    return static_cast<IPType>(mask_value);
  }
  bool is_valid() const {
    EndPoint::IPType type = ip_type();
    return type == IPType::IPV4 || type == IPType::IPV6;
  }
  uint16_t port() const { return port_; }
  void set_port(uint16_t port) { port_ = port; }
  void to_string(char *buffer, const int64_t buffer_len, int64_t &pos) const {
    auto result = std::format_to_n(buffer + pos, buffer_len - pos, "{}.{}.{}.{}:{}",
                                   ip_.ipv4_.addr_[0],
                                   ip_.ipv4_.addr_[1],
                                   ip_.ipv4_.addr_[2],
                                   ip_.ipv4_.addr_[3],
                                   port_);
    pos += result.out - (buffer + pos);
  }
private:
  union IP {
    IP() : ipv6_{0} {}
    IPV4 ipv4_;
    IPV6 ipv6_;
  } ip_;
  uint16_t port_;
  uint32_t flags_;
};

struct NetBuffer { // RAII
  NetBuffer() : buffer_{nullptr}, buffer_len_{0} {}
  NetBuffer(const uint64_t size)
  : buffer_{new std::byte[size]},
  buffer_len_{size} {}
  ~NetBuffer() {
    if (nullptr != buffer_)
      delete[] buffer_;
  }
  NetBuffer(const NetBuffer &) = delete;
  NetBuffer(NetBuffer &&rhs) {
    buffer_ = rhs.buffer_;
    rhs.buffer_ = nullptr;
    buffer_len_ = rhs.buffer_len_;
    rhs.buffer_len_ = 0;
  }
  NetBuffer &operator=(const NetBuffer &) = delete;
  NetBuffer &operator=(NetBuffer &&rhs) {
    if (&rhs != this) {
      new (this) NetBuffer{std::move(rhs)};
    }
    return *this;
  }
  std::byte *buffer_;
  uint64_t buffer_len_;
};

struct NetBufferView {
  NetBufferView(std::byte *buffer, const uint64_t buffer_len)
  : buffer_{buffer},
  buffer_len_{buffer_len} {}
  NetBufferView(NetBuffer &net_buffer)
  : buffer_{net_buffer.buffer_}, buffer_len_{net_buffer.buffer_len_} {}
  NetBufferView(const NetBufferView &) = default;
  NetBufferView(NetBufferView &&) = default;
  NetBufferView &operator=(const NetBufferView &) = default;
  NetBufferView &operator=(NetBufferView &&) = default;
  std::byte *buffer_;
  uint64_t buffer_len_;
};

enum class FinishCondition : uint8_t {
  INVALID = 0,
  WHEN_ANY = 1,
  WHEN_ALL = 2,
  WHEN_MAJORITY = 3,
};

}
STATIC_REFLECT(G::EndPoint, ip_, port_);
STATIC_REFLECT(G::NetBuffer, buffer_, buffer_len_);
STATIC_REFLECT(G::NetBufferView, buffer_, buffer_len_);