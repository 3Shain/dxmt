#pragma once

#include "Foundation/NSAutoreleasePool.hpp"
#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLBlitCommandEncoder.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLCommandQueue.hpp"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLHeap.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "objc_pointer.hpp"
#include "thread.hpp"
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

class CommandChunk {

  template <typename T> class linear_allocator {
  public:
    typedef T value_type;

    linear_allocator() = delete;
    linear_allocator(CommandChunk *chunk) : chunk(chunk){};

    [[nodiscard]] constexpr T *allocate(std::size_t n) {
      return reinterpret_cast<T *>(
          chunk->allocateCPUHeap(n * sizeof(T), alignof(T)));
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
    virtual ~BFunc(){};
  };

  template <typename context, typename F>
  class EFunc final : public BFunc<context> {
  public:
    void invoke(context &ctx) final { std::invoke(func, ctx); };
    ~EFunc() final = default;
    EFunc(F &&ff) : func(std::forward<F>(ff)) {}
    EFunc(const EFunc &copy) = delete;
    EFunc &operator=(const EFunc &copy_assign) = delete;

  private:
    F func;
  };

  template <typename context> class MFunc final : public BFunc<context> {
  public:
    void invoke(context &ctx) final{/* nop */};
    ~MFunc() = default;
  };

  template <typename value_t> struct Node {
    value_t value;
    Node *next;
  };

  struct context_t {
    MTL::CommandBuffer *cmdbuf;
    Obj<MTL::RenderCommandEncoder> render_encoder;
    Obj<MTL::ArgumentEncoder> vs_binding_encoder;
    Obj<MTL::ArgumentEncoder> ps_binding_encoder;
    Obj<MTL::ComputeCommandEncoder> compute_encoder;
    Obj<MTL::ArgumentEncoder> consts_binding_encoder;
    Obj<MTL::BlitCommandEncoder> blit_encoder;
  };

public:
  template <typename T>
  using fixed_vector_on_heap = std::vector<T, linear_allocator<T>>;

  CommandChunk(const CommandChunk &) = delete; // delete copy constructor

  template <typename T> fixed_vector_on_heap<T> allocate(size_t n = 1) {
    linear_allocator<T> allocator(this);
    fixed_vector_on_heap<T> ret(allocator);
    ret.reserve(n);
    return ret;
  }

  void *allocateCPUHeap(size_t size, size_t align) {
    // TODO: proper implementation
    return 0;
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
  }

  void encode(MTL::CommandBuffer *cmdbuf) {
    attached_cmdbuf = cmdbuf;
    context_t context{cmdbuf, {}, {}, {}, {}, {}, {}};
    auto cur = &monoid_list;
    while (true) {
      cur->value->invoke(context);
      if (cur->next == nullptr)
        break;
      cur = cur->next;
    }
  };

private:
  char *cpu_argument_heap;
  Obj<MTL::Heap> gpu_argument_heap;
  uint32_t cpu_arugment_heap_offset;
  uint32_t gpu_arugment_heap_offset;
  MFunc<context> monoid;
  Node<BFunc<context> *> monoid_list;
  Node<BFunc<context> *> *list_end;
  Obj<MTL::CommandBuffer> attached_cmdbuf;

  friend class CommandQueue;

public:
  CommandChunk()
      : monoid(), monoid_list{&monoid, nullptr}, list_end(&monoid_list) {}

  void reset() {
    auto cur = monoid_list.next;
    while (cur != nullptr) {
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

constexpr uint32_t kCommandChunkCount = 16;
constexpr size_t kCommandChunkCPUHeapSize = 0x4000; // 16kb
constexpr size_t kCommandChunkGPUHeapSize = 0x4000;

class CommandQueue {

private:
  uint32_t EncodingThread() {
    env::setThreadName("dxmt-encode-thread");
    uint64_t internal_seq = 0;
    while (!stopped.load()) {
      if (internal_seq == encoding_index.load())
        encoding_index.wait(internal_seq);
      if (stopped.load())
        break;
      // perform...
      auto &chunk = chunks[internal_seq % kCommandChunkCount];

      auto pool = transfer(NS::AutoreleasePool::alloc()->init());
      auto cmdbuf = commandQueue->commandBuffer();
      chunk.encode(cmdbuf);
      wait_for_finish_index.fetch_add(1);
      wait_for_finish_index.notify_all();
      internal_seq++;
    }
    return 0;
  };

  uint32_t WaitForFinishThread() {
    env::setThreadName("dxmt-finish-thread");
    uint64_t internal_seq = 0;
    while (!stopped.load()) {
      if (internal_seq == wait_for_finish_index.load())
        wait_for_finish_index.wait(internal_seq);
      if (stopped.load())
        break;
      auto &chunk = chunks[internal_seq % kCommandChunkCount];
      chunk.attached_cmdbuf->waitUntilCompleted();
      // TODO: perform cleanup
      chunk.reset();
      internal_seq++;
    }
    return 0;
  }

  std::atomic_uint64_t encoding_index = 0;
  std::atomic_uint64_t wait_for_finish_index = 0;
  std::atomic_bool stopped;

  std::array<CommandChunk, kCommandChunkCount> chunks;

  dxmt::thread encodeThread;
  dxmt::thread finishThread;
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
      auto heap_desc = transfer(MTL::HeapDescriptor::alloc()->init());
      heap_desc->setType(MTL::HeapTypePlacement);
      heap_desc->setSize(kCommandChunkGPUHeapSize);
      chunk.gpu_argument_heap = transfer(device->newHeap(heap_desc));
    };
  };

  ~CommandQueue() {
    stopped.store(true);
    encoding_index++;
    encoding_index.notify_all();
    wait_for_finish_index++;
    wait_for_finish_index.notify_all();
    for (unsigned i = 0; i < kCommandChunkCount; i++) {
      auto &chunk = chunks[i];
      chunk.reset();
      free(chunk.cpu_argument_heap);
      chunk.gpu_argument_heap = nullptr;
    };
  }

  CommandChunk *CurrentChunk() {
    // TODO: prevent overflow
    return &chunks[encoding_index.load() % kCommandChunkCount];
  };

  void CommitCurrentChunk() {
    // TODO: prevent overflow
    encoding_index.fetch_add(1);
    encoding_index.notify_all();
  };

  void WaitCPUFence(uint64_t seq) {
    uint64_t current;
    while ((current = wait_for_finish_index.load()) < seq) {
      wait_for_finish_index.wait(current);
    }
  };
};

} // namespace dxmt