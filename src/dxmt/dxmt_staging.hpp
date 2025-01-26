#pragma once
#include "Metal/MTLBuffer.hpp"
#include "objc_pointer.hpp"
#include "thread.hpp"
#include <cstdint>
#include <atomic>
#include <queue>

namespace dxmt {

enum class StagingMapResult : uint64_t {
  Mappable = 0,
  Renamable = 0xffffffffffffffff,
  Mapped = 0xfffffffffffffffe,
};

class StagingResource {
public:
  void incRef();
  void decRef();

  void useCopyDestination(uint64_t seq_id);
  void useCopySource(uint64_t seq_id);

  StagingMapResult tryMap(uint64_t coherent_seq_id, bool read, bool write);
  void unmap();

  Obj<MTL::Buffer> allocate(uint64_t coherent_seq_id);
  void updateImmediateName(uint64_t current_seq_id, Obj<MTL::Buffer> &&allocation);

  Obj<MTL::Buffer>
  immediateName() {
    return name_;
  };

  void *
  mappedMemory() {
    return name_->contents();
  }

  Obj<MTL::Buffer> current;
  /**
  readonly
   */
  uint32_t bytesPerRow;
  /**
  readonly
   */
  uint32_t bytesPerImage;

  StagingResource(Obj<MTL::Buffer> &&buffer, uint32_t bytes_per_row, uint32_t bytes_per_image);

private:
  struct QueueEntry {
    Obj<MTL::Buffer> allocation;
    uint64_t will_free_at;
  };

  Obj<MTL::Buffer> name_;
  std::atomic<uint32_t> refcount_ = {0u};
  std::queue<QueueEntry> fifo;
  dxmt::mutex mutex_;
  bool mapped = false;
  // prevent read from staging before
  uint64_t cpu_coherent_after_finished_seq_id = 0;
  // prevent write to staging before
  uint64_t gpu_occupied_until_finished_seq_id = 0;
};

} // namespace dxmt