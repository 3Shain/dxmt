#include "dxmt_counter_pool.hpp"
#include "Foundation/NSAutoreleasePool.hpp"
#include "Foundation/NSString.hpp"
#include "Metal/MTLBlitCommandEncoder.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLDevice.hpp"
#include "objc_pointer.hpp"
#include <cassert>

namespace dxmt {

/**
 * TODO: allocate a larger buffer then suballocate
 * It's non-trivial due to hazard tracking
 * Sharing a tracked buffer introduce unintentional dependency
 */
constexpr size_t kCounterBufferSize = 4;

CounterPool::~CounterPool() {
  for (auto buffer : underlying_buffers_) {
    buffer->release();
  }
}

CounterHandle CounterPool::AllocateCounter(uint64_t SeqId,
                                           uint32_t InitialValue) {
  if (!free_.size()) {
    auto pool = transfer(NS::AutoreleasePool::alloc()->init());
    MTL::Buffer *buffer =
        device->newBuffer(kCounterBufferSize, MTL::ResourceStorageModePrivate);
    underlying_buffers_.push_back(buffer);
    buffer->setLabel(
        NS::String::string("dxmt_uav_counter", NS::UTF8StringEncoding));
    for (unsigned i = 0; i < kCounterBufferSize; i += 4) {
      free_.push_back(counter_pool_.size());
      counter_pool_.push_back({buffer, i});
    }
  }

  CounterHandle handle = free_.back();
  free_.pop_back();

  auto inserted = to_initialize_.insert({SeqId, {}});
  inserted.first->second.push_back({handle, InitialValue});

  return handle;
}

void CounterPool::DiscardCounter(uint64_t SeqId, CounterHandle Handle) {
  auto inserted = to_discard_.insert({SeqId, {}});
  inserted.first->second.push_back(Handle);
}

Counter CounterPool::GetCounter(CounterHandle Handle) {
  assert(Handle < counter_pool_.size());
  return counter_pool_[Handle];
}

void CounterPool::FillCounters(uint64_t SeqId, MTL::CommandBuffer *pCmdbuf) {
  if (!to_initialize_.contains(SeqId)) {
    return;
  }
  auto encoder = pCmdbuf->blitCommandEncoder();
  auto initialize_list = to_initialize_[SeqId];
  for (auto &[handle, value] : initialize_list) {
    auto &counter = counter_pool_[handle];
    encoder->fillBuffer(counter.Buffer, {counter.Offset, 4}, value);
  }
  to_initialize_.erase(SeqId);
  encoder->endEncoding();
}

void CounterPool::ReleaseCounters(uint64_t SeqId) {
  if (!to_discard_.contains(SeqId)) {
    return;
  }
  auto discard_list = to_discard_[SeqId];
  for (auto handle : discard_list) {
    free_.push_back(handle);
  }
  to_discard_.erase(SeqId);
}

} // namespace dxmt