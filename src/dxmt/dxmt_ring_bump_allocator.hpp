#pragma once

#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLResource.hpp"
#include "log/log.hpp"
#include "thread.hpp"
#include "util_math.hpp"
#include <mutex>
#include <queue>

namespace dxmt {

constexpr size_t kStagingBlockSize = 0x2000000; // 32MB
constexpr size_t kStagingBlockLifetime = 300;

template <bool cpu_visible> class RingBumpAllocator {

public:
  RingBumpAllocator(MTL::Device *device, MTL::ResourceOptions block_options)
      : device(device), block_options(block_options) {}

  std::tuple<void *, MTL::Buffer *, uint64_t> allocate(uint64_t seq_id,
                                                       uint64_t coherent_id,
                                                       size_t size,
                                                       size_t alignment) {
    std::lock_guard<dxmt::mutex> lock(mutex);
    while (!fifo.empty()) {
      auto &latest = fifo.back();
      if ((align(latest.allocated_size, alignment) + size) >
          latest.total_size) {
        break;
      }
      latest.last_used_seq_id = seq_id;
      return suballocate(latest, size, alignment);
    }
    return suballocate(
        allocate_or_reuse_block(
            seq_id,                           //
            coherent_id,                      //
            std::max(size, kStagingBlockSize) // in case required size is larger
                                              // than block size
            ),
        size, alignment);
  };

  void free_blocks(uint64_t coherent_id) {
    std::lock_guard<dxmt::mutex> lock(mutex);
    while (!fifo.empty()) {
      auto front = fifo.front();
      if (front.last_used_seq_id <= coherent_id &&
          (coherent_id - front.last_used_seq_id) > kStagingBlockLifetime) {
        // can be deallocated
        front.buffer_gpu->release();
        if constexpr (cpu_visible) {
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
    MTL::Buffer *buffer_gpu;
    size_t allocated_size;
    size_t total_size;
    uint64_t last_used_seq_id;
  };

  StagingBlock &allocate_or_reuse_block(uint64_t seq_id, uint64_t coherent_id,
                                        size_t block_size) {
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
    if constexpr (cpu_visible) {
      auto cpu = malloc(block_size);
      auto gpu = device->newBuffer(cpu, block_size, block_options, nullptr);
      fifo.push({.buffer_cpu = cpu,
                 .buffer_gpu = gpu,
                 .allocated_size = 0,
                 .total_size = block_size,
                 .last_used_seq_id = seq_id});
    } else {
      auto gpu = device->newBuffer(block_size, block_options);
      fifo.push({.buffer_cpu = nullptr,
                 .buffer_gpu = gpu,
                 .allocated_size = 0,
                 .total_size = block_size,
                 .last_used_seq_id = seq_id});
    }
    return fifo.back();
  };

  std::tuple<void *, MTL::Buffer *, uint64_t>
  suballocate(StagingBlock &block, size_t size, size_t alignment) {
    auto offset = align(block.allocated_size, alignment);
    block.allocated_size = offset + size;
    return {((char *)block.buffer_cpu + offset), block.buffer_gpu, offset};
  };

  std::queue<StagingBlock> fifo;
  MTL::Device *device;
  dxmt::mutex mutex;
  MTL::ResourceOptions block_options;
};

} // namespace dxmt