#include "net_define.h"

namespace G
{

bool EndPoint::IPV4::operator==(const IPV4 &rhs) const {
  return addr_[0] == rhs.addr_[0] &&
          addr_[1] == rhs.addr_[1] &&
          addr_[2] == rhs.addr_[2] &&
          addr_[3] == rhs.addr_[3];
}

EndPoint::IPV4::IPV4(const std::initializer_list<int64_t> &list) {
  assert(list.size() == 4);
  auto iter = list.begin();
  addr_[0] = *iter++;
  addr_[1] = *iter++;
  addr_[2] = *iter++;
  addr_[3] = *iter++;
}

bool EndPoint::IPV6::operator==(const IPV6 &rhs) const {
  return addr_[0] == rhs.addr_[0] &&
          addr_[1] == rhs.addr_[1] &&
          addr_[2] == rhs.addr_[2] &&
          addr_[3] == rhs.addr_[3] &&
          addr_[4] == rhs.addr_[4] &&
          addr_[5] == rhs.addr_[5] &&
          addr_[6] == rhs.addr_[6] &&
          addr_[7] == rhs.addr_[7];
}

bool EndPoint::operator==(const EndPoint &rhs) const {
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

void EndPoint::from_asio_end_point(const ASIO_EndPoint &end_point) {
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

ASIO_EndPoint EndPoint::to_asio_endpoint() const {
  char ip_str[16];
  auto result = std::format_to_n(ip_str, sizeof(ip_str), "{}.{}.{}.{}",
                                  ip_.ipv4_.addr_[0],
                                  ip_.ipv4_.addr_[1],
                                  ip_.ipv4_.addr_[2],
                                  ip_.ipv4_.addr_[3]);
  *result.out++ = '\0';
  return {boost::asio::ip::make_address(ip_str), port_};
}

EndPoint::IPType EndPoint::ip_type() const {
  bool ret = false;
  uint32_t mask_value = flags_ & IP_TYPE_MASK;
  return static_cast<IPType>(mask_value);
}

bool EndPoint::is_valid() const {
  EndPoint::IPType type = ip_type();
  return type == IPType::IPV4 || type == IPType::IPV6;
}

void EndPoint::to_string(char *buffer, const int64_t buffer_len, int64_t &pos) const {
  auto result = std::format_to_n(buffer + pos, buffer_len - pos, "{}.{}.{}.{}:{}",
                                  ip_.ipv4_.addr_[0],
                                  ip_.ipv4_.addr_[1],
                                  ip_.ipv4_.addr_[2],
                                  ip_.ipv4_.addr_[3],
                                  port_);
  pos += result.out - (buffer + pos);
}

NetBuffer::NetBuffer(NetBuffer &&rhs) {
  buffer_ = rhs.buffer_;
  rhs.buffer_ = nullptr;
  buffer_len_ = rhs.buffer_len_;
  rhs.buffer_len_ = 0;
}

NetBuffer &NetBuffer::operator=(NetBuffer &&rhs) {
  if (&rhs != this) {
    new (this) NetBuffer{std::move(rhs)};
  }
  return *this;
}

}