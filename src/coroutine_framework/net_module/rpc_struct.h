#pragma once
#include <cstdint>
#include <assert.h>
#include "mechanism/static_reflection.hpp"

namespace ToE
{

struct PackageHeader {
  enum class MessageType : uint32_t {
    NOT_USED_1 = 0,
    REQUEST  = 1,
    RESPONSE = 2,
    NOT_USED_2 = 3,
  };
  static constexpr uint32_t MAGIC_NUMBER = 0xaabbccdd;
  static constexpr uint32_t MASK_MESSAGE_TYPE_BITS = 0x00000000000000000000000000000011b;
  static constexpr uint16_t VERSION = 1;
  PackageHeader()
  : version_{0},
  reserved_{0},
  server_port_{0},
  flags_{0},
  rpc_id_{0},
  process_restart_counter_{0},
  rpc_type_{0},
  checksum_{MAGIC_NUMBER},
  payload_len_{0} {}
  PackageHeader(const uint16_t process_restart_counter,
                const uint16_t rpc_type,
                const uint64_t rpc_id,
                const uint16_t server_port,
                const uint64_t payload_len)
  : version_{VERSION},
  reserved_{0},
  server_port_{server_port},
  flags_{0},
  rpc_id_{rpc_id},
  process_restart_counter_{process_restart_counter},
  rpc_type_{rpc_type},
  checksum_{MAGIC_NUMBER},
  payload_len_{payload_len} {}
  bool operator==(const PackageHeader &rhs) const {
    return version_ == rhs.version_ &&
           reserved_ == rhs.reserved_ &&
           flags_ == rhs.flags_ &&
           rpc_id_ == rhs.rpc_id_ &&
           process_restart_counter_ == rhs.process_restart_counter_ &&
           rpc_type_ == rhs.rpc_type_ &&
           payload_len_ == rhs.payload_len_;
  }
  void set_message_type(const MessageType type) {
    assert((flags_ & MASK_MESSAGE_TYPE_BITS) == 0);
    flags_ |= static_cast<uint32_t>(type);
  }
  MessageType get_message_type() const {
    return static_cast<MessageType>(flags_ & MASK_MESSAGE_TYPE_BITS);
  }
  PackageHeader(const PackageHeader &) = delete;
  PackageHeader(PackageHeader &&) = delete;
  PackageHeader &operator=(const PackageHeader &) = delete;
  PackageHeader &operator=(PackageHeader &&) = delete;
  uint16_t version_; // for compat reason
  uint16_t reserved_; // not used for now, but for alignment
  uint16_t server_port_; // for response to send
  uint16_t flags_; // for future use, currently unused
  uint64_t rpc_id_; // for response to find request in map
  uint16_t process_restart_counter_; // for restart refuse older message
  uint16_t rpc_type_; // for request processer to find function
  uint32_t checksum_; // for check
  uint64_t payload_len_; // args serialization data
};

}
STATIC_REFLECT(ToE::PackageHeader, version_, reserved_, server_port_, flags_, rpc_id_, process_restart_counter_, rpc_type_, payload_len_, checksum_);