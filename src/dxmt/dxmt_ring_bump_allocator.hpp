#pragma once

#include "Metal.hpp"
#include "log/log.hpp"
#include "thread.hpp"
#include "util_math.hpp"
#include <mutex>
#include <queue>

namespace dxmt {

constexpr size_t kStagingBlockSize = 0x2000000; // 32MB
constexpr size_t kStagingBlockSizeForDeferredContext = 0x200000; // 2MB
constexpr size_t kStagingBlockLifetime = 300;

struct AllocatedRingBufferSlice {
  void * mapped_memory;
  WMT::Buffer gpu_buffer;
  uint64_t offset;
  uint64_t gpu_address;
};

template <bool CpuVisible, size_t BlockSize = kStagingBlockSize> class RingBumpAllocator {

public:
  RingBumpAllocator(WMT::Device device, WMTResourceOptions block_options) : device(device) {
    buffer_info.options = block_options;
  }

  std::tuple<void *, WMT::Buffer, uint64_t>
  allocate(uint64_t seq_id, uint64_t coherent_id, size_t size, size_t alignment) {
    auto slice = allocate1(seq_id, coherent_id, size, alignment);
    return {slice.mapped_memory, slice.gpu_buffer, slice.offset};
  };

  AllocatedRingBufferSlice
  allocate1(uint64_t seq_id, uint64_t coherent_id, size_t size, size_t alignment) {
    std::lock_guard<dxmt::mutex> lock(mutex);
    while (!fifo.empty()) {
      auto &latest = fifo.back();
      if ((align(latest.allocated_size, alignment) + size) > latest.total_size) {
        break;
      }
      latest.last_used_seq_id = seq_id;
      return suballocate(latest, size, alignment);
    }
    return suballocate(
        allocate_or_reuse_block(
            seq_id, coherent_id, std::max(size, BlockSize) // in case required size is larger than block size
        ),
        size, alignment
    );
  };

  void
  free_blocks(uint64_t coherent_id) {
    std::lock_guard<dxmt::mutex> lock(mutex);
    while (!fifo.empty()) {
      auto front = fifo.front();
      if (front.last_used_seq_id <= coherent_id && (coherent_id - front.last_used_seq_id) > kStagingBlockLifetime) {
        // can be deallocated
        if constexpr (CpuVisible) {
          free(front.buffer_cpu);
        }
        fifo.pop();
      } else {
        break;
      }
    }
  };

private:
  struct StagingBlock {
    void *buffer_cpu;
    WMT::Reference<WMT::Buffer> buffer_gpu;
    size_t allocated_size;
    size_t total_size;
    uint64_t last_used_seq_id;
    WMTBufferInfo buffer_info;
  };

  StagingBlock &
  allocate_or_reuse_block(uint64_t seq_id, uint64_t coherent_id, size_t block_size) {
    if (!fifo.empty()) {
      auto front = fifo.front();
      if (front.last_used_seq_id < coherent_id) {
        if (front.total_size >= block_size) {
          front.last_used_seq_id = seq_id;
          front.allocated_size = 0;
          fifo.push(front);
          fifo.pop();
          return fifo.back();
        } else {
          ERR("forced to allocate new block of size ", block_size);
        }
      }
    }
    if constexpr (CpuVisible) {
      auto cpu = malloc(block_size);
      buffer_info.length = block_size;
      buffer_info.memory.set(cpu);
      auto gpu = device.newBuffer(buffer_info);
      fifo.push(
          {.buffer_cpu = cpu,
           .buffer_gpu = std::move(gpu),
           .allocated_size = 0,
           .total_size = block_size,
           .last_used_seq_id = seq_id,
           .buffer_info = buffer_info,
          }
      );
    } else {
      buffer_info.length = block_size;
      buffer_info.memory.set(nullptr);
      auto gpu = device.newBuffer(buffer_info);
      fifo.push(
          {.buffer_cpu = nullptr,
           .buffer_gpu = std::move(gpu),
           .allocated_size = 0,
           .total_size = block_size,
           .last_used_seq_id = seq_id,
           .buffer_info = buffer_info,
          }
      );
    }
    return fifo.back();
  };

  AllocatedRingBufferSlice
  suballocate(StagingBlock &block, size_t size, size_t alignment) {
    auto offset = align(block.allocated_size, alignment);
    block.allocated_size = offset + size;
    return {((char *)block.buffer_cpu + offset), block.buffer_gpu, offset, block.buffer_info.gpu_address};
  };

  std::queue<StagingBlock> fifo;
  WMT::Device device;
  dxmt::mutex mutex;
  WMTBufferInfo buffer_info;
};

} // namespace dxmt