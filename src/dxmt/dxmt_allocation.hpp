#pragma once

#include <atomic>
#include <cstdint>

namespace dxmt {

class Allocation {
public:
  virtual ~Allocation(){};

  void incRef();
  void decRef();

private:
  std::atomic<uint32_t> refcount_ = {0u};
};

} // namespace dxmt
