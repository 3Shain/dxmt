#pragma once

#include "Foundation/NSAutoreleasePool.hpp"
#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLBlitCommandEncoder.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLCommandQueue.hpp"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLCaptureManager.hpp"
#include "Metal/MTLTypes.hpp"
#include "log/log.hpp"
#include "objc_pointer.hpp"
// #include "thread.hpp"
#include <thread>
#include "util_env.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace dxmt {

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

constexpr uint32_t kCommandChunkCount = 16;
constexpr size_t kCommandChunkCPUHeapSize = 0x4000; // 16kb
constexpr size_t kCommandChunkGPUHeapSize = 0x4000;

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

  class context_t {
  public:
    CommandChunk *chk;
    MTL::CommandBuffer *cmdbuf;
    Obj<MTL::RenderCommandEncoder> render_encoder;
    Obj<MTL::ComputeCommandEncoder> compute_encoder;
    MTL::Size cs_threadgroup_size;
    Obj<MTL::BlitCommandEncoder> blit_encoder;

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
    // TODO: check overflow
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
    // TODO: check overflow
    return {gpu_argument_heap, aligned};
  }

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
    if (cpu_arugment_heap_offset > kCommandChunkCPUHeapSize) {
      ERR("cpu argument heap overflow, expect error.");
    }
  }

  void encode(MTL::CommandBuffer *cmdbuf) {
    attached_cmdbuf = cmdbuf;
    context_t context{
        this, cmdbuf, {}, {}, {}, {},
    };
    auto cur = monoid_list.next;
    while (cur) {
      cur->value->invoke(context);
      cur = cur->next;
    }
  };

private:
  char *cpu_argument_heap;
  Obj<MTL::Buffer> gpu_argument_heap;
  uint64_t cpu_arugment_heap_offset;
  uint64_t gpu_arugment_heap_offset;
  MFunc<context> monoid;
  Node<BFunc<context> *> monoid_list;
  Node<BFunc<context> *> *list_end;
  Obj<MTL::CommandBuffer> attached_cmdbuf;

  friend class CommandQueue;

public:
  CommandChunk()
      : monoid(), monoid_list{&monoid, nullptr}, list_end(&monoid_list) {}

  void reset() noexcept {
    auto cur = monoid_list.next;
    while (cur) {
      cur->value->~BFunc<context>(); // call destructor
      cur = cur->next;
    }
    cpu_arugment_heap_offset = 0;
    gpu_arugment_heap_offset = 0;
    monoid_list.next = nullptr;
    list_end = &monoid_list;
    attached_cmdbuf = nullptr;
  }
};

class CommandQueue {

private:
  uint32_t EncodingThread() {
    env::setThreadName("dxmt-encode-thread");
    uint64_t internal_seq = 0;
    while (!stopped.load()) {
      ready_for_encode.wait(internal_seq, std::memory_order_acquire);
      if (stopped.load())
        break;
      // perform...
      auto &chunk = chunks[internal_seq % kCommandChunkCount];

      auto pool = transfer(NS::AutoreleasePool::alloc()->init());

      auto c = MTL::CaptureManager::sharedCaptureManager();
      auto d = transfer(MTL::CaptureDescriptor::alloc()->init());
      d->setCaptureObject(commandQueue->device());
      d->setDestination(MTL::CaptureDestinationGPUTraceDocument);
      char filename[1024];
      std::time_t now;
      std::time(&now);
      // TODO: absolute path? not a good idea
      std::strftime(filename, 1024,
                    "/Users/sanshain/capture-%H-%M-%S_%m-%d-%y.gputrace",
                    std::localtime(&now));
      auto _pTraceSaveFilePath =
          (NS::String::string(filename, NS::UTF8StringEncoding));
      NS::URL *pURL =
          NS::URL::alloc()->initFileURLWithPath(_pTraceSaveFilePath);

      NS::Error *pError = nullptr;
      d->setOutputURL(pURL);

      c->startCapture(d.ptr(), &pError);

      auto cmdbuf = commandQueue->commandBuffer();
      chunk.encode(cmdbuf);
      cmdbuf->commit();
      // cmdbuf->waitUntilScheduled(); // seems unnecessary

      c->stopCapture();

      ready_for_commit.fetch_add(1, std::memory_order_release);
      ready_for_commit.notify_all();
      internal_seq++;
    }
    TRACE("encoder thread gracefully terminates");
    return 0;
  };

  uint32_t WaitForFinishThread() {
    env::setThreadName("dxmt-finish-thread");
    uint64_t internal_seq = 0;
    while (!stopped.load()) {
      ready_for_commit.wait(internal_seq, std::memory_order_acquire);
      if (stopped.load())
        break;
      auto &chunk = chunks[internal_seq % kCommandChunkCount];
      if (chunk.attached_cmdbuf->status() <=
          MTL::CommandBufferStatusScheduled) {
        chunk.attached_cmdbuf->waitUntilCompleted();
      }
      chunk.reset();
      chunk_ongoing.fetch_sub(1, std::memory_order_relaxed);
      chunk_ongoing.notify_all();
      internal_seq++;
    }
    TRACE("finishing thread gracefully terminates");
    return 0;
  }

  std::atomic_uint64_t ready_for_encode = 0;
  std::atomic_uint64_t ready_for_commit = 0;
  std::atomic_uint64_t chunk_ongoing = 0;
  std::atomic_bool stopped;

  std::array<CommandChunk, kCommandChunkCount> chunks;

  /**
  FIXME: dxmt::thread cause access page fault when
  program shutdown. recheck this later
  */
  std::thread encodeThread;
  std::thread finishThread;
  Obj<MTL::CommandQueue> commandQueue;

public:
  CommandQueue(MTL::Device *device)
      : encodeThread([this]() { this->EncodingThread(); }),
        finishThread([this]() { this->WaitForFinishThread(); }) {
    commandQueue = transfer(device->newCommandQueue(kCommandChunkCount));
    for (unsigned i = 0; i < kCommandChunkCount; i++) {
      auto &chunk = chunks[i];
      chunk.reset();
      chunk.cpu_argument_heap = (char *)malloc(kCommandChunkGPUHeapSize);
      chunk.gpu_argument_heap = transfer(device->newBuffer(
          kCommandChunkGPUHeapSize, MTL::ResourceHazardTrackingModeUntracked |
                                        MTL::ResourceCPUCacheModeWriteCombined |
                                        MTL::ResourceStorageModeShared));
    };
  };

  ~CommandQueue() {
    TRACE("Destructing command queue");
    stopped.store(true);
    ready_for_encode++;
    ready_for_encode.notify_all();
    ready_for_commit++;
    ready_for_commit.notify_all();
    encodeThread.join();
    finishThread.join();
    for (unsigned i = 0; i < kCommandChunkCount; i++) {
      auto &chunk = chunks[i];
      chunk.reset();
      free(chunk.cpu_argument_heap);
      chunk.gpu_argument_heap = nullptr;
    };
    TRACE("Destructed command queue");
  }

  CommandChunk *CurrentChunk() {
    auto id = ready_for_encode.load(std::memory_order_acquire);
    return &chunks[id % kCommandChunkCount];
  };

  /**
  This is not thread-safe!
  CurrentChunk & CommitCurrentChunk should be called on the same thread

  */
  void CommitCurrentChunk() {
    chunk_ongoing.wait(kCommandChunkCount - 1, std::memory_order_relaxed);
    chunk_ongoing.fetch_add(1, std::memory_order_relaxed);
    ready_for_encode.fetch_add(1, std::memory_order_release);
    ready_for_encode.notify_all();
  };

  void WaitCPUFence(uint64_t seq) {
    uint64_t current;
    while ((current = chunk_ongoing.load(std::memory_order_relaxed))) {
      chunk_ongoing.wait(current);
    }
  };
};

} // namespace dxmt