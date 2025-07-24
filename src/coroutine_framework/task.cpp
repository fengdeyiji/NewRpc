#include "task.h"
#include "coroutine_framework/net_module/net_define.h"
#include "coroutine_framework/net_module/rpc_struct.h"
#include "coroutine_framework/scheduler.h"
#include "mechanism/serialization.hpp"
#include <cassert>

namespace G
{

void send_result_to_caller(const EndPoint &endpoint, NetBuffer &&net_buffer) {
  TLS_FRAMEWORK->get_net_module().commit_send_response_task(endpoint, std::move(net_buffer));
}

void prepare_buffer(const ResponseInfo &response_info,
                    const uint64_t payload_len,
                    NetBuffer &net_buffer,
                    int64_t &pos) {
  PackageHeader header{1,
                       response_info.response_rpc_type_,
                       response_info.response_to_coro_id_,
                       TLS_FRAMEWORK->get_net_module().get_listen_port(),
                       payload_len};
  header.set_message_type(PackageHeader::MessageType::RESPONSE);
  uint64_t header_size = Serializer<PackageHeader>::get_serialize_size(header);
  uint64_t serialize_size = header_size + payload_len;
  new (&net_buffer) NetBuffer(serialize_size);
  Serializer<PackageHeader>::serialize(header, net_buffer.buffer_, net_buffer.buffer_len_, pos);
  assert(pos == header_size);
}

}