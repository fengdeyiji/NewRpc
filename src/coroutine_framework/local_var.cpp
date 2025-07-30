#include "local_var.h"

namespace G
{

std::atomic<uint64_t> CoroLocalVar::ID = 0;

CoroLocalVar::~CoroLocalVar() {
  if (response_info_) {
    delete response_info_;
    response_info_ = nullptr;
  }
}

}