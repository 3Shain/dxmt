#pragma once
#include "Metal/MTLBuffer.hpp"
#include "objc_pointer.hpp"
#include <cstdint>

namespace dxmt {

class StagingResource {
public:
  void incRef();
  void decRef();

  void useCopyDestination(uint64_t seq_id);
  void useCopySource(uint64_t seq_id);

  int64_t tryMap(uint64_t coherent_seq_id, bool read, bool write);
  void unmap();

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
  std::atomic<uint32_t> refcount_ = {0u};
  bool mapped = false;
  // prevent read from staging before
  uint64_t cpu_coherent_after_finished_seq_id = 0;
  // prevent write to staging before
  uint64_t gpu_occupied_until_finished_seq_id = 0;
};

} // namespace dxmt