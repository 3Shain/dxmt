#pragma once
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dxmt {

using CounterHandle = uint64_t;

struct Counter {
  MTL::Buffer *Buffer;
  uint64_t Offset;
};

class CounterPool {
public:
  CounterPool(MTL::Device *device) : device(device) {};
  ~CounterPool();

  CounterHandle AllocateCounter(uint64_t SeqId, uint32_t InitialValue);
  void DiscardCounter(uint64_t SeqId, CounterHandle Handle);
  Counter GetCounter(CounterHandle Handle);

  void FillCounters(uint64_t SeqId, MTL::CommandBuffer *pCmdbuf);

  void ReleaseCounters(uint64_t SeqId);

private:
  std::vector<Counter> counter_pool_;
  std::vector<CounterHandle> free_;
  std::unordered_map<uint64_t, std::vector<std::pair<CounterHandle, uint32_t>>>
      to_initialize_;
  std::unordered_map<uint64_t, std::vector<CounterHandle>> to_discard_;
  std::vector<MTL::Buffer *> underlying_buffers_;
  MTL::Device *device;
};

} // namespace dxmt