#pragma once

#include "Metal/MTLBuffer.hpp"
#include <queue>
namespace dxmt {

class BufferPool {
  struct QueueEntry {
    // manually manage ownership
    MTL::Buffer *buffer;
    uint64_t gpu_addr;
    void *cpu_addr;
    uint64_t will_free_at;
    // uint64_t offset; TODO: allow multiple entry defined on the same buffer
  };

  std::queue<QueueEntry> fifo;

public:
  BufferPool(MTL::Device *device, size_t buffer_len, MTL::ResourceOptions options) :
      device(device),
      buffer_len(buffer_len),
      options(options) {};

  ~BufferPool() {
    while (fifo.size()) {
      fifo.front().buffer->release();
      fifo.pop();
    }
  };

  void GetNext(uint64_t currentSeqId, uint64_t coherentSeqId, MTL::Buffer **next, uint64_t *gpuAddr, void **cpuAddr);

  MTL::Device *device;
  size_t buffer_len;
  MTL::ResourceOptions options;
};

}; // namespace dxmt