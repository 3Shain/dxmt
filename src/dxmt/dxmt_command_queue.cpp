#include "dxmt_command_queue.hpp"
#include "Metal/MTLCaptureManager.hpp"
#include "Metal/MTLFunctionLog.hpp"
#include "Foundation/NSAutoreleasePool.hpp"
#include "util_env.hpp"
#include <atomic>

#define ASYNC_ENCODING 1

namespace dxmt {

CommandQueue::CommandQueue(MTL::Device *device) :
    encodeThread([this]() { this->EncodingThread(); }),
    finishThread([this]() { this->WaitForFinishThread(); }),
    staging_allocator(
        device, MTL::ResourceOptionCPUCacheModeWriteCombined | MTL::ResourceHazardTrackingModeUntracked |
                    MTL::ResourceStorageModeShared
    ),
    copy_temp_allocator(device, MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceStorageModePrivate),
    emulated_cmd(device),
    argument_encoding_ctx(*this, device) {
  commandQueue = transfer(device->newCommandQueue(kCommandChunkCount));
  for (unsigned i = 0; i < kCommandChunkCount; i++) {
    auto &chunk = chunks[i];
    chunk.queue = this;
    chunk.cpu_argument_heap = (char *)malloc(kCommandChunkCPUHeapSize);
    chunk.gpu_argument_heap = transfer(device->newBuffer(
        kCommandChunkGPUHeapSize, MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceCPUCacheModeWriteCombined |
                                      MTL::ResourceStorageModeShared
    ));
    chunk.reset();
  };
  event = transfer(device->newSharedEvent());

  std::string env = env::getEnvVar("DXMT_CAPTURE_FRAME");

  if (!env.empty()) {
    try {
      capture_state.scheduleNextFrameCapture(std::stoull(env));
    } catch (const std::invalid_argument &) {
    }
  }
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

void
CommandQueue::CommitCurrentChunk() {
  chunk_ongoing.wait(kCommandChunkCount - 1, std::memory_order_acquire);
  chunk_ongoing.fetch_add(1, std::memory_order_relaxed);
  auto chunk_id = ready_for_encode.load(std::memory_order_relaxed);
  auto &chunk = chunks[chunk_id % kCommandChunkCount];
  chunk.chunk_id = chunk_id;
  chunk.frame_ = present_seq;
#if ASYNC_ENCODING
  ready_for_encode.fetch_add(1, std::memory_order_release);
  ready_for_encode.notify_one();
#else
  CommitChunkInternal(chunk, ready_for_encode.fetch_add(1, std::memory_order_relaxed));
#endif
}

void
CommandQueue::CommitChunkInternal(CommandChunk &chunk, uint64_t seq) {

  auto pool = transfer(NS::AutoreleasePool::alloc()->init());

  switch (capture_state.getNextAction(chunk.frame_)) {
  case CaptureState::NextAction::StartCapture: {
    auto capture_mgr = MTL::CaptureManager::sharedCaptureManager();
    auto capture_desc = transfer(MTL::CaptureDescriptor::alloc()->init());
    capture_desc->setCaptureObject(commandQueue->device());
    capture_desc->setDestination(MTL::CaptureDestinationGPUTraceDocument);
    char filename[1024];
    std::time_t now;
    std::time(&now);
    std::strftime(filename, 1024, "-capture-%H-%M-%S_%m-%d-%y.gputrace", std::localtime(&now));
    auto fileUrl = env::getUnixPath(env::getExeBaseName() + filename);
    WARN("A new capture will be saved to ", fileUrl);
    NS::URL *pURL = NS::URL::alloc()->initFileURLWithPath(NS::String::string(fileUrl.c_str(), NS::UTF8StringEncoding));

    NS::Error *pError = nullptr;
    capture_desc->setOutputURL(pURL);

    capture_mgr->startCapture(capture_desc.ptr(), &pError);
    break;
  }
  case CaptureState::NextAction::StopCapture: {
    auto capture_mgr = MTL::CaptureManager::sharedCaptureManager();
    capture_mgr->stopCapture();
    break;
  }
  case CaptureState::NextAction::Nothing: {
    if (CaptureState::shouldCaptureNextFrame()) {
      capture_state.scheduleNextFrameCapture(chunk.frame_ + 1);
    }
    break;
  }
  }

  chunk.attached_cmdbuf = commandQueue->commandBuffer();
  auto cmdbuf = chunk.attached_cmdbuf;
  chunk.encode(cmdbuf, this->argument_encoding_ctx);
  cmdbuf->commit();

  ready_for_commit.fetch_add(1, std::memory_order_release);
  ready_for_commit.notify_one();
}

uint32_t
CommandQueue::EncodingThread() {
#if ASYNC_ENCODING
  env::setThreadName("dxmt-encode-thread");
  __pthread_set_qos_class_self_np(__QOS_CLASS_USER_INTERACTIVE, 0);
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

uint32_t
CommandQueue::WaitForFinishThread() {
  env::setThreadName("dxmt-finish-thread");
  __pthread_set_qos_class_self_np(__QOS_CLASS_USER_INTERACTIVE, 0);
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
      ERR("Device error at frame ", chunk.frame_,
          ", : ", chunk.attached_cmdbuf->error()->localizedDescription()->cString(NS::ASCIIStringEncoding));
    }
    if (chunk.attached_cmdbuf->logs()) {
      if (((NS::Array *)chunk.attached_cmdbuf->logs())->count()) {
        ERR("logs at frame ", chunk.frame_);
        ERR(chunk.attached_cmdbuf->logs()->debugDescription()->cString(NS::ASCIIStringEncoding));
      }
    }

    chunk.reset();
    cpu_coherent.fetch_add(1, std::memory_order_relaxed);
    chunk_ongoing.fetch_sub(1, std::memory_order_release);
    cpu_coherent.notify_all();
    chunk_ongoing.notify_one();

    staging_allocator.free_blocks(internal_seq);
    copy_temp_allocator.free_blocks(internal_seq);

    internal_seq++;
  }
  TRACE("finishing thread gracefully terminates");
  return 0;
}
} // namespace dxmt
