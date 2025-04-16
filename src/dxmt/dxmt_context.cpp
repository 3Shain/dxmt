#include "dxmt_context.hpp"
#include "Metal.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLResource.hpp"
#include "dxmt_command_queue.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "dxmt_command_list.hpp"
#include "dxmt_occlusion_query.hpp"
#include <cstdint>

namespace dxmt {

ArgumentEncodingContext::ArgumentEncodingContext(CommandQueue &queue, MTL::Device *device) : queue_(queue) {
  Obj<MTL::SamplerDescriptor> descriptor = transfer(MTL::SamplerDescriptor::alloc()->init());
  descriptor->setSupportArgumentBuffers(true);
  dummy_sampler_ = transfer(device->newSamplerState(descriptor));
  dummy_cbuffer_ = transfer(device->newBuffer(
      65536, MTL::ResourceOptionCPUCacheModeWriteCombined | MTL::ResourceStorageModeShared |
                 MTL::ResourceHazardTrackingModeUntracked
  ));
  std::memset(dummy_cbuffer_->contents(), 0, 65536);
  cpu_buffer_ = malloc(kCommandChunkCPUHeapSize);
};

ArgumentEncodingContext::~ArgumentEncodingContext() {
  free(cpu_buffer_);
};

template void ArgumentEncodingContext::encodeVertexBuffers<PipelineKind::Ordinary>(uint32_t slot_mask);
template void ArgumentEncodingContext::encodeVertexBuffers<PipelineKind::Tessellation>(uint32_t slot_mask);
template void ArgumentEncodingContext::encodeVertexBuffers<PipelineKind::Geometry>(uint32_t slot_mask);

template <PipelineKind kind>
void
ArgumentEncodingContext::encodeVertexBuffers(uint32_t slot_mask) {
  struct VERTEX_BUFFER_ENTRY {
    uint64_t buffer_handle;
    uint32_t stride;
    uint32_t length;
  };
  uint32_t max_slot = 32 - __builtin_clz(slot_mask);
  uint32_t num_slots = __builtin_popcount(slot_mask);

  uint64_t offset = allocate_gpu_heap(16 * num_slots, 16);
  VERTEX_BUFFER_ENTRY *entries = (VERTEX_BUFFER_ENTRY *)(((char *)gpu_buffer_->contents()) + offset);

  for (unsigned slot = 0, index = 0; slot < max_slot; slot++) {
    if (!(slot_mask & (1 << slot)))
      continue;
    auto &state = vbuf_[slot];
    auto &buffer = state.buffer;
    if (!buffer.ptr()) {
      entries[index].buffer_handle = 0;
      entries[index].stride = 0;
      entries[index++].length = 0;
      continue;
    }
    auto current = buffer->current();
    auto length = buffer->length();
    entries[index].buffer_handle = current->gpuAddress + state.offset;
    entries[index].stride = state.stride;
    entries[index++].length = length > state.offset ? length - state.offset : 0;
    // FIXME: did we intended to use the whole buffer?
    access(buffer, DXMT_ENCODER_RESOURCE_ACESS_READ);
    makeResident<PipelineStage::Vertex, kind>(buffer.ptr());
  };
  if constexpr (kind == PipelineKind::Tessellation)
    encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 16); });
  else if constexpr (kind == PipelineKind::Geometry)
    encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 16); });
  else
    encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setVertexBufferOffset(offset, 16); });
}

template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Vertex, PipelineKind::Ordinary>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Pixel, PipelineKind::Ordinary>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Vertex, PipelineKind::Tessellation>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Pixel, PipelineKind::Tessellation>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Hull, PipelineKind::Tessellation>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Domain, PipelineKind::Tessellation>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Compute, PipelineKind::Ordinary>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Vertex, PipelineKind::Geometry>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Geometry, PipelineKind::Geometry>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Pixel, PipelineKind::Geometry>(const MTL_SHADER_REFLECTION *reflection);

template <PipelineStage stage, PipelineKind kind>
void
ArgumentEncodingContext::encodeConstantBuffers(const MTL_SHADER_REFLECTION *reflection) {
  auto ConstantBufferCount = reflection->NumConstantBuffers;
  uint64_t offset = allocate_gpu_heap(ConstantBufferCount << 3, 16);
  uint64_t *encoded_buffer = reinterpret_cast<uint64_t *>((char *)gpu_buffer_->contents() + offset);

  for (unsigned i = 0; i < reflection->NumConstantBuffers; i++) {
    auto &arg = reflection->ConstantBuffers[i];
    auto slot = 14 * unsigned(stage) + arg.SM50BindingSlot;
    switch (arg.Type) {
    case SM50BindingType::ConstantBuffer: {
      auto &cbuf = cbuf_[slot];
      if (!cbuf.buffer.ptr()) {
        encoded_buffer[arg.StructurePtrOffset] = dummy_cbuffer_->gpuAddress();
        makeResident<stage, kind>(dummy_cbuffer_, GetResidencyMask<kind>(stage, true, false));
        continue;
      }
      auto argbuf = cbuf.buffer;
      encoded_buffer[arg.StructurePtrOffset] = argbuf->current()->gpuAddress + (cbuf.offset);
      // FIXME: did we intended to use the whole buffer?
      access(argbuf, DXMT_ENCODER_RESOURCE_ACESS_READ);
      makeResident<stage, kind>(argbuf.ptr());
      break;
    }
    default:
      DXMT_UNREACHABLE
    }
  }

  /* kConstantBufferTableBinding = 29 */
  if constexpr (stage == PipelineStage::Compute) {
    auto &cmd = encodeComputeCommand<wmtcmd_compute_setbufferoffset>();
    cmd.type = WMTComputeCommandSetBufferOffset;
    cmd.offset = offset;
    cmd.index = 29;
  } else if constexpr (stage == PipelineStage::Hull) {
    encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setMeshBufferOffset(offset, 29); });
  } else if constexpr (stage == PipelineStage::Vertex) {
    if constexpr (kind == PipelineKind::Tessellation)
      encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 29); });
    else if constexpr (kind == PipelineKind::Geometry)
      encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 29); });
    else
      encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setVertexBufferOffset(offset, 29); });
  } else {
    encodeRenderCommand([offset](RenderCommandContext &ctx) {
      if constexpr (stage == PipelineStage::Pixel) {
        ctx.encoder->setFragmentBufferOffset(offset, 29);
      } else if constexpr (stage == PipelineStage::Domain) {
        ctx.encoder->setVertexBufferOffset(offset, 29);
      } else if constexpr (stage == PipelineStage::Geometry) {
        ctx.encoder->setMeshBufferOffset(offset, 29);
      } else {
        assert(0 && "Not implemented or unreachable");
      }
    });
  }
};

template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Vertex, PipelineKind::Ordinary>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Pixel, PipelineKind::Ordinary>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Vertex, PipelineKind::Tessellation>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Pixel, PipelineKind::Tessellation>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Hull, PipelineKind::Tessellation>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Domain, PipelineKind::Tessellation>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Compute, PipelineKind::Ordinary>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Vertex, PipelineKind::Geometry>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Geometry, PipelineKind::Geometry>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Pixel, PipelineKind::Geometry>(const MTL_SHADER_REFLECTION *reflection);

template <PipelineStage stage, PipelineKind kind>
void
ArgumentEncodingContext::encodeShaderResources(const MTL_SHADER_REFLECTION *reflection) {
  auto BindingCount = reflection->NumArguments;
  auto ArgumentTableQwords = reflection->ArgumentTableQwords;

  auto offset = allocate_gpu_heap(ArgumentTableQwords * 8, 16);
  uint64_t *encoded_buffer = reinterpret_cast<uint64_t *>((char *)gpu_buffer_->contents() + offset);

  auto &UAVBindingSet = stage == PipelineStage::Compute ? cs_uav_ : om_uav_;

  for (unsigned i = 0; i < BindingCount; i++) {
    auto &arg = reflection->Arguments[i];
    switch (arg.Type) {
    case SM50BindingType::ConstantBuffer: {
      DXMT_UNREACHABLE
    }
    case SM50BindingType::Sampler: {
      auto slot = 16 * unsigned(stage) + arg.SM50BindingSlot;
      if (!sampler_[slot].sampler) {
        encoded_buffer[arg.StructurePtrOffset] = dummy_sampler_->gpuResourceID()._impl;
        encoded_buffer[arg.StructurePtrOffset + 1] = (uint64_t)std::bit_cast<uint32_t>(0.0f);
        break;
      }
      encoded_buffer[arg.StructurePtrOffset] = sampler_[slot].sampler->gpuResourceID()._impl;
      encoded_buffer[arg.StructurePtrOffset + 1] = (uint64_t)std::bit_cast<uint32_t>(sampler_[slot].bias);
      break;
    }
    case SM50BindingType::SRV: {
      auto slot = 128 * unsigned(stage) + arg.SM50BindingSlot;
      auto &srv = resview_[slot];

      if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
        if (srv.buffer.ptr()) {
          encoded_buffer[arg.StructurePtrOffset] = srv.buffer->current()->gpuAddress + srv.slice.byteOffset;
          encoded_buffer[arg.StructurePtrOffset + 1] = srv.slice.byteLength;
          access(srv.buffer, srv.slice.byteOffset, srv.slice.byteLength, DXMT_ENCODER_RESOURCE_ACESS_READ);
          makeResident<stage, kind>(srv.buffer.ptr());
        } else {
          encoded_buffer[arg.StructurePtrOffset] = 0;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
        }
      } else if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
        if (srv.buffer.ptr()) {
          assert(arg.Flags & MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET);
          encoded_buffer[arg.StructurePtrOffset] =
              access(srv.buffer, srv.viewId, DXMT_ENCODER_RESOURCE_ACESS_READ)->gpuResourceID()._impl;
          encoded_buffer[arg.StructurePtrOffset + 1] =
              ((uint64_t)srv.slice.elementCount << 32) | (uint64_t)srv.slice.firstElement;
          makeResident<stage, kind>(srv.buffer.ptr(), srv.viewId);
        } else if (srv.texture.ptr()) {
          assert(arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP);
          auto viewIdChecked = srv.texture->checkViewUseArray(srv.viewId, arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_ARRAY);
          encoded_buffer[arg.StructurePtrOffset] =
              access(srv.texture, viewIdChecked, DXMT_ENCODER_RESOURCE_ACESS_READ)->gpuResourceID()._impl;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
          makeResident<stage, kind>(srv.texture.ptr(), viewIdChecked);
        } else {
          encoded_buffer[arg.StructurePtrOffset] = 0;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
        }
      }
      if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
        assert(0 && "srv can not have counter associated");
      }
      break;
    }
    case SM50BindingType::UAV: {
      auto &uav = UAVBindingSet[arg.SM50BindingSlot];
      bool read = (arg.Flags >> 10) & 1, write = (arg.Flags >> 10) & 2;
      auto access_flags =  (DXMT_ENCODER_RESOURCE_ACESS)((arg.Flags >> 10) & 3);

      if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
        if (uav.buffer.ptr()) {
          encoded_buffer[arg.StructurePtrOffset] = uav.buffer->current()->gpuAddress + uav.slice.byteOffset;
          encoded_buffer[arg.StructurePtrOffset + 1] = uav.slice.byteLength;
          access(uav.buffer, uav.slice.byteOffset, uav.slice.byteLength, access_flags);
          makeResident<stage, kind>(uav.buffer.ptr(), read, write);
        } else {
          encoded_buffer[arg.StructurePtrOffset] = 0;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
        }
      } else if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
        if (uav.buffer.ptr()) {
          assert(arg.Flags & MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET);
          encoded_buffer[arg.StructurePtrOffset] = access(uav.buffer, uav.viewId, access_flags)->gpuResourceID()._impl;
          encoded_buffer[arg.StructurePtrOffset + 1] =
              ((uint64_t)uav.slice.elementCount << 32) | (uint64_t)uav.slice.firstElement;
          makeResident<stage, kind>(uav.buffer.ptr(), uav.viewId, read, write);
        } else if (uav.texture.ptr()) {
          assert(arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP);
          auto viewIdChecked = uav.texture->checkViewUseArray(uav.viewId, arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_ARRAY);
          encoded_buffer[arg.StructurePtrOffset] = access(uav.texture, viewIdChecked, access_flags)->gpuResourceID()._impl;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
          makeResident<stage, kind>(uav.texture.ptr(), viewIdChecked, read, write);
        } else {
          encoded_buffer[arg.StructurePtrOffset] = 0;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
        }
      }
      if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
        if (uav.counter) {
          encoded_buffer[arg.StructurePtrOffset + 2] = uav.counter->current()->gpuAddress;
          access(uav.counter, 0, 4, DXMT_ENCODER_RESOURCE_ACESS_READ | DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          makeResident<stage, kind>(uav.counter.ptr(), true, true);
        } else {
          /*
           * potentially cause gpu pagefault, even providing a dummy buffer doesn't improve since the returned
           * counter value is likely to be used as an index to another read/write operation.
           */
          encoded_buffer[arg.StructurePtrOffset + 2] = 0;
        }
      }
      break;
    }
    }
  }

  if constexpr (stage == PipelineStage::Compute) {
    auto &cmd = encodeComputeCommand<wmtcmd_compute_setbufferoffset>();
    cmd.type = WMTComputeCommandSetBufferOffset;
    cmd.offset = offset;
    cmd.index = 30;
  } else if constexpr (stage == PipelineStage::Hull) {
    encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setMeshBufferOffset(offset, 30); });
  } else if constexpr (stage == PipelineStage::Vertex) {
    if constexpr (kind == PipelineKind::Tessellation)
      encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 30); });
    else if constexpr (kind == PipelineKind::Geometry)
      encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 30); });
    else
      encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setVertexBufferOffset(offset, 30); });
  } else {
    encodeRenderCommand([offset](RenderCommandContext &ctx) {
      if constexpr (stage == PipelineStage::Pixel) {
        ctx.encoder->setFragmentBufferOffset(offset, 30);
      } else if constexpr (stage == PipelineStage::Domain) {
        ctx.encoder->setVertexBufferOffset(offset, 30);
      } else if constexpr (stage == PipelineStage::Geometry) {
        ctx.encoder->setMeshBufferOffset(offset, 30);
      } else {
        assert(0 && "Not implemented or unreachable");
      }
    });
  }
}

void
ArgumentEncodingContext::clearColor(Rc<Texture> &&texture, unsigned viewId, unsigned arrayLength, WMTClearColor color) {
  assert(!encoder_current);
  auto encoder_info = allocate<ClearEncoderData>();
  encoder_info->type = EncoderType::Clear;
  encoder_info->id = nextEncoderId();
  encoder_info->clear_dsv = 0;
  encoder_info->texture = WMT::Texture{(obj_handle_t)texture->view(viewId)};
  encoder_info->color = color;
  encoder_info->array_length = arrayLength;
  encoder_info->width = texture->width();
  encoder_info->height = texture->height();

  encoder_info->tex_write.add(texture->current()->depkey);

  encoder_current = encoder_info;

  currentFrameStatistics().clear_pass_count++;

  endPass();
}

void
ArgumentEncodingContext::clearDepthStencil(
    Rc<Texture> &&texture, unsigned viewId, unsigned arrayLength, unsigned flag, float depth, uint8_t stencil
) {
  assert(!encoder_current);
  auto encoder_info = allocate<ClearEncoderData>();
  encoder_info->type = EncoderType::Clear;
  encoder_info->id = nextEncoderId();
  encoder_info->clear_dsv = flag & DepthStencilPlanarFlags(texture->pixelFormat());
  encoder_info->texture = WMT::Texture{(obj_handle_t)texture->view(viewId)};
  encoder_info->depth_stencil = {depth, stencil};
  encoder_info->array_length = arrayLength;
  encoder_info->width = texture->width();
  encoder_info->height = texture->height();

  encoder_info->tex_write.add(texture->current()->depkey);

  encoder_current = encoder_info;

  currentFrameStatistics().clear_pass_count++;
  
  endPass();
}

void
ArgumentEncodingContext::resolveTexture(
    Rc<Texture> &&src, unsigned srcSlice, Rc<Texture> &&dst, unsigned dstSlice, unsigned dstLevel
) {
  assert(!encoder_current);
  auto encoder_info = allocate<ResolveEncoderData>();
  encoder_info->type = EncoderType::Resolve;
  // FIXME: resolve by specific format view
  encoder_info->src = WMT::Texture{(obj_handle_t)src->current()->texture()};
  encoder_info->dst = WMT::Texture{(obj_handle_t)dst->current()->texture()};
  encoder_info->src_slice = srcSlice;
  encoder_info->dst_slice = dstSlice;
  encoder_info->dst_level = dstLevel;

  encoder_info->tex_read.add(src->current()->depkey);
  encoder_info->tex_write.add(dst->current()->depkey);

  encoder_current = encoder_info;
  endPass();
};

void
ArgumentEncodingContext::present(Rc<Texture> &texture, CA::MetalLayer *layer, double after) {
  assert(!encoder_current);
  auto encoder_info = allocate<PresentData>();
  encoder_info->type = EncoderType::Present;
  encoder_info->id = nextEncoderId();
  encoder_info->backbuffer = texture->current()->texture();
  encoder_info->layer = layer;
  encoder_info->after = after;

  encoder_info->tex_read.add(texture->current()->depkey);

  encoder_current = encoder_info;
  endPass();
}

void
ArgumentEncodingContext::upscale(Rc<Texture> &texture, Rc<Texture> &upscaled, Obj<MTLFX::SpatialScaler> &scaler) {
  assert(!encoder_current);
  auto encoder_info = allocate<SpatialUpscaleData>();
  encoder_info->type = EncoderType::SpatialUpscale;
  encoder_info->id = nextEncoderId();
  encoder_info->backbuffer = texture->current()->texture();
  encoder_info->upscaled = upscaled->current()->texture();
  encoder_info->scaler = scaler;

  encoder_info->tex_read.add(texture->current()->depkey);
  encoder_info->tex_write.add(upscaled->current()->depkey);

  encoder_current = encoder_info;
  endPass();
}

void
ArgumentEncodingContext::upscaleTemporal(
    Rc<Texture> &input, Rc<Texture> &output, Rc<Texture> &depth, Rc<Texture> &motion_vector, TextureViewKey mvViewId,
    Rc<Texture> &exposure, Obj<MTLFX::TemporalScaler> &scaler, const TemporalScalerProps &props
) {
  assert(!encoder_current);
  auto encoder_info = allocate<TemporalUpscaleData>();
  encoder_info->type = EncoderType::TemporalUpscale;
  encoder_info->id = nextEncoderId();
  encoder_info->input = input->current()->texture();
  encoder_info->output = output->current()->texture();
  encoder_info->depth = depth->current()->texture();
  encoder_info->motion_vector = motion_vector->view(mvViewId);
  encoder_info->scaler = scaler;
  encoder_info->props = props;

  encoder_info->tex_read.add(input->current()->depkey);
  encoder_info->tex_read.add(depth->current()->depkey);
  encoder_info->tex_read.add(motion_vector->current()->depkey);
  encoder_info->tex_write.add(output->current()->depkey);
  if(exposure) {
    encoder_info->exposure = exposure->current()->texture();
    encoder_info->tex_read.add(exposure->current()->depkey);
  }

  encoder_current = encoder_info;
  endPass();
}

void
ArgumentEncodingContext::signalEvent(uint64_t value) {
  assert(!encoder_current);
  auto encoder_info = allocate<SignalEventData>();
  encoder_info->type = EncoderType::SignalEvent;
  encoder_info->id = nextEncoderId();
  encoder_info->event = queue_.event;
  encoder_info->value = value;

  encoder_current = encoder_info;
  endPass();
}

RenderEncoderData *
ArgumentEncodingContext::startRenderPass(
    uint8_t dsv_planar_flags, uint8_t dsv_readonly_flags, uint8_t render_target_count
) {
  assert(!encoder_current);
  auto encoder_info = allocate<RenderEncoderData>();
  encoder_info->type = EncoderType::Render;
  encoder_info->id = nextEncoderId();
  WMT::InitializeRenderPassInfo(encoder_info->info);
  encoder_info->dsv_planar_flags = dsv_planar_flags;
  encoder_info->dsv_readonly_flags = dsv_readonly_flags;
  encoder_info->render_target_count = render_target_count;
  encoder_current = encoder_info;

  currentFrameStatistics().render_pass_count++;

  vro_state_.beginEncoder();

  return encoder_info;
}

EncoderData *
ArgumentEncodingContext::startComputePass() {
  assert(!encoder_current);
  auto encoder_info = allocate<ComputeEncoderData>();
  encoder_info->type = EncoderType::Compute;
  encoder_info->id = nextEncoderId();
  encoder_info->cmd_head.type = WMTComputeCommandNop;
  encoder_info->cmd_head.next.set(0);
  encoder_info->cmd_tail = (wmtcmd_base *)&encoder_info->cmd_head;
  encoder_current = encoder_info;

  currentFrameStatistics().compute_pass_count++;

  return encoder_info;
}

EncoderData *
ArgumentEncodingContext::startBlitPass() {
  assert(!encoder_current);
  auto encoder_info = allocate<BlitEncoderData>();
  encoder_info->type = EncoderType::Blit;
  encoder_info->id = nextEncoderId();
  encoder_info->cmd_head.type = WMTBlitCommandNop;
  encoder_info->cmd_head.next.set(0);
  encoder_info->cmd_tail = (wmtcmd_base *)&encoder_info->cmd_head;
  encoder_current = encoder_info;

  currentFrameStatistics().blit_pass_count++;

  return encoder_info;
}

void
ArgumentEncodingContext::endPass() {
  assert(encoder_current);
  encoder_last->next = encoder_current;
  encoder_last = encoder_current;

  if (encoder_current->type == EncoderType::Render)
    vro_state_.endEncoder();

  encoder_current = nullptr;
  encoder_count_++;
}

std::pair<MTL::Buffer *, size_t>
ArgumentEncodingContext::allocateTempBuffer(size_t size, size_t alignment) {
  auto [_, buffer, offset] = queue_.AllocateTempBuffer(seq_id_, size, alignment);
  return {buffer, offset};
};

void
ArgumentEncodingContext::beginVisibilityResultQuery(Rc<VisibilityResultQuery> &&query) {
  query->begin(seq_id_, vro_state_.getNextReadOffset());
  active_visibility_query_count_++;
  pending_queries_.push_back(std::move(query));
}
void
ArgumentEncodingContext::endVisibilityResultQuery(Rc<VisibilityResultQuery> &&query) {
  query->end(seq_id_, vro_state_.getNextReadOffset());
  assert(active_visibility_query_count_);
  active_visibility_query_count_--;
}
void
ArgumentEncodingContext::bumpVisibilityResultOffset() {
  auto render_encoder = currentRenderEncoder();
  render_encoder->use_visibility_result = render_encoder->use_visibility_result || bool(active_visibility_query_count_);

  uint64_t offset;
  if (vro_state_.tryGetNextWriteOffset(active_visibility_query_count_, offset)) {
    encodeRenderCommand([offset](RenderCommandContext &ctx) {
      if (~offset == 0)
        ctx.encoder->setVisibilityResultMode(MTL::VisibilityResultModeDisabled, 0);
      else
        ctx.encoder->setVisibilityResultMode(MTL::VisibilityResultModeCounting, offset << 3);
    });
  }
}

FrameStatistics&
ArgumentEncodingContext::currentFrameStatistics() {
  return queue_.statistics.at(frame_id_);
}

void
ArgumentEncodingContext::$$setEncodingContext(uint64_t seq_id, uint64_t frame_id) {
  cpu_buffer_offset_ = 0;
  seq_id_ = seq_id;
  frame_id_ = frame_id;
  auto [_, gpu_buffer, offset] = queue_.AllocateCommandDataBuffer(seq_id);
  gpu_buffer_ = gpu_buffer;
  gpu_bufer_offset_ = offset;
}

constexpr unsigned kEncoderOptimizerThreshold = 64;

std::unique_ptr<VisibilityResultReadback>
ArgumentEncodingContext::flushCommands(MTL::CommandBuffer *cmdbuf, uint64_t seqId, uint64_t event_seq_id) {
  assert(!encoder_current);

  WMT::CommandBuffer cmdbuf_{(obj_handle_t)cmdbuf};

  unsigned encoder_count = encoder_count_;
  unsigned encoder_index = 0;
  EncoderData **encoders =
      reinterpret_cast<EncoderData **>(allocate_cpu_heap(sizeof(EncoderData *) * encoder_count, alignof(EncoderData *))
      );

  {
    EncoderData *current = encoder_head.next;
    while (current) {
      encoders[encoder_index++] = current;
      current = current->next;
    }
    assert(encoder_index == encoder_count);
  }

  if (encoder_count > 1) {
    unsigned j, i;
    for (j = encoder_count - 2; j != ~0u; j--) {
      if (encoders[j]->type == EncoderType::Null)
        continue;
      for (i = j + 1; i < std::min(encoder_count, j + kEncoderOptimizerThreshold); i++) {
        if (encoders[i]->type == EncoderType::Null)
          continue;
        if (checkEncoderRelation(encoders[j], encoders[i]) == DXMT_ENCODER_LIST_OP_SYNCHRONIZE)
          break;
      }
    }
  }

  std::unique_ptr<VisibilityResultReadback> visibility_readback {};

  if (auto count = vro_state_.reset()) {
    visibility_readback = std::make_unique<VisibilityResultReadback>(
        seqId, count, pending_queries_,
        transfer(cmdbuf->device()->newBuffer(
            count * sizeof(uint64_t), MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceStorageModeShared
        ))
    );
  }
  std::erase_if(pending_queries_, [=](auto &query) -> bool { return query->queryEndAt() == seqId; });

  while (encoder_index) {
    auto current = encoders[encoder_count - encoder_index];
    switch (current->type) {
    case EncoderType::Render: {
      auto data = static_cast<RenderEncoderData *>(current);
      if (data->use_visibility_result) {
        assert(visibility_readback);
        data->info.visibility_buffer = (obj_handle_t)visibility_readback->visibility_result_heap.ptr();
      }
      auto encoder = cmdbuf_.renderCommandEncoder(data->info);
      RenderCommandContext ctx{(MTL::RenderCommandEncoder *)encoder.handle, data->dsv_planar_flags, gpu_buffer_};
      ctx.encoder->setVertexBuffer(gpu_buffer_, 0, 16);
      ctx.encoder->setVertexBuffer(gpu_buffer_, 0, 29);
      ctx.encoder->setVertexBuffer(gpu_buffer_, 0, 30);
      ctx.encoder->setFragmentBuffer(gpu_buffer_, 0, 29);
      ctx.encoder->setFragmentBuffer(gpu_buffer_, 0, 30);
      if (data->use_tessellation) {
        ctx.encoder->setObjectBuffer(gpu_buffer_, 0, 16);
        ctx.encoder->setObjectBuffer(gpu_buffer_, 0, 21); // draw arguments
        ctx.encoder->setObjectBuffer(gpu_buffer_, 0, 29);
        ctx.encoder->setObjectBuffer(gpu_buffer_, 0, 30);
        ctx.encoder->setMeshBuffer(gpu_buffer_, 0, 29);
        ctx.encoder->setMeshBuffer(gpu_buffer_, 0, 30);
        ctx.encoder->setVertexBuffer(gpu_buffer_, 0, 23); // draw arguments
        data->pretess_cmds.execute(ctx);
        ctx.encoder->memoryBarrier(MTL::BarrierScopeBuffers, MTL::RenderStageMesh, MTL::RenderStageVertex);
      }
      if (data->use_geometry && !data->use_tessellation) {
        ctx.encoder->setObjectBuffer(gpu_buffer_, 0, 16);
        ctx.encoder->setObjectBuffer(gpu_buffer_, 0, 21); // draw arguments
        ctx.encoder->setObjectBuffer(gpu_buffer_, 0, 29);
        ctx.encoder->setObjectBuffer(gpu_buffer_, 0, 30);
        ctx.encoder->setMeshBuffer(gpu_buffer_, 0, 29);
        ctx.encoder->setMeshBuffer(gpu_buffer_, 0, 30);
      }
      if (data->gs_arg_marshal_tasks.size()) {
        auto task_count = data->gs_arg_marshal_tasks.size();
        struct GS_MARSHAL_TASK {
          uint64_t draw_args;
          uint64_t dispatch_args_out;
          uint32_t vertex_count_per_warp;
          uint32_t end_of_command;
        };
        auto offset = allocate_gpu_heap(sizeof(GS_MARSHAL_TASK) * task_count, 8);
        auto tasks_data = (GS_MARSHAL_TASK *)((char*)gpu_buffer_->contents() + offset);
        for (unsigned i = 0; i<task_count; i++) {
          auto & task = data->gs_arg_marshal_tasks[i];
          tasks_data[i].draw_args = task.draw_arguments->gpuAddress() + task.draw_arguments_offset;
          tasks_data[i].dispatch_args_out = gpu_buffer_->gpuAddress() + task.dispatch_arguments_offset;
          tasks_data[i].vertex_count_per_warp = task.vertex_count_per_warp;
          tasks_data[i].end_of_command = 0;
          ctx.encoder->useResource(task.draw_arguments, MTL::ResourceUsageRead, MTL::RenderStageVertex);
        }
        tasks_data[task_count - 1].end_of_command = 1;
        // FIXME: 
        ctx.encoder->useResource(gpu_buffer_, MTL::ResourceUsageWrite | MTL::ResourceUsageRead, MTL::RenderStageVertex);
        queue_.emulated_cmd.MarshalGSDispatchArguments(ctx.encoder, gpu_buffer_, offset);
        ctx.encoder->memoryBarrier(
            MTL::BarrierScopeBuffers, MTL::RenderStageVertex,
            MTL::RenderStageVertex | MTL::RenderStageMesh | MTL::RenderStageObject
        );
      }
      data->cmds.execute(ctx);
      encoder.endEncoding();
      data->~RenderEncoderData();
      break;
    }
    case EncoderType::Compute: {
      auto data = static_cast<ComputeEncoderData *>(current);
      auto encoder = cmdbuf_.computeCommandEncoder(false);
      struct wmtcmd_compute_setbuffer setcmd;
      setcmd.type = WMTComputeCommandSetBuffer;
      setcmd.next.set(nullptr);
      setcmd.buffer = (obj_handle_t)gpu_buffer_;
      setcmd.offset = 0;
      setcmd.index = 29;
      encoder.encodeCommands((const wmtcmd_compute_nop *)&setcmd);
      setcmd.index = 30;
      encoder.encodeCommands((const wmtcmd_compute_nop *)&setcmd);
      encoder.encodeCommands(&data->cmd_head);
      encoder.endEncoding();
      data->~ComputeEncoderData();
      break;
    }
    case EncoderType::Blit: {
      auto data = static_cast<BlitEncoderData *>(current);
      auto encoder = cmdbuf_.blitCommandEncoder();
      encoder.encodeCommands(&data->cmd_head);
      encoder.endEncoding();
      data->~BlitEncoderData();
      break;
    }
    case EncoderType::Present: {
      auto data = static_cast<PresentData *>(current);
      auto t0 = clock::now();
      auto drawable = data->layer->nextDrawable();
      auto t1 = clock::now();
      currentFrameStatistics().drawable_blocking_interval += (t1 - t0);
      queue_.emulated_cmd.PresentToDrawable(cmdbuf, data->backbuffer, drawable->texture());
      if (data->after > 0)
        cmdbuf->presentDrawableAfterMinimumDuration(drawable, data->after);
      else
        cmdbuf->presentDrawable(drawable);
      data->~PresentData();
      break;
    }
    case EncoderType::Clear: {
      auto data = static_cast<ClearEncoderData *>(current);
      {
        WMTRenderPassInfo info;
        WMT::InitializeRenderPassInfo(info);
        if (data->clear_dsv) {
          if (data->clear_dsv & 1) {
            info.depth.clear_depth = data->depth_stencil.first;
            info.depth.texture = data->texture;
            info.depth.load_action = WMTLoadActionClear;
            info.depth.store_action = WMTStoreActionStore;
          }
          if (data->clear_dsv & 2) {
            info.stencil.clear_stencil = data->depth_stencil.second;
            info.stencil.texture = data->texture;
            info.stencil.load_action = WMTLoadActionClear;
            info.stencil.store_action = WMTStoreActionStore;
          }
          info.render_target_width = data->width;
          info.render_target_height = data->height;
        } else {
          info.colors[0].clear_color = data->color;
          info.colors[0].texture = data->texture;
          info.colors[0].load_action = WMTLoadActionClear;
          info.colors[0].store_action = WMTStoreActionStore;
        }
        info.render_target_array_length = data->array_length;
        auto encoder = cmdbuf_.renderCommandEncoder(info);
        ((MTL::RenderCommandEncoder *)encoder.handle)
            ->setLabel(NS::String::string("ClearPass", NS::ASCIIStringEncoding));
        encoder.endEncoding();
      }
      data->~ClearEncoderData();
      break;
    }
    case EncoderType::Resolve: {
      auto data = static_cast<ResolveEncoderData *>(current);
      {
        WMTRenderPassInfo info;
        WMT::InitializeRenderPassInfo(info);
        info.colors[0].texture = data->src;
        info.colors[0].load_action = WMTLoadActionLoad;
        info.colors[0].store_action = WMTStoreActionStoreAndMultisampleResolve;
        info.colors[0].slice = data->src_slice;
        info.colors[0].resolve_texture = data->dst;
        info.colors[0].resolve_slice = data->dst_slice;
        info.colors[0].resolve_level = data->dst_level;

        auto encoder = cmdbuf_.renderCommandEncoder(info);
        ((MTL::RenderCommandEncoder *)encoder.handle)
            ->setLabel(NS::String::string("ResolvePass", NS::ASCIIStringEncoding));
        encoder.endEncoding();
      }
      data->~ResolveEncoderData();
      break;
    }
    case EncoderType::SpatialUpscale: {
      auto data = static_cast<SpatialUpscaleData *>(current);
      data->scaler->setColorTexture(data->backbuffer);
      data->scaler->setOutputTexture(data->upscaled);
      data->scaler->encodeToCommandBuffer(cmdbuf);
      data->~SpatialUpscaleData();
      break;
    }
    case EncoderType::SignalEvent: {
      auto data = static_cast<SignalEventData *>(current);
      cmdbuf_.encodeSignalEvent(data->event, data->value);
      data->~SignalEventData();
      break;
    }
    case EncoderType::TemporalUpscale: {
      auto data = static_cast<TemporalUpscaleData *>(current);
      data->scaler->setColorTexture(data->input);
      data->scaler->setOutputTexture(data->output);
      data->scaler->setDepthTexture(data->depth);
      data->scaler->setMotionTexture(data->motion_vector);
      data->scaler->setReset(data->props.reset);
      data->scaler->setDepthReversed(data->props.depth_reversed);
      data->scaler->setInputContentWidth(data->props.input_content_width);
      data->scaler->setInputContentHeight(data->props.input_content_height);
      data->scaler->setMotionVectorScaleX(data->props.motion_vector_scale_x);
      data->scaler->setMotionVectorScaleY(data->props.motion_vector_scale_y);
      data->scaler->setJitterOffsetX(data->props.jitter_offset_x);
      data->scaler->setJitterOffsetY(data->props.jitter_offset_y);
      data->scaler->setPreExposure(data->props.pre_exposure);
      if(data->exposure)
        data->scaler->setExposureTexture(data->exposure);
      data->scaler->encodeToCommandBuffer(cmdbuf);
      data->~TemporalUpscaleData();
      break;
    }
    default:
      break;
    }
    encoder_index--;
  }
  encoder_head.next = nullptr;
  encoder_last = &encoder_head;
  encoder_count_ = 0;

  cmdbuf_.encodeSignalEvent(queue_.event, event_seq_id);

  return visibility_readback;
}

DXMT_ENCODER_LIST_OP
ArgumentEncodingContext::checkEncoderRelation(EncoderData *former, EncoderData *latter) {
  if (former->type == EncoderType::Null)
    return DXMT_ENCODER_LIST_OP_SWAP;
  if (latter->type == EncoderType::Null)
    return DXMT_ENCODER_LIST_OP_SWAP;
  if (former->type == EncoderType::SignalEvent)
    return DXMT_ENCODER_LIST_OP_SYNCHRONIZE;
  if (latter->type == EncoderType::SignalEvent)
    return DXMT_ENCODER_LIST_OP_SYNCHRONIZE;

  while (former->type != latter->type) {
    if (former->type == EncoderType::Clear && latter->type == EncoderType::Render) {
      auto render = reinterpret_cast<RenderEncoderData *>(latter);
      auto clear = reinterpret_cast<ClearEncoderData *>(former);

      if (render->info.render_target_array_length != clear->array_length)
        break;

      if (clear->clear_dsv) {
        if (auto depth_attachment = isClearDepthSignatureMatched(clear, render)) {
          if (depth_attachment->load_action == WMTLoadActionLoad) {
            depth_attachment->clear_depth = clear->depth_stencil.first;
            depth_attachment->load_action = WMTLoadActionClear;
            depth_attachment->store_action = WMTStoreActionStore;
            render->tex_write.merge(clear->tex_write);
          }
          clear->clear_dsv &= ~1;
        }
        if (auto stencil_attachment = isClearStencilSignatureMatched(clear, render)) {
          if (stencil_attachment->load_action == WMTLoadActionLoad) {
            stencil_attachment->clear_stencil = clear->depth_stencil.second;
            stencil_attachment->load_action = WMTLoadActionClear;
            stencil_attachment->store_action = WMTStoreActionStore;
            render->tex_write.merge(clear->tex_write);
          }
          clear->clear_dsv &= ~2;
        }
        if (clear->clear_dsv == 0) {
          currentFrameStatistics().clear_pass_optimized++;
          clear->~ClearEncoderData();
          clear->next = nullptr;
          clear->type = EncoderType::Null;
          return DXMT_ENCODER_LIST_OP_SYNCHRONIZE;
        }
      } else {
        if (auto attachment = isClearColorSignatureMatched(clear, render)) {
          if (attachment->load_action == WMTLoadActionLoad) {
            attachment->load_action = WMTLoadActionClear;
            attachment->clear_color = clear->color;
            if (attachment->store_action != WMTStoreActionDontCare)
              render->tex_write.merge(clear->tex_write);
          }

          currentFrameStatistics().clear_pass_optimized++;
          clear->~ClearEncoderData();
          clear->next = nullptr;
          clear->type = EncoderType::Null;
          return DXMT_ENCODER_LIST_OP_SYNCHRONIZE;
        }
      }
      break;
    }
    if (latter->type == EncoderType::Clear && former->type == EncoderType::Render) {
      auto render = reinterpret_cast<RenderEncoderData *>(former);
      auto clear = reinterpret_cast<ClearEncoderData *>(latter);

      if (clear->clear_dsv) {
        if (render->info.depth.texture == clear->texture.handle) {
          render->info.depth.store_action = WMTStoreActionDontCare;
        }
        if (render->info.stencil.texture == clear->texture.handle) {
          render->info.stencil.store_action = WMTStoreActionDontCare;
        }
      }
    }
    return hasDataDependency(latter, former) ? DXMT_ENCODER_LIST_OP_SYNCHRONIZE : DXMT_ENCODER_LIST_OP_SWAP;
  }

  if (former->type == EncoderType::Render) {
    auto r1 = reinterpret_cast<RenderEncoderData *>(latter);
    auto r0 = reinterpret_cast<RenderEncoderData *>(former);

    if (isEncoderSignatureMatched(r0, r1)) {
      for (unsigned i = 0; i < r0->render_target_count; i++) {
        auto &a0 = r0->info.colors[i];
        auto &a1 = r1->info.colors[i];
        a1.load_action = a0.load_action;
        a1.clear_color = a0.clear_color;
      }

      r1->info.depth.load_action = r0->info.depth.load_action;
      r1->info.depth.clear_depth = r0->info.depth.clear_depth;
      r1->info.depth.store_action = r0->info.depth.store_action;
      r1->info.stencil.load_action = r0->info.stencil.load_action;
      r1->info.stencil.clear_stencil = r0->info.stencil.clear_stencil;
      r1->info.stencil.store_action = r0->info.stencil.store_action;

      r0->cmds.append(std::move(r1->cmds));
      r1->cmds = std::move(r0->cmds);
      r0->pretess_cmds.append(std::move(r1->pretess_cmds));
      r1->pretess_cmds = std::move(r0->pretess_cmds);
      r1->use_tessellation = r0->use_tessellation || r1->use_tessellation;
      r1->use_geometry = r0->use_geometry || r1->use_geometry;
      std::move(
        r1->gs_arg_marshal_tasks.begin(),
        r1->gs_arg_marshal_tasks.end(),
        std::back_inserter(r0->gs_arg_marshal_tasks)
      );
      r1->gs_arg_marshal_tasks = std::move(r0->gs_arg_marshal_tasks);
      r1->use_visibility_result = r0->use_visibility_result || r1->use_visibility_result;

      r1->buf_read.merge(r0->buf_read);
      r1->buf_write.merge(r0->buf_write);
      r1->tex_read.merge(r0->tex_read);
      r1->tex_write.merge(r0->tex_write);

      currentFrameStatistics().render_pass_optimized++;
      r0->~RenderEncoderData();
      r0->next = nullptr;
      r0->type = EncoderType::Null;

      return DXMT_ENCODER_LIST_OP_SYNCHRONIZE;
    }
  }

  return hasDataDependency(latter, former) ? DXMT_ENCODER_LIST_OP_SYNCHRONIZE : DXMT_ENCODER_LIST_OP_SWAP;
}

bool
ArgumentEncodingContext::hasDataDependency(EncoderData *latter, EncoderData *former) {
  if (latter->type == EncoderType::Clear && former->type == EncoderType::Clear) {
    // FIXME: prove it's safe to return false
    return false;
  }
  // read-after-write
  if (!former->buf_write.isDisjointWith(latter->buf_read))
    return true;
  if (!former->tex_write.isDisjointWith(latter->tex_read))
    return true;
  // write-after-write
  if (!former->buf_write.isDisjointWith(latter->buf_write))
    return true;
  if (!former->tex_write.isDisjointWith(latter->tex_write))
    return true;
  // write-after-read
  if (!former->buf_read.isDisjointWith(latter->buf_write))
    return true;
  if (!former->tex_read.isDisjointWith(latter->tex_write))
    return true;
  return false;
}

bool
ArgumentEncodingContext::isEncoderSignatureMatched(RenderEncoderData *r0, RenderEncoderData *r1) {
  // FIXME: it can be different?
  if (r0->render_target_count != r1->render_target_count)
    return false;
  if (r0->dsv_planar_flags != r1->dsv_planar_flags)
    return false;
  if (r0->dsv_readonly_flags != r1->dsv_readonly_flags)
    return false;
  if (r0->info.render_target_array_length != r1->info.render_target_array_length)
    return false;
  if (r0->dsv_planar_flags & 1) {
    if (r0->info.depth.texture != r1->info.depth.texture)
      return false;
    if (r0->dsv_readonly_flags & 1) {
      if (r1->info.depth.load_action == WMTLoadActionClear)
        return false;
    } else {
      if (r0->info.depth.store_action != WMTStoreActionStore)
        return false;
      if (r1->info.depth.load_action != WMTLoadActionLoad)
        return false;
    }
  }
  if (r0->dsv_planar_flags & 2) {
    if (r0->info.stencil.texture != r1->info.stencil.texture)
      return false;
    if (r0->dsv_readonly_flags & 2) {
      if (r1->info.stencil.load_action == WMTLoadActionClear)
        return false;
    } else {
      if (r0->info.stencil.store_action != WMTStoreActionStore)
        return false;
      if (r1->info.stencil.load_action != WMTLoadActionLoad)
        return false;
    }
  }
  for (unsigned i = 0; i < r0->render_target_count; i++) {
    auto &a0 = r0->info.colors[i];
    auto &a1 = r1->info.colors[i];
    if (a0.texture != a1.texture)
      return false;
    if (a0.depth_plane != a1.depth_plane)
      return false;
    if (!a0.texture)
      continue;
    if (a0.store_action != WMTStoreActionStore)
      return false;
    if (a1.load_action != WMTLoadActionLoad)
      return false;
  }
  return true;
}

WMTColorAttachmentInfo *
ArgumentEncodingContext::isClearColorSignatureMatched(ClearEncoderData *clear, RenderEncoderData *render) {
  for (unsigned i = 0; i < render->render_target_count; i++) {
    auto &attachment = render->info.colors[i];
    if (attachment.texture == clear->texture.handle) {
      return &attachment;
    }
  }
  return nullptr;
}

WMTDepthAttachmentInfo *
ArgumentEncodingContext::isClearDepthSignatureMatched(ClearEncoderData *clear, RenderEncoderData *render) {
  if ((clear->clear_dsv & 1) == 0)
    return nullptr;
  if (render->info.depth.texture != clear->texture.handle)
    return nullptr;
  return &render->info.depth;
}

WMTStencilAttachmentInfo *
ArgumentEncodingContext::isClearStencilSignatureMatched(ClearEncoderData *clear, RenderEncoderData *render) {
  if ((clear->clear_dsv & 2) == 0)
    return nullptr;
  if (render->info.stencil.texture != clear->texture.handle)
    return nullptr;
  return &render->info.stencil;
}

} // namespace dxmt