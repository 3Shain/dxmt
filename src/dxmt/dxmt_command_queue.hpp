#pragma once

#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLBlitCommandEncoder.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLCommandQueue.hpp"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLEvent.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLTypes.hpp"
#include "dxmt_capture.hpp"
#include "dxmt_command.hpp"
#include "dxmt_command_list.hpp"
#include "dxmt_context.hpp"
#include "dxmt_occlusion_query.hpp"
#include "dxmt_ring_bump_allocator.hpp"
#include "log/log.hpp"
#include "objc_pointer.hpp"
#include "thread.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

namespace dxmt {

struct CLEAR_DEPTH_STENCIL {
  float depth;
  uint8_t stencil;
};

template <typename T> class moveonly_list {
public:
  moveonly_list(T *storage, size_t size) : storage(storage), size_(size) {
    for (unsigned i = 0; i < size_; i++) {
      new (storage + i) T();
    }
  }

  moveonly_list(const moveonly_list &copy) = delete;
  moveonly_list(moveonly_list &&move) {
    storage = move.storage;
    move.storage = nullptr;
    size_ = move.size_;
    move.size_ = 0;
  };

  ~moveonly_list() {
    for (unsigned i = 0; i < size_; i++) {
      storage[i].~T();
    }
  };

  T &
  operator[](int index) {
    return storage[index];
  }

  std::span<T>
  span() const {
    return std::span<T>(storage, size_);
  }

  T *
  data() const {
    return storage;
  }

  size_t
  size() const {
    return size_;
  }

private:
  T *storage;
  size_t size_;
};

constexpr uint32_t kCommandChunkCount = 8;

class CommandQueue;

class CommandChunk {
public:

  class context_t {
  public:
    CommandChunk *chk;
    CommandQueue *queue;
    MTL::CommandBuffer *cmdbuf;
    Obj<MTL::RenderCommandEncoder> render_encoder;
    Obj<MTL::ComputeCommandEncoder> compute_encoder;
    MTL::Size cs_threadgroup_size{};
    Obj<MTL::BlitCommandEncoder> blit_encoder;
    // we don't need an extra reference here
    // since it's guaranteed to be captured by closure
    MTL::Buffer *current_index_buffer_ref{};
    uint32_t dsv_planar_flags = 0;

    MTL::RenderPipelineState *tess_mesh_pso;
    MTL::RenderPipelineState *tess_raster_pso;
    uint32_t tess_num_output_control_point_element;
    uint32_t tess_num_output_patch_constant_scalar;
    uint32_t tess_threads_per_patch;

    uint64_t offset_base = 0;
    uint64_t visibility_offset_base = 0;

    context_t(CommandChunk *chk, MTL::CommandBuffer *cmdbuf) : chk(chk), queue(chk->queue), cmdbuf(cmdbuf) {}

  private:
  };

public:
  CommandChunk(const CommandChunk &) = delete; // delete copy constructor

  void *
  allocate_cpu_heap(size_t size, size_t alignment) {
    std::size_t adjustment = align_forward_adjustment((void *)cpu_arugment_heap_offset, alignment);
    auto aligned = cpu_arugment_heap_offset + adjustment;
    cpu_arugment_heap_offset = aligned + size;
    if (cpu_arugment_heap_offset >= kCommandChunkCPUHeapSize) {
      ERR(cpu_arugment_heap_offset, " - cpu argument heap overflow, expect error.");
    }
    return ptr_add(cpu_argument_heap, aligned);
  }

  std::pair<MTL::Buffer *, uint64_t>
  inspect_gpu_heap() {
    return {gpu_argument_heap, gpu_arugment_heap_offset};
  }

  std::pair<MTL::Buffer *, uint64_t>
  allocate_gpu_heap(size_t size, size_t alignment) {
    std::size_t adjustment = align_forward_adjustment((void *)gpu_arugment_heap_offset, alignment);
    auto aligned = gpu_arugment_heap_offset + adjustment;
    gpu_arugment_heap_offset = aligned + size;
    if (gpu_arugment_heap_offset > kCommandChunkGPUHeapSize) {
      ERR("gpu argument heap overflow, expect error.");
    }
    return {gpu_argument_heap, aligned};
  }

  using context = context_t;

  template <CommandWithContext<ArgumentEncodingContext> F>
  void
  emitcc(F &&func) {
    list_enc.emit(std::forward<F>(func), allocate_cpu_heap(list_enc.calculateCommandSize<F>(), 16));
  }

  void
  encode(MTL::CommandBuffer *cmdbuf, ArgumentEncodingContext &enc) {
    enc.$$setEncodingBuffer(
      cpu_argument_heap,
      cpu_arugment_heap_offset,
      gpu_argument_heap.ptr(),
      gpu_arugment_heap_offset,
      chunk_id
    );
    list_enc.execute(enc);
    attached_cmdbuf = cmdbuf;
    visibility_readback = enc.flushCommands(cmdbuf, chunk_id);
  };

  uint32_t
  has_no_work_encoded_yet() {
    return cpu_arugment_heap_offset == 0;
  }

  uint64_t chunk_id;
  uint64_t frame_;
  std::unique_ptr<VisibilityResultReadback> visibility_readback;

private:
  CommandQueue *queue;
  char *cpu_argument_heap;
  Obj<MTL::Buffer> gpu_argument_heap;
  uint64_t cpu_arugment_heap_offset;
  uint64_t gpu_arugment_heap_offset;
  Obj<MTL::CommandBuffer> attached_cmdbuf;
  
  CommandList<ArgumentEncodingContext> list_enc;

  friend class CommandQueue;

public:
  CommandChunk() {}

  void
  reset() noexcept {
    visibility_readback = {};
    list_enc.~CommandList();
    cpu_arugment_heap_offset = 0;
    gpu_arugment_heap_offset = 0;
    attached_cmdbuf = nullptr;
  }
};

class CommandQueue {

private:
  void CommitChunkInternal(CommandChunk &chunk, uint64_t seq);

  uint32_t EncodingThread();

  uint32_t WaitForFinishThread();

  std::atomic_uint64_t ready_for_encode = 1; // we start from 1, so 0 is always coherent
  std::atomic_uint64_t ready_for_commit = 1;
  std::atomic_uint64_t chunk_ongoing = 0;
  std::atomic_uint64_t cpu_coherent = 0;
  std::atomic_bool stopped;

  std::array<CommandChunk, kCommandChunkCount> chunks;
  uint64_t encoder_seq = 1;
  uint64_t present_seq = 0;

  dxmt::thread encodeThread;
  dxmt::thread finishThread;
  Obj<MTL::CommandQueue> commandQueue;

  friend class CommandChunk;
  uint64_t
  GetNextEncoderId() {
    return encoder_seq++;
  }

  RingBumpAllocator<true> staging_allocator;
  RingBumpAllocator<false> copy_temp_allocator;
  CaptureState capture_state;

public:
  EmulatedCommandContext emulated_cmd;
  ArgumentEncodingContext argument_encoding_ctx;
  Obj<MTL::SharedEvent> event;
  std::uint64_t current_event_seq_id = 1;

  CommandQueue(MTL::Device *device);

  ~CommandQueue();

  CommandChunk *
  CurrentChunk() {
    auto id = ready_for_encode.load(std::memory_order_relaxed);
    return &chunks[id % kCommandChunkCount];
  };

  uint64_t
  CoherentSeqId() {
    return cpu_coherent.load(std::memory_order_acquire);
  };

  uint64_t
  CurrentSeqId() {
    return ready_for_encode.load(std::memory_order_relaxed);
  };

  uint64_t
  EncodedWorkFinishAt() {
    auto id = ready_for_encode.load(std::memory_order_relaxed);
    return id - chunks[id % kCommandChunkCount].has_no_work_encoded_yet();
  };

  uint64_t
  GetNextEventSeqId() {
    return current_event_seq_id++;
  };

  uint64_t
  SignaledEventSeqId() {
    return event->signaledValue();
  };

  /**
  This is not thread-safe!
  CurrentChunk & CommitCurrentChunk should be called on the same thread

  */
  void CommitCurrentChunk();

  void
  PresentBoundary() {
    present_seq++;
  }

  void
  WaitCPUFence(uint64_t seq) {
    uint64_t current;
    while ((current = cpu_coherent.load(std::memory_order_relaxed))) {
      if (current == seq) {
        return;
      }
      cpu_coherent.wait(current, std::memory_order_acquire);
    }
  };

  void
  FIXME_YieldUntilCoherenceBoundaryUpdate(uint64_t seq_id) {
    cpu_coherent.wait(seq_id, std::memory_order_acquire);
  };

  std::tuple<void *, MTL::Buffer *, uint64_t>
  AllocateStagingBuffer(size_t size, size_t alignment) {
    return staging_allocator.allocate(ready_for_encode, cpu_coherent.load(std::memory_order_acquire), size, alignment);
  }

  std::tuple<void *, MTL::Buffer *, uint64_t>
  AllocateTempBuffer(uint64_t seq, size_t size, size_t alignment) {
    return copy_temp_allocator.allocate(seq, cpu_coherent.load(std::memory_order_acquire), size, alignment);
  }
};

} // namespace dxmt