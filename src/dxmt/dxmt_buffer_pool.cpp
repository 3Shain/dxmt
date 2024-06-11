#include "./dxmt_buffer_pool.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

void BufferPool::GetNext(uint64_t currentSeqId, uint64_t coherentSeqId,
                         MTL::Buffer **next) {
  fifo.push(
      QueueEntry{.buffer = transfer(*next), .will_free_at = currentSeqId});
  for (;;) {
    if (fifo.empty()) {
      break;
    }
    auto entry = fifo.front();
    if (entry.will_free_at > coherentSeqId) {
      break;
    }
    *next = entry.buffer.takeOwnership();
    fifo.pop();
    return;
  }

  auto buffer = device->newBuffer(buffer_len, options);
  // fifo.push(QueueEntry{.buffer = Obj(buffer), .will_free_at = currentSeqId});
  *next = buffer;
}
}; // namespace dxmt