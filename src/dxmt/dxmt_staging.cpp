#include "dxmt_staging.hpp"

namespace dxmt {

StagingResource::StagingResource(Obj<MTL::Buffer> &&buffer, uint32_t bytes_per_row, uint32_t bytes_per_image) :
    current(std::move(buffer)),
    bytesPerRow(bytes_per_row),
    bytesPerImage(bytes_per_image) {}

void
StagingResource::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
StagingResource::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

void
StagingResource::useCopyDestination(uint64_t seq_id) {
  assert(!mapped && "write to staging resource while being mapped");
  // coherent read-after-write
  cpu_coherent_after_finished_seq_id = std::max(seq_id, cpu_coherent_after_finished_seq_id);
  // coherent write-after-write
  gpu_occupied_until_finished_seq_id = std::max(seq_id, gpu_occupied_until_finished_seq_id);
}
void
StagingResource::useCopySource(uint64_t seq_id) {
  assert(!mapped && "read from staging resource while being mapped");
  gpu_occupied_until_finished_seq_id = std::max(seq_id, gpu_occupied_until_finished_seq_id);
}

int64_t
StagingResource::tryMap(uint64_t coherent_seq_id, bool read, bool write) {
  if (mapped)
    return -1;
  if (read && coherent_seq_id < cpu_coherent_after_finished_seq_id) {
    return cpu_coherent_after_finished_seq_id - coherent_seq_id;
  }
  if (write && coherent_seq_id < gpu_occupied_until_finished_seq_id) {
    return gpu_occupied_until_finished_seq_id - coherent_seq_id;
  }
  return 0;
}
void
StagingResource::unmap() {
  mapped = false;
}

} // namespace dxmt
