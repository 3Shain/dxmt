#include "dxmt_buffer_pool.hpp"
#include "dxmt_buffer.hpp"
#include "dxmt_texture.hpp"

namespace dxmt {

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


void
DynamicTexturePool2::Rename(uint64_t currentSeqId, uint64_t coherentSeqId) {
  Rc<TextureAllocation> alloc;
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