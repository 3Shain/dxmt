#pragma once

#include "dxmt_texture.hpp"
#include <queue>
namespace dxmt {

class DynamicTexturePool2 {

  struct QueryEntry {
    Rc<TextureAllocation> allocation;
    uint64_t will_free_at;
  };

public:
  DynamicTexturePool2(Texture *buffer, Flags<TextureAllocationFlag> flags) : buffer_(buffer), flags_(flags) {}

  Rc<TextureAllocation> allocate(uint64_t coherentSeqId);
  void discard(Rc<TextureAllocation> &&allocation, uint64_t currentSeqId);

private:
  Texture *buffer_;
  std::queue<QueryEntry> fifo;
  Flags<TextureAllocationFlag> flags_;
};

}; // namespace dxmt