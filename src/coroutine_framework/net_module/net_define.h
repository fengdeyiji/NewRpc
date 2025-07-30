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
class LinkedCoroutine;

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
    bool operator==(const IPV4 &rhs) const;
    IPV4(const std::initializer_list<int64_t> &list);
    IPV4(uint8_t addr[4]) { memcpy(addr_, addr, 4); }
    uint8_t addr_[4];
  };
  struct IPV6 {
    bool operator==(const IPV6 &rhs) const;
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
  bool operator==(const EndPoint &rhs) const;
  void from_asio_end_point(const ASIO_EndPoint &end_point);
  ASIO_EndPoint to_asio_endpoint() const;
  EndPoint::IPType ip_type() const;
  bool is_valid() const;
  uint16_t port() const { return port_; }
  void set_port(uint16_t port) { port_ = port; }
  void to_string(char *buffer, const int64_t buffer_len, int64_t &pos) const;
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
  ~NetBuffer() { if (buffer_) delete[] buffer_; }
  NetBuffer(const NetBuffer &) = delete;
  NetBuffer(NetBuffer &&rhs);
  NetBuffer &operator=(const NetBuffer &) = delete;
  NetBuffer &operator=(NetBuffer &&rhs);
  std::byte *buffer_;
  uint64_t buffer_len_;
};

struct NetBufferView {
  NetBufferView(std::byte *buffer, const uint64_t buffer_len)
  : buffer_{buffer}, buffer_len_{buffer_len} {}
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

struct CoRpcCallBack {
  virtual void process_buffer_cb(const EndPoint endpoint,
                                 const NetBuffer &net_buffer,
                                 LinkedCoroutine *&coroutine) = 0;
  virtual void timeout_cb() = 0;
};

}
STATIC_REFLECT(G::EndPoint, ip_, port_);
STATIC_REFLECT(G::NetBuffer, buffer_, buffer_len_);
STATIC_REFLECT(G::NetBufferView, buffer_, buffer_len_);