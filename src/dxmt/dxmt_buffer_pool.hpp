#pragma once

#include "dxmt_buffer.hpp"
#include "dxmt_texture.hpp"
#include <queue>
namespace dxmt {

class BufferPool2 {

  struct QueryEntry {
    Rc<BufferAllocation> allocation;
    uint64_t will_free_at;
  };

public:
  BufferPool2(Buffer *buffer, Flags<BufferAllocationFlag> flags) : buffer_(buffer), flags_(flags) {}

  Rc<BufferAllocation> allocate(uint64_t coherentSeqId);
  void discard(Rc<BufferAllocation> &&allocation, uint64_t currentSeqId);

private:
  Buffer *buffer_;
  std::queue<QueryEntry> fifo;
  Flags<BufferAllocationFlag> flags_;
};

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