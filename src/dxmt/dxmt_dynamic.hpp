#pragma once
#include "dxmt_buffer.hpp"
#include "dxmt_texture.hpp"
#include <queue>

namespace dxmt {

class DynamicBuffer {
public:
  void incRef();
  void decRef();

  Rc<BufferAllocation> allocate(uint64_t coherent_seq_id);
  void discard(uint64_t current_seq_id, Rc<BufferAllocation> &&allocation);

  DynamicBuffer(Buffer *buffer, Flags<BufferAllocationFlag> flags);

  struct QueueEntry {
    Rc<BufferAllocation> allocation;
    uint64_t will_free_at;
  };

  /**
   * readonly
   */
  Buffer *buffer;

  Rc<BufferAllocation> immediateContextAllocation;

private:
  Flags<BufferAllocationFlag> flags_;
  std::atomic<uint32_t> refcount_ = {0u};
  std::queue<QueueEntry> fifo;
  dxmt::mutex mutex_;
};

class DynamicTexture {
public:
  void incRef();
  void decRef();

  Rc<TextureAllocation> allocate(uint64_t coherent_seq_id);
  void discard(uint64_t current_seq_id, Rc<TextureAllocation> &&allocation);

  DynamicTexture(Texture *buffer, Flags<TextureAllocationFlag> flags);

  struct QueueEntry {
    Rc<TextureAllocation> allocation;
    uint64_t will_free_at;
  };
  /**
   * readonly
   */
  Texture *texture;

  Rc<TextureAllocation> immediateContextAllocation;
private:
  Flags<TextureAllocationFlag> flags_;
  std::atomic<uint32_t> refcount_ = {0u};
  std::queue<QueueEntry> fifo;
  dxmt::mutex mutex_;
};

} // namespace dxmt
