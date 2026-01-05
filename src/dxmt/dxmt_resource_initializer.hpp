#pragma once
#include "Metal.hpp"
#include "dxmt_buffer.hpp"
#include "dxmt_context.hpp"
#include "dxmt_ring_bump_allocator.hpp"
#include "dxmt_texture.hpp"

namespace dxmt {

constexpr size_t kResourceInitializerCpuCommandHeapSize = 0x100000; // 1MB
constexpr size_t kResourceInitializerGpuUploadHeapSize = 0x2000000; // 32MB
constexpr size_t kResourceInitializerGpuUploadHeapAlignment = 256;
constexpr size_t kResourceInitializerChunks = 2;

static_assert(kResourceInitializerChunks > 1);

class ResourceInitializer {
public:
  ResourceInitializer(WMT::Device device);
  ~ResourceInitializer();

  uint64_t initWithZero(BufferAllocation *buffer, uint64_t offset, uint64_t length);

  uint64_t initDepthStencilWithZero(
      const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level, uint32_t dsv_planar
  );
  uint64_t
  initRenderTargetWithZero(const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level);
  uint64_t initWithZero(const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level);
  uint64_t initWithData(
      const Texture *texture, TextureAllocation *allocation, uint32_t slice, uint32_t level, const void *data,
      size_t row_pitch, size_t depth_pitch
  );

  /*
   * Flush pending works and return the event id to wait
   * 0 may be returned, meaning no work to wait
   */
  uint64_t flushToWait();

  void
  wait(uint64_t seq_id, uint64_t timeout = -1) {
    if (cached_coherent_seq_id >= seq_id)
      return;
    upload_queue_event_.waitUntilSignaledValue(seq_id, timeout);
  }

  WMT::Event
  event() {
    return upload_queue_event_;
  }

private:
  uint64_t flushInternal();

  struct ClearRenderPassInfo {
    WMTRenderPassInfo info;
    ClearRenderPassInfo *next;
  };

  bool
  idle() {
    if (blit_cmd_head.next.ptr)
      return false;
    if (clear_render_pass_head.next)
      return false;
    return true;
  }

  void reset();

  void encode(WMT::CommandBuffer cmdbuf);

  template <typename T>
  T *
  allocateCpuHeap() {
    constexpr size_t size = sizeof(T);
    constexpr size_t alignment = alignof(T);
    size_t adjustment = align_forward_adjustment((void *)cpu_command_heap_offset, alignment);
    auto aligned = cpu_command_heap_offset + adjustment;
    if (aligned + size > kResourceInitializerCpuCommandHeapSize)
      return nullptr;
    cpu_command_heap_offset = aligned + size;
    return reinterpret_cast<T *>(ptr_add(cpu_command_heap, aligned));
  }

  template <typename T>
  bool
  allocateBlit(T **p) {
    if (auto ptr = (wmtcmd_base *)allocateCpuHeap<T>()) {
      blit_cmd_tail->next.set(ptr);
      blit_cmd_tail = ptr;
      ptr->next.set(nullptr);
      *p = (T *)ptr;
      return true;
    }
    return false;
  }

  bool
  allocateClear(WMTRenderPassInfo **p) {
    if (auto ptr = allocateCpuHeap<ClearRenderPassInfo>()) {
      clear_render_pass_tail->next = ptr;
      clear_render_pass_tail = ptr;
      ptr->next = nullptr;
      WMT::InitializeRenderPassInfo(ptr->info);
      *p = &ptr->info;
      return true;
    }
    return false;
  }

  WMT::Buffer allocateGpuHeap(size_t size, size_t &offset);

  WMT::Buffer allocateZeroBuffer(size_t size);

  bool retainAllocation(Allocation *allocation);

  uint64_t current_seq_id_ = 1;
  uint64_t cached_coherent_seq_id = 0;
  WMT::Device device_;
  WMT::Reference<WMT::CommandQueue> upload_queue_;
  WMT::Reference<WMT::SharedEvent> upload_queue_event_;
  WMT::Reference<WMT::Buffer> zero_buffer_;
  size_t zero_buffer_size_ = 0;
  dxmt::mutex mutex_;

  void *cpu_command_heap;
  size_t cpu_command_heap_size;
  size_t cpu_command_heap_offset;

  RingBumpState<StagingBufferBlockAllocator, kResourceInitializerGpuUploadHeapSize> gpu_command_heap_allocator;

  wmtcmd_blit_nop blit_cmd_head;
  wmtcmd_base *blit_cmd_tail;

  ClearRenderPassInfo clear_render_pass_head;
  ClearRenderPassInfo *clear_render_pass_tail;

  AllocationRefTracking ref_tracker;
};

} // namespace dxmt