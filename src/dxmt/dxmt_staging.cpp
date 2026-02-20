#include "dxmt_staging.hpp"
#include <cassert>

namespace dxmt {

StagingResource::StagingResource(
  WMT::Device device, uint64_t length, uint32_t bytes_per_row, uint32_t bytes_per_image
) :
    bytesPerRow(bytes_per_row),
    bytesPerImage(bytes_per_image),
    length(length),
    device_(device) {
  buffer_ = new Buffer(length, device);
  immediate_name_ = allocate(0);
  buffer_->rename(allocation(0));
}

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

StagingMapResult
StagingResource::tryMap(uint64_t coherent_seq_id, bool read, bool write) {
  if (mapped)
    return StagingMapResult::Mapped;
  if (read && coherent_seq_id < cpu_coherent_after_finished_seq_id)
    return StagingMapResult(cpu_coherent_after_finished_seq_id - coherent_seq_id);
  if (write && coherent_seq_id < gpu_occupied_until_finished_seq_id) {
    if (coherent_seq_id >= cpu_coherent_after_finished_seq_id)
      return StagingMapResult::Renamable;
    return StagingMapResult(gpu_occupied_until_finished_seq_id - coherent_seq_id);
  }
  return StagingMapResult::Mappable;
}

void
StagingResource::unmap() {
  mapped = false;
}

uint64_t
StagingResource::allocate(uint64_t coherent_seq_id) {
  std::lock_guard<dxmt::mutex> lock(mutex_);
  uint64_t ret = ~0ull;
  for (;;) {
    if (fifo.empty()) {
      break;
    }
    auto entry = fifo.front();
    if (entry.will_free_at > coherent_seq_id) {
      break;
    }
    ret = entry.id;
    fifo.pop();
    break;
  }
  if (ret == ~0ull) {
    Flags<BufferAllocationFlag> flags;
#ifdef __i386__
    flags.set(BufferAllocationFlag::CpuPlaced);
#endif
    buffer_pool.push_back(buffer_->allocate(flags));
    ret = buffer_pool.size() - 1;
  }
  return ret;
}

void
StagingResource::updateImmediateName(uint64_t current_seq_id, uint64_t allocation) {
  std::lock_guard<dxmt::mutex> lock(mutex_);
  fifo.push(QueueEntry{.id = immediate_name_, .will_free_at = current_seq_id});
  immediate_name_ = allocation;
  cpu_coherent_after_finished_seq_id = 0;
  gpu_occupied_until_finished_seq_id = 0;
}

} // namespace dxmt
