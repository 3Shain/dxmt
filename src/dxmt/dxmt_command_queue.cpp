#include "dxmt_command_queue.hpp"
#include "Metal.hpp"
#include "Metal/MTLCaptureManager.hpp"
#include "dxmt_statistics.hpp"
#include "util_env.hpp"
#include <atomic>

#define ASYNC_ENCODING 1

namespace dxmt {

CommandQueue::CommandQueue(WMT::Device device) :
    encodeThread([this]() { this->EncodingThread(); }),
    finishThread([this]() { this->WaitForFinishThread(); }),
    device(device),
    commandQueue(device.newCommandQueue(kCommandChunkCount)),
    staging_allocator(
        (MTL::Device *)device.handle, MTL::ResourceOptionCPUCacheModeWriteCombined | MTL::ResourceHazardTrackingModeUntracked |
                    MTL::ResourceStorageModeShared
    ),
    copy_temp_allocator((MTL::Device *)device.handle, MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceStorageModePrivate),
    command_data_allocator(
      (MTL::Device *)device.handle, MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceCPUCacheModeWriteCombined |
                    MTL::ResourceStorageModeShared
    ),
    emulated_cmd((MTL::Device *)device.handle),
    argument_encoding_ctx(*this, (MTL::Device *)device.handle) {
  for (unsigned i = 0; i < kCommandChunkCount; i++) {
    auto &chunk = chunks[i];
    chunk.queue = this;
    chunk.cpu_argument_heap = (char *)malloc(kCommandChunkCPUHeapSize);
    chunk.reset();
  };
  event = transfer(((MTL::Device *)device.handle)->newSharedEvent());

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
  };
  TRACE("Destructed command queue");
}

void
CommandQueue::CommitCurrentChunk() {
  auto chunk_id = ready_for_encode.load(std::memory_order_relaxed);
  auto &chunk = chunks[chunk_id % kCommandChunkCount];
  chunk.chunk_id = chunk_id;
  chunk.chunk_event_id = GetNextEventSeqId();
  chunk.frame_ = frame_count;
  auto& statistics = CurrentFrameStatistics();
  statistics.command_buffer_count++;
#if ASYNC_ENCODING
  ready_for_encode.fetch_add(1, std::memory_order_release);
  ready_for_encode.notify_one();

  auto t0 = clock::now();
  chunk_ongoing.wait(kCommandChunkCount - 1, std::memory_order_acquire);
  chunk_ongoing.fetch_add(1, std::memory_order_relaxed);
  auto t1 = clock::now();
  statistics.commit_interval += (t1 - t0);

#else
  CommitChunkInternal(chunk, ready_for_encode.fetch_add(1, std::memory_order_relaxed));
#endif
}

void
CommandQueue::CommitChunkInternal(CommandChunk &chunk, uint64_t seq) {

  auto pool = WMT::MakeAutoreleasePool();

  switch (capture_state.getNextAction(chunk.frame_)) {
  case CaptureState::NextAction::StartCapture: {
    auto capture_mgr = MTL::CaptureManager::sharedCaptureManager();
    auto capture_desc = transfer(MTL::CaptureDescriptor::alloc()->init());
    capture_desc->setCaptureObject((MTL::Device *)device.handle);
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

  auto cmdbuf = commandQueue.commandBuffer();
  chunk.attached_cmdbuf = cmdbuf;
  chunk.encode(chunk.attached_cmdbuf, this->argument_encoding_ctx);
  cmdbuf.commit();

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
    if (chunk.attached_cmdbuf.status() <= WMTCommandBufferStatusScheduled) {
      chunk.attached_cmdbuf.waitUntilCompleted();
    }
    // FIXME: make below functional
    // if (chunk.attached_cmdbuf.status() == WMTCommandBufferStatusError) {
    //   ERR("Device error at frame ", chunk.frame_,
    //       ", : ", chunk.attached_cmdbuf->error()->localizedDescription()->cString(NS::ASCIIStringEncoding));
    // }
    // if (chunk.attached_cmdbuf->logs()) {
    //   if (((NS::Array *)chunk.attached_cmdbuf->logs())->count()) {
    //     ERR("logs at frame ", chunk.frame_);
    //     ERR(chunk.attached_cmdbuf->logs()->debugDescription()->cString(NS::ASCIIStringEncoding));
    //   }
    // }

    if (chunk.signal_frame_latency_fence_ != ~0ull)
      frame_latency_fence_.signal(chunk.signal_frame_latency_fence_);

    chunk.reset();
    cpu_coherent.signal(internal_seq);
    chunk_ongoing.fetch_sub(1, std::memory_order_release);
    chunk_ongoing.notify_one();

    staging_allocator.free_blocks(internal_seq);
    copy_temp_allocator.free_blocks(internal_seq);
    command_data_allocator.free_blocks(internal_seq);

    internal_seq++;
  }
  TRACE("finishing thread gracefully terminates");
  return 0;
}
} // namespace dxmt
