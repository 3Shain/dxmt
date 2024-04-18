#pragma once

#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLCommandQueue.hpp"
#include "Metal/MTLHeap.hpp"
#include "objc_pointer.hpp"
#include "rc/util_rc.hpp"
#include "thread.hpp"
#include "util_env.hpp"
#include <atomic>
#include <cstdint>
#include <map>
#include <queue>
#include <vector>

namespace dxmt {

class CommandChunk : public RcObject {

  template <typename T> struct CommandChunkHeapLinearAllocator {
    typedef T value_type;

    CommandChunkHeapLinearAllocator() = delete;
    CommandChunkHeapLinearAllocator(CommandChunk *chunk) : chunk(chunk){};

    [[nodiscard]] constexpr T *allocate(std::size_t n) {
      return reinterpret_cast<T *>(
          chunk->allocateCPUHeap(n * sizeof(T), alignof(T)));
    }

    constexpr void deallocate(T *p, [[maybe_unused]] std::size_t n) noexcept {
      // do nothing
    }

    bool
    operator==(const CommandChunkHeapLinearAllocator<T> &rhs) const noexcept {
      return chunk == rhs.chunk;
    }

    bool
    operator!=(const CommandChunkHeapLinearAllocator<T> &rhs) const noexcept {
      return !(*this == rhs);
    }

    CommandChunk *chunk;
  };

public:
  template <typename T>
  using fixed_vector_on_heap =
      std::vector<T, CommandChunkHeapLinearAllocator<T>>;

  CommandChunk(const CommandChunk &) = delete; // delete copy constructor

  template <typename T> fixed_vector_on_heap<T> allocate(size_t n = 1) {
    CommandChunkHeapLinearAllocator<T> allocator(this);
    fixed_vector_on_heap<T> ret(allocator);
    ret.reserve(n);
    return ret;
  }

  void *allocateCPUHeap(size_t size, size_t align) {
    // TODO: proper implementation
    return 0;
  }

  char *cpu_argument_heap;
  Obj<MTL::Heap> gpu_argument_heap;
  uint32_t cpu_arugment_heap_offset;
  uint32_t gpu_arugment_heap_offset;
};

class CommandQueue {

public:
  CommandQueue()
      : encodeThread([this]() { this->EncodingThread(); }),
        finishThread([this]() { this->FinishThread(); }){};

  ~CommandQueue() { stopped.store(true); }

private:
  uint32_t EncodingThread() {
    env::setThreadName("dxmt-encode-thread");
    while (!stopped.load()) {

      auto cmdbuf = commandQueue->commandBuffer();
    }
    return 0;
  };

  uint32_t FinishThread() {
    env::setThreadName("dxmt-finish-thread");
    while (!stopped.load()) {
    }
    return 0;
  }

  // std::queue<CommandBuffer> pendingCmdBufferQueue;
  // std::queue<CommandBuffer> ongoingCmdBufferQueue;
  std::atomic_bool stopped;

  dxmt::thread encodeThread;
  dxmt::thread finishThread;
  MTL::CommandQueue *commandQueue;
};

} // namespace dxmt