#pragma once

#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLBlitCommandEncoder.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLCommandQueue.hpp"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLTypes.hpp"
#include "dxmt_binding.hpp"
#include "dxmt_capture.hpp"
#include "dxmt_command.hpp"
#include "dxmt_counter_pool.hpp"
#include "dxmt_occlusion_query.hpp"
#include "dxmt_ring_bump_allocator.hpp"
#include "log/log.hpp"
#include "objc_pointer.hpp"
#include "thread.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace dxmt {

enum class EncoderKind : uint32_t { Nil, ClearPass, Render, Compute, Blit, Resolve };

struct ENCODER_INFO {
  EncoderKind kind;
  uint64_t encoder_id;
};

struct ENCODER_RENDER_INFO {
  EncoderKind kind = EncoderKind::Render;
  uint64_t encoder_id;
  uint32_t tessellation_pass = 0;
};

struct ENCODER_CLEARPASS_INFO {
  EncoderKind kind = EncoderKind::ClearPass;
  uint64_t encoder_id;
  uint32_t skipped = 0; // this can be flipped by later render pass
  uint32_t num_color_attachments = 0;
  uint32_t depth_stencil_flags = 0;
  BindingRef clear_depth_stencil_attachment;
  float clear_depth = 0.0;
  uint8_t clear_stencil = 0;
  std::array<BindingRef, 8> clear_color_attachments;
  std::array<MTL::ClearColor, 8> clear_colors;

  struct CLEARPASS_CLEANUP {
    ENCODER_CLEARPASS_INFO *info;
    CLEARPASS_CLEANUP(ENCODER_CLEARPASS_INFO *info) : info(info) {}
    ~CLEARPASS_CLEANUP() {
      // FIXME: too complicated
      if (!info || info->skipped)
        return;
      for (unsigned i = 0; i < info->num_color_attachments; i++) {
        info->clear_color_attachments[i].~BindingRef();
      }
      info->clear_depth_stencil_attachment.~BindingRef();
    }
    CLEARPASS_CLEANUP(const CLEARPASS_CLEANUP &copy) = delete;
    CLEARPASS_CLEANUP(CLEARPASS_CLEANUP &&move) {
      info = move.info;
      move.info = nullptr;
    };
  };

  [[nodiscard("")]] CLEARPASS_CLEANUP use_clearpass() {
    return CLEARPASS_CLEANUP(this);
  }
};

template <typename F, typename context>
concept cpu_cmd = requires(F f, context &ctx) {
  { f(ctx) } -> std::same_as<void>;
};

inline std::size_t
align_forward_adjustment(const void *const ptr,
                         const std::size_t &alignment) noexcept {
  const auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
  const auto aligned = (iptr - 1u + alignment) & -alignment;
  return aligned - iptr;
}

inline void *ptr_add(const void *const p,
                     const std::uintptr_t &amount) noexcept {
  return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(p) + amount);
}

constexpr uint32_t kCommandChunkCount = 8;
constexpr size_t kCommandChunkCPUHeapSize = 0x800000; // is 8MB too large?
constexpr size_t kCommandChunkGPUHeapSize = 0x400000;
constexpr size_t kOcclusionSampleCount = 1024;

class CommandQueue;

class CommandChunk {

  template <typename T> class linear_allocator {
  public:
    typedef T value_type;

    linear_allocator() = delete;
    linear_allocator(CommandChunk *chunk) : chunk(chunk) {};

    [[nodiscard]] constexpr T *allocate(std::size_t n) {
      return reinterpret_cast<T *>(
          chunk->allocate_cpu_heap(n * sizeof(T), alignof(T)));
    }

    constexpr void deallocate(T *p, [[maybe_unused]] std::size_t n) noexcept {
      // do nothing
    }

    bool operator==(const linear_allocator<T> &rhs) const noexcept {
      return chunk == rhs.chunk;
    }

    bool operator!=(const linear_allocator<T> &rhs) const noexcept {
      return !(*this == rhs);
    }

    CommandChunk *chunk;
  };

  template <typename context> class BFunc {
  public:
    virtual void invoke(context &) = 0;
    virtual ~BFunc() noexcept {};
  };

  template <typename context, typename F>
  class EFunc final : public BFunc<context> {
  public:
    void invoke(context &ctx) final { std::invoke(func, ctx); };
    ~EFunc() noexcept final = default;
    EFunc(F &&ff) : func(std::forward<F>(ff)) {}
    EFunc(const EFunc &copy) = delete;
    EFunc &operator=(const EFunc &copy_assign) = delete;

  private:
    F func;
  };

  template <typename context> class MFunc final : public BFunc<context> {
  public:
    void invoke(context &ctx) final { /* nop */ };
    ~MFunc() noexcept = default;
  };

  template <typename value_t> struct Node {
    value_t value;
    Node *next;
  };

  class context_t : public EncodingContext {
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

    MTL::RenderPipelineState* tess_mesh_pso;
    MTL::RenderPipelineState* tess_raster_pso;
    uint32_t tess_num_output_control_point_element;
    uint32_t tess_num_output_patch_constant_scalar;
    uint32_t tess_threads_per_patch;

    context_t(CommandChunk *chk, MTL::CommandBuffer *cmdbuf)
        : chk(chk), queue(chk->queue), cmdbuf(cmdbuf) {}

  private:
  };

public:
  template <typename T>
  using fixed_vector_on_heap = std::vector<T, linear_allocator<T>>;

  CommandChunk(const CommandChunk &) = delete; // delete copy constructor

  template <typename T> fixed_vector_on_heap<T> reserve_vector(size_t n = 1) {
    linear_allocator<T> allocator(this);
    fixed_vector_on_heap<T> ret(allocator);
    ret.reserve(n);
    return ret;
  }

  void *allocate_cpu_heap(size_t size, size_t alignment) {
    std::size_t adjustment =
        align_forward_adjustment((void *)cpu_arugment_heap_offset, alignment);
    auto aligned = cpu_arugment_heap_offset + adjustment;
    cpu_arugment_heap_offset = aligned + size;
    if (cpu_arugment_heap_offset >= kCommandChunkCPUHeapSize) {
      ERR(cpu_arugment_heap_offset,
          " - cpu argument heap overflow, expect error.");
    }
    return ptr_add(cpu_argument_heap, aligned);
  }

  std::pair<MTL::Buffer *, uint64_t> inspect_gpu_heap() {
    return {gpu_argument_heap, gpu_arugment_heap_offset};
  }

  std::pair<MTL::Buffer *, uint64_t> allocate_gpu_heap(size_t size,
                                                       size_t alignment) {
    std::size_t adjustment =
        align_forward_adjustment((void *)gpu_arugment_heap_offset, alignment);
    auto aligned = gpu_arugment_heap_offset + adjustment;
    gpu_arugment_heap_offset = aligned + size;
    if (gpu_arugment_heap_offset > kCommandChunkGPUHeapSize) {
      ERR("gpu argument heap overflow, expect error.");
    }
    return {gpu_argument_heap, aligned};
  }

  uint64_t *gpu_argument_heap_contents;

  using context = context_t;

  template <cpu_cmd<context> F> void emit(F &&func) {
    linear_allocator<EFunc<context, F>> allocator(this);
    auto ptr = allocator.allocate(1);
    new (ptr) EFunc<context, F>(std::forward<F>(func)); // in placement
    linear_allocator<Node<BFunc<context> *>>            // force break
        allocator_node(this);
    auto ptr_node = allocator_node.allocate(1);
    *ptr_node = {ptr, nullptr};
    list_end->next = ptr_node;
    list_end = ptr_node;
  }

  void encode(MTL::CommandBuffer *cmdbuf) {
    attached_cmdbuf = cmdbuf;
    context_t context(this, cmdbuf);
    auto cur = monoid_list.next;
    while (cur) {
      assert((uint64_t)cur->value >= (uint64_t)cpu_argument_heap);
      assert((uint64_t)cur->value <
             ((uint64_t)cpu_argument_heap + cpu_arugment_heap_offset));
      cur->value->invoke(context);
      cur = cur->next;
    }
  };

  ENCODER_RENDER_INFO *mark_render_pass();

  ENCODER_CLEARPASS_INFO *mark_clear_pass();

  ENCODER_INFO *mark_pass(EncoderKind kind);

  ENCODER_INFO *get_last_encoder() { return last_encoder_info; }

  uint64_t current_encoder_id() { return last_encoder_info->encoder_id; }

  uint32_t has_no_work_encoded_yet() {
    return last_encoder_info->kind == EncoderKind::Nil ? 1u : 0u;
  }

  uint64_t frame_;
  uint64_t visibility_result_seq_begin;
  uint64_t visibility_result_seq_end;
  Obj<MTL::Buffer> visibility_result_heap;

private:
  CommandQueue *queue;
  char *cpu_argument_heap;
  Obj<MTL::Buffer> gpu_argument_heap;
  uint64_t cpu_arugment_heap_offset;
  uint64_t gpu_arugment_heap_offset;
  MFunc<context> monoid;
  Node<BFunc<context> *> monoid_list;
  Node<BFunc<context> *> *list_end;
  Obj<MTL::CommandBuffer> attached_cmdbuf;
  ENCODER_INFO init_encoder_info{EncoderKind::Nil, 0};
  ENCODER_INFO *last_encoder_info;
  uint64_t encoder_id;

  friend class CommandQueue;

public:
  CommandChunk()
      : monoid(), monoid_list{&monoid, nullptr}, list_end(&monoid_list),
        last_encoder_info(&init_encoder_info) {}

  void reset() noexcept {
    auto cur = monoid_list.next;
    while (cur) {
      assert((uint64_t)cur->value >= (uint64_t)cpu_argument_heap);
      assert((uint64_t)cur->value <
             ((uint64_t)cpu_argument_heap + cpu_arugment_heap_offset));
      cur->value->~BFunc<context>(); // call destructor
      cur = cur->next;
    }
    cpu_arugment_heap_offset = 0;
    gpu_arugment_heap_offset = 0;
    monoid_list.next = nullptr;
    list_end = &monoid_list;
    attached_cmdbuf = nullptr;
    last_encoder_info = &init_encoder_info;
  }
};

class CommandQueue {

private:
  void CommitChunkInternal(CommandChunk &chunk, uint64_t seq);

  uint32_t EncodingThread();

  uint32_t WaitForFinishThread();

  std::atomic_uint64_t ready_for_encode =
      1; // we start from 1, so 0 is always coherent
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
  uint64_t GetNextEncoderId() { return encoder_seq++; }

  std::vector<VisibilityResultObserver *> visibility_result_observers;
  dxmt::mutex mutex_observers;

  RingBumpAllocator<true> staging_allocator;
  RingBumpAllocator<false> copy_temp_allocator;
  CaptureState capture_state;

public:
  DXMTCommandContext clear_cmd;
  CounterPool counter_pool;

  CommandQueue(MTL::Device *device);

  ~CommandQueue();

  void RegisterVisibilityResultObserver(VisibilityResultObserver *observer) {
    std::lock_guard<dxmt::mutex> lock(mutex_observers);
    visibility_result_observers.push_back(observer);
  }

  CommandChunk *CurrentChunk() {
    auto id = ready_for_encode.load(std::memory_order_relaxed);
    return &chunks[id % kCommandChunkCount];
  };

  uint64_t CoherentSeqId() {
    return cpu_coherent.load(std::memory_order_acquire);
  };

  uint64_t CurrentSeqId() {
    return ready_for_encode.load(std::memory_order_relaxed);
  };

  uint64_t EncodedWorkFinishAt() {
    auto id = ready_for_encode.load(std::memory_order_relaxed);
    return id - chunks[id % kCommandChunkCount].has_no_work_encoded_yet();
  };

  /**
  This is not thread-safe!
  CurrentChunk & CommitCurrentChunk should be called on the same thread

  */
  void CommitCurrentChunk(uint64_t occlusion_counter_begin,
                          uint64_t occlusion_counter_end);

  void PresentBoundary() { present_seq++; }

  void WaitCPUFence(uint64_t seq) {
    uint64_t current;
    while ((current = cpu_coherent.load(std::memory_order_relaxed))) {
      if (current == seq) {
        return;
      }
      cpu_coherent.wait(current, std::memory_order_acquire);
    }
  };

  void FIXME_YieldUntilCoherenceBoundaryUpdate(uint64_t seq_id) {
    cpu_coherent.wait(seq_id, std::memory_order_acquire);
  };

  std::tuple<void *, MTL::Buffer *, uint64_t>
  AllocateStagingBuffer(size_t size, size_t alignment) {
    return staging_allocator.allocate(ready_for_encode,
                                      cpu_coherent.load(std::memory_order_acquire),
                                      size, alignment);
  }

  std::tuple<void *, MTL::Buffer *, uint64_t>
  AllocateTempBuffer(uint64_t seq, size_t size, size_t alignment) {
    return copy_temp_allocator.allocate(seq,
                                        cpu_coherent.load(std::memory_order_acquire),
                                        size, alignment);
  }
};

} // namespace dxmt