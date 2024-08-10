#include "dxmt_command_queue.hpp"
#include "Metal/MTLCaptureManager.hpp"
#include "Metal/MTLFunctionLog.hpp"
#include "Foundation/NSAutoreleasePool.hpp"
#include "util_env.hpp"
#include <atomic>

#define ASYNC_ENCODING 1

namespace dxmt {

ENCODER_CLEARPASS_INFO *CommandChunk::mark_clear_pass() {
  linear_allocator<ENCODER_CLEARPASS_INFO> allocator(this);
  auto ptr = allocator.allocate(1);
  new (ptr) ENCODER_CLEARPASS_INFO();
  ptr->encoder_id = queue->GetNextEncoderId();
  last_encoder_info = (ENCODER_INFO *)ptr;
  return ptr;
};

ENCODER_INFO *CommandChunk::mark_pass(EncoderKind kind) {
  linear_allocator<ENCODER_INFO> allocator(this);
  auto ptr = allocator.allocate(1);
  ptr->kind = kind;
  ptr->encoder_id = queue->GetNextEncoderId();
  last_encoder_info = ptr;
  return ptr;
};

CommandQueue::CommandQueue(MTL::Device *device)
    : encodeThread([this]() { this->EncodingThread(); }),
      finishThread([this]() { this->WaitForFinishThread(); }),
      staging_allocator(device, MTL::ResourceOptionCPUCacheModeWriteCombined |
                                    MTL::ResourceHazardTrackingModeUntracked |
                                    MTL::ResourceStorageModeShared),
      copy_temp_allocator(device, MTL::ResourceHazardTrackingModeUntracked |
                                      MTL::ResourceStorageModePrivate),
      clear_cmd(device) {
  commandQueue = transfer(device->newCommandQueue(kCommandChunkCount));
  for (unsigned i = 0; i < kCommandChunkCount; i++) {
    auto &chunk = chunks[i];
    chunk.queue = this;
    chunk.cpu_argument_heap = (char *)malloc(kCommandChunkCPUHeapSize);
    chunk.gpu_argument_heap = transfer(device->newBuffer(
        kCommandChunkGPUHeapSize, MTL::ResourceHazardTrackingModeUntracked |
                                      MTL::ResourceCPUCacheModeWriteCombined |
                                      MTL::ResourceStorageModeShared));
    chunk.visibility_result_heap =
        transfer(device->newBuffer(kOcclusionSampleCount * sizeof(uint64_t),
                                   MTL::ResourceHazardTrackingModeUntracked |
                                       MTL::ResourceStorageModeShared));
    chunk.gpu_argument_heap_contents =
        (uint64_t *)chunk.gpu_argument_heap->contents();
    chunk.reset();
  };
}

CommandQueue::~CommandQueue() {
  TRACE("Destructing command queue");
  stopped.store(true);
  ready_for_encode++;
  ready_for_encode.notify_one();
  ready_for_commit++;
  ready_for_commit.notify_one();
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

void CommandQueue::CommitCurrentChunk(uint64_t occlusion_counter_begin,
                                      uint64_t occlusion_counter_end) {
  chunk_ongoing.wait(kCommandChunkCount - 1, std::memory_order_acquire);
  chunk_ongoing.fetch_add(1, std::memory_order_relaxed);
  auto &chunk = chunks[
    ready_for_encode.load(std::memory_order_relaxed) % kCommandChunkCount
  ];
  chunk.frame_ = present_seq;
  chunk.visibility_result_seq_begin = occlusion_counter_begin;
  chunk.visibility_result_seq_end = occlusion_counter_end;
#if ASYNC_ENCODING
  ready_for_encode.fetch_add(1, std::memory_order_release);
  ready_for_encode.notify_one();
#else
  CommitChunkInternal(chunk, 
    ready_for_encode.fetch_add(1, std::memory_order_relaxed)
  );
#endif
}

void CommandQueue::CommitChunkInternal(CommandChunk &chunk, uint64_t seq) {

  auto pool = transfer(NS::AutoreleasePool::alloc()->init());

  // TODO:
  bool should_capture = false;

  auto c = MTL::CaptureManager::sharedCaptureManager();
  if (should_capture) {
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
    NS::URL *pURL = NS::URL::alloc()->initFileURLWithPath(_pTraceSaveFilePath);

    NS::Error *pError = nullptr;
    d->setOutputURL(pURL);

    c->startCapture(d.ptr(), &pError);
  }

  chunk.attached_cmdbuf = commandQueue->commandBuffer();
  auto cmdbuf = chunk.attached_cmdbuf;
  chunk.encode(cmdbuf);
  cmdbuf->commit();

  if (should_capture) {

    c->stopCapture();
  }

  ready_for_commit.fetch_add(1, std::memory_order_release);
  ready_for_commit.notify_one();
}

uint32_t CommandQueue::EncodingThread() {
#if ASYNC_ENCODING
  env::setThreadName("dxmt-encode-thread");
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
  uint64_t internal_seq = 1;
  while (!stopped.load()) {
    ready_for_encode.wait(internal_seq, std::memory_order_acquire);
    if (stopped.load())
      break;
    // perform...
    auto &chunk = chunks[internal_seq % kCommandChunkCount];
    CommitChunkInternal(chunk, internal_seq);
    internal_seq++;
  }
  TRACE("encoder thread gracefully terminates");
#endif
  return 0;
}

uint32_t CommandQueue::WaitForFinishThread() {
  env::setThreadName("dxmt-finish-thread");
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
  uint64_t internal_seq = 1;
  while (!stopped.load()) {
    ready_for_commit.wait(internal_seq, std::memory_order_acquire);
    if (stopped.load())
      break;
    auto &chunk = chunks[internal_seq % kCommandChunkCount];
    if (chunk.attached_cmdbuf->status() <= MTL::CommandBufferStatusScheduled) {
      chunk.attached_cmdbuf->waitUntilCompleted();
    }
    if (chunk.attached_cmdbuf->status() == MTL::CommandBufferStatusError) {
      ERR("Device error at frame ", chunk.frame_, ", : ",
          chunk.attached_cmdbuf->error()->localizedDescription()->cString(
              NS::ASCIIStringEncoding));
    }
    if (chunk.attached_cmdbuf->logs()) {
      if (((NS::Array *)chunk.attached_cmdbuf->logs())->count()) {
        ERR("logs at frame ", chunk.frame_);
        ERR(chunk.attached_cmdbuf->logs()->debugDescription()->cString(
            NS::ASCIIStringEncoding));
      }
    }
    uint64_t *visibility_result_buffer =
        (uint64_t *)chunk.visibility_result_heap->contents();
    {
      std::lock_guard<dxmt::mutex> lock(mutex_observers);
      for (auto seq = chunk.visibility_result_seq_begin;
           seq < chunk.visibility_result_seq_end; seq++) {
        uint64_t counter_value =
            visibility_result_buffer[seq % kOcclusionSampleCount];
        std::erase_if(visibility_result_observers,
                      [=](VisibilityResultObserver *observer) {
                        return observer->Update(seq, counter_value);
                      });
      }
    }

    chunk.reset();
    cpu_coherent.fetch_add(1, std::memory_order_relaxed);
    chunk_ongoing.fetch_sub(1, std::memory_order_release);
    cpu_coherent.notify_all();
    chunk_ongoing.notify_one();

    staging_allocator.free_blocks(internal_seq);

    internal_seq++;
  }
  TRACE("finishing thread gracefully terminates");
  return 0;
}
} // namespace dxmt
