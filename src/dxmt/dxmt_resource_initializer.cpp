#include "Metal.hpp"
#include "dxmt_resource_initializer.hpp"
#include "dxmt_format.hpp"
#include "util_math.hpp"
#include <cstdint>
#include <mutex>

namespace dxmt {

#define ALLOC_BLIT(type, cmd)                                                                                          \
  type *cmd = nullptr;                                                                                                 \
  if (!allocateBlit(&cmd)) {                                                                                           \
    flushInternal();                                                                                                   \
    continue;                                                                                                          \
  }

#define ALLOC_CLEAR(info)                                                                                              \
  WMTRenderPassInfo *info;                                                                                             \
  if (!allocateClear(&info)) {                                                                                         \
    flushInternal();                                                                                                   \
    continue;                                                                                                          \
  }

#define RETAIN(allocation)                                                                                             \
  if (!retainAllocation(allocation)) {                                                                                 \
    flushInternal();                                                                                                   \
    continue;                                                                                                          \
  }

ResourceInitializer::ResourceInitializer(WMT::Device device) : device_(device) {
  upload_queue_ = device.newCommandQueue(kResourceInitializerChunks);
  upload_queue_event_ = device.newSharedEvent();

  cpu_command_heap_size = kResourceInitializerCpuCommandHeapSize;
  cpu_command_heap = malloc(cpu_command_heap_size);
  reset();
}

ResourceInitializer::~ResourceInitializer() {
  free(cpu_command_heap);
}

uint64_t
ResourceInitializer::initWithZero(BufferAllocation *buffer, uint64_t offset, uint64_t length) {
  std::lock_guard<dxmt::mutex> lock(mutex_);

  do {
    RETAIN(buffer);
    ALLOC_BLIT(wmtcmd_blit_fillbuffer, fill);
    fill->type = WMTBlitCommandFillBuffer;
    fill->buffer = buffer->buffer();
    fill->offset = offset;
    fill->length = length;
    fill->value = 0;

  } while (0);

  return current_seq_id_;
}

uint64_t
ResourceInitializer::initWithDefault(const Texture *texture, TextureAllocation *allocation) {
  std::lock_guard<dxmt::mutex> lock(mutex_);

  do {
    if (auto dsv_planar = DepthStencilPlanarFlags(texture->pixelFormat())) {
      RETAIN(allocation);
      ALLOC_CLEAR(info);
      info->render_target_array_length = texture->arrayLength();
      info->render_target_width = texture->width();
      info->render_target_height = texture->height();
      if (dsv_planar & 1) {
        info->depth.texture = allocation->texture();
        info->depth.clear_depth = 0;
        info->depth.load_action = WMTLoadActionClear;
        info->depth.store_action = WMTStoreActionStore;
      }
      if (dsv_planar & 2) {
        info->stencil.texture = allocation->texture();
        info->stencil.clear_stencil = 0;
        info->stencil.load_action = WMTLoadActionClear;
        info->stencil.store_action = WMTStoreActionStore;
      }

    } else if (texture->usage() & WMTTextureUsageRenderTarget) {
      RETAIN(allocation);
      ALLOC_CLEAR(info);
      info->render_target_array_length =
          texture->textureType() == WMTTextureType3D ? texture->depth() : texture->arrayLength();
      info->render_target_width = texture->width();
      info->render_target_height = texture->height();
      info->colors[0].texture = allocation->texture();
      info->colors[0].load_action = WMTLoadActionClear;
      info->colors[0].clear_color = {0, 0, 0, 1.0};
      info->colors[0].store_action = WMTStoreActionStore;
    }
    // NOTE: otherwise the initial data is undefined
    // "If you don't pass anything to pInitialData, the initial content of the memory for the resource is undefined"

    break;
  } while (0);

  return current_seq_id_;
}

std::uint64_t
ResourceInitializer::flushInternal() {
  auto pool = WMT::MakeAutoreleasePool();

  auto seq_id = current_seq_id_++;
  auto cmdbuf = upload_queue_.commandBuffer();
  encode(cmdbuf);
  cmdbuf.encodeSignalEvent(upload_queue_event_, seq_id);
  cmdbuf.commit();
  reset();
  return seq_id;
}

uint64_t
ResourceInitializer::flushToWait() {
  std::lock_guard<dxmt::mutex> lock(mutex_);

  if (idle()) {
    if (cached_coherent_seq_id == current_seq_id_ - 1)
      return 0;
    cached_coherent_seq_id = upload_queue_event_.signaledValue();
    if (cached_coherent_seq_id == current_seq_id_ - 1)
      return 0;
    return current_seq_id_ - 1;
  }

  return flushInternal();
}

void
ResourceInitializer::reset() {
  cpu_command_heap_offset = 0;

  clear_render_pass_head.next = nullptr;
  clear_render_pass_tail = &clear_render_pass_head;

  blit_cmd_head.type = WMTBlitCommandNop;
  blit_cmd_head.next.set(nullptr);
  blit_cmd_tail = (wmtcmd_base *)&blit_cmd_head;

  ref_tracker.clear();
}

void
ResourceInitializer::encode(WMT::CommandBuffer cmdbuf) {

  auto clear_pass = clear_render_pass_head.next;
  while (clear_pass) {
    auto r = cmdbuf.renderCommandEncoder(clear_pass->info);
    r.endEncoding();
    clear_pass = clear_pass->next;
  }

  if (blit_cmd_head.next.ptr) {
    auto b = cmdbuf.blitCommandEncoder();
    b.encodeCommands(&blit_cmd_head);
    b.endEncoding();
  }

}

bool
ResourceInitializer::retainAllocation(Allocation *allocation) {
  constexpr size_t block_size = 0x20;
  while (unlikely(!ref_tracker.track(allocation))) {
    auto temp = allocateCpuHeap<intptr_t[block_size]>();
    if (!temp)
      return false;
    ref_tracker.addStorage(temp, block_size * sizeof(intptr_t));
  }
  return true;
}

} // namespace dxmt