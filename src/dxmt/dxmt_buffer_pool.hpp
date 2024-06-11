#pragma once

#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLResource.hpp"
#include "objc_pointer.hpp"
#include <cstddef>
#include <queue>
namespace dxmt {

class BufferPool {
  struct QueueEntry {
    Obj<MTL::Buffer> buffer;
    uint64_t will_free_at;
    // uint64_t offset; TODO: allow multiple entry defined on the same buffer
  };

  std::queue<QueueEntry> fifo;

public:
  BufferPool(MTL::Device *device, size_t buffer_len,
             MTL::ResourceOptions options)
      : device(device), buffer_len(buffer_len), options(options) {};

  void GetNext(uint64_t currentSeqId, uint64_t coherentSeqId,
               MTL::Buffer **next);

  MTL::Device *device;
  size_t buffer_len;
  MTL::ResourceOptions options;
};

}; // namespace dxmt