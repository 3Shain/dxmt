#include "./dxmt_buffer_pool.hpp"
#include "Metal/MTLDevice.hpp"
#include "dxmt_buffer.hpp"

namespace dxmt {

void
BufferPool::GetNext(
    uint64_t currentSeqId, uint64_t coherentSeqId, MTL::Buffer **next, uint64_t *gpuAddr, void **cpuAddr
) {
  fifo.push(QueueEntry{.buffer = *next, .gpu_addr = *gpuAddr, .cpu_addr = *cpuAddr, .will_free_at = currentSeqId});
  for (;;) {
    if (fifo.empty()) {
      break;
    }
    auto entry = fifo.front();
    if (entry.will_free_at > coherentSeqId) {
      break;
    }
    *next = entry.buffer;
    *gpuAddr = entry.gpu_addr;
    *cpuAddr = entry.cpu_addr;
    fifo.pop();
    return;
  }

  auto buffer = device->newBuffer(buffer_len, options);
  *next = buffer;
  *gpuAddr = buffer->gpuAddress();
  *cpuAddr = buffer->contents();
}

void
BufferPool2::Rename(uint64_t currentSeqId, uint64_t coherentSeqId) {
  Rc<BufferAllocation> alloc;
  for (;;) {
    if (fifo.empty()) {
      break;
    }
    auto entry = fifo.front();
    if (entry.will_free_at > coherentSeqId) {
      break;
    }
    alloc = std::move(entry.allocation);
    fifo.pop();
    break;
  }
  if (!alloc.ptr())
    alloc = buffer_->allocate(flags_);
  auto old = buffer_->rename(std::move(alloc));
  if (old.ptr()) {
    fifo.push(QueryEntry{.allocation = std::move(old), .will_free_at = currentSeqId});
  }
}
}; // namespace dxmt