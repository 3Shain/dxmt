#include "dxmt_buffer_pool.hpp"
#include "dxmt_texture.hpp"

namespace dxmt {

Rc<TextureAllocation>
DynamicTexturePool2::allocate(uint64_t coherentSeqId) {
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
  return alloc;
}

void DynamicTexturePool2::discard(Rc<TextureAllocation>&& allocation, uint64_t currentSeqId) {
  if (allocation.ptr()) {
    fifo.push(QueryEntry{.allocation = std::move(allocation), .will_free_at = currentSeqId});
  }
}

}; // namespace dxmt