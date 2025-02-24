#include "dxmt_context.hpp"
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

template void ArgumentEncodingContext::encodeVertexBuffers<true>(uint32_t slot_mask);
template void ArgumentEncodingContext::encodeVertexBuffers<false>(uint32_t slot_mask);

template <bool TessellationDraw>
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
    auto length = current->buffer()->length();
    entries[index].buffer_handle = current->gpuAddress + state.offset;
    entries[index].stride = state.stride;
    entries[index++].length = length > state.offset ? length - state.offset : 0;
    // FIXME: did we intended to use the whole buffer?
    access(buffer, DXMT_ENCODER_RESOURCE_ACESS_READ);
    makeResident<PipelineStage::Vertex, TessellationDraw>(buffer.ptr());
  };
  if constexpr (TessellationDraw)
    encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 16); });
  else
    encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setVertexBufferOffset(offset, 16); });
}

template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Vertex, false>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Pixel, false>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Vertex, true>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Pixel, true>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Hull, true>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Domain, true>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeConstantBuffers<PipelineStage::Compute, false>(const MTL_SHADER_REFLECTION *reflection);

template <PipelineStage stage, bool TessellationDraw>
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
        makeResident<stage, TessellationDraw>(dummy_cbuffer_, GetResidencyMask<TessellationDraw>(stage, true, false));
        continue;
      }
      auto argbuf = cbuf.buffer;
      encoded_buffer[arg.StructurePtrOffset] = argbuf->current()->gpuAddress + (cbuf.offset);
      // FIXME: did we intended to use the whole buffer?
      access(argbuf, DXMT_ENCODER_RESOURCE_ACESS_READ);
      makeResident<stage, TessellationDraw>(argbuf.ptr());
      break;
    }
    default:
      DXMT_UNREACHABLE
    }
  }

  /* kConstantBufferTableBinding = 29 */
  if constexpr (stage == PipelineStage::Compute) {
    encodeComputeCommand([offset](ComputeCommandContext &ctx) { ctx.encoder->setBufferOffset(offset, 29); });
  } else if constexpr (stage == PipelineStage::Hull) {
    encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setMeshBufferOffset(offset, 29); });
  } else if constexpr (stage == PipelineStage::Vertex) {
    if constexpr (TessellationDraw)
      encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 29); });
    else
      encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setVertexBufferOffset(offset, 29); });
  } else {
    encodeRenderCommand([offset](RenderCommandContext &ctx) {
      if constexpr (stage == PipelineStage::Pixel) {
        ctx.encoder->setFragmentBufferOffset(offset, 29);
      } else if constexpr (stage == PipelineStage::Domain) {
        ctx.encoder->setVertexBufferOffset(offset, 29);
      } else {
        assert(0 && "Not implemented or unreachable");
      }
    });
  }
};

template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Vertex, false>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Pixel, false>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Vertex, true>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Pixel, true>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Hull, true>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Domain, true>(const MTL_SHADER_REFLECTION *reflection);
template void
ArgumentEncodingContext::encodeShaderResources<PipelineStage::Compute, false>(const MTL_SHADER_REFLECTION *reflection);

template <PipelineStage stage, bool TessellationDraw>
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
          makeResident<stage, TessellationDraw>(srv.buffer.ptr());
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
          makeResident<stage, TessellationDraw>(srv.buffer.ptr(), srv.viewId);
        } else if (srv.texture.ptr()) {
          assert(arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP);
          auto viewIdChecked = srv.texture->checkViewUseArray(srv.viewId, arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_ARRAY);
          encoded_buffer[arg.StructurePtrOffset] =
              access(srv.texture, viewIdChecked, DXMT_ENCODER_RESOURCE_ACESS_READ)->gpuResourceID()._impl;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
          makeResident<stage, TessellationDraw>(srv.texture.ptr(), viewIdChecked);
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
          makeResident<stage, TessellationDraw>(uav.buffer.ptr(), read, write);
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
          makeResident<stage, TessellationDraw>(uav.buffer.ptr(), uav.viewId, read, write);
        } else if (uav.texture.ptr()) {
          assert(arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP);
          auto viewIdChecked = uav.texture->checkViewUseArray(uav.viewId, arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_ARRAY);
          encoded_buffer[arg.StructurePtrOffset] = access(uav.texture, viewIdChecked, access_flags)->gpuResourceID()._impl;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
          makeResident<stage, TessellationDraw>(uav.texture.ptr(), viewIdChecked, read, write);
        } else {
          encoded_buffer[arg.StructurePtrOffset] = 0;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
        }
      }
      if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
        encoded_buffer[arg.StructurePtrOffset + 2] = uav.counter->current()->gpuAddress;
        access(uav.counter, 0, 4, DXMT_ENCODER_RESOURCE_ACESS_READ | DXMT_ENCODER_RESOURCE_ACESS_WRITE);
        makeResident<stage, TessellationDraw>(uav.counter.ptr(), true, true);
      }
      break;
    }
    }
  }

  if constexpr (stage == PipelineStage::Compute) {
    encodeComputeCommand([offset](ComputeCommandContext &ctx) { ctx.encoder->setBufferOffset(offset, 30); });
  } else if constexpr (stage == PipelineStage::Hull) {
    encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setMeshBufferOffset(offset, 30); });
  } else if constexpr (stage == PipelineStage::Vertex) {
    if constexpr (TessellationDraw)
      encodePreTessCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setObjectBufferOffset(offset, 30); });
    else
      encodeRenderCommand([offset](RenderCommandContext &ctx) { ctx.encoder->setVertexBufferOffset(offset, 30); });
  } else {
    encodeRenderCommand([offset](RenderCommandContext &ctx) {
      if constexpr (stage == PipelineStage::Pixel) {
        ctx.encoder->setFragmentBufferOffset(offset, 30);
      } else if constexpr (stage == PipelineStage::Domain) {
        ctx.encoder->setVertexBufferOffset(offset, 30);
      } else {
        assert(0 && "Not implemented or unreachable");
      }
    });
  }
}

void
ArgumentEncodingContext::clearColor(
    Rc<Texture> &&texture, unsigned viewId, unsigned arrayLength, MTL::ClearColor color
) {
  assert(!encoder_current);
  auto encoder_info = allocate<ClearEncoderData>();
  encoder_info->type = EncoderType::Clear;
  encoder_info->id = nextEncoderId();
  encoder_info->clear_dsv = 0;
  encoder_info->texture = texture->view(viewId);
  encoder_info->color = color;
  encoder_info->array_length = arrayLength;

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
  encoder_info->clear_dsv = flag & DepthStencilPlanarFlags(texture->pixelFormat());;
  encoder_info->texture = texture->view(viewId);
  encoder_info->depth_stencil = {depth, stencil};
  encoder_info->array_length = arrayLength;

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
  encoder_info->src = src->current()->texture();
  encoder_info->dst = dst->current()->texture();
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
    Obj<MTL::RenderPassDescriptor> &&descriptor, uint32_t dsv_planar_flags, uint32_t render_target_count
) {
  assert(!encoder_current);
  auto encoder_info = allocate<RenderEncoderData>();
  encoder_info->type = EncoderType::Render;
  encoder_info->id = nextEncoderId();
  encoder_info->descriptor = std::move(descriptor);
  encoder_info->dsv_planar_flags = dsv_planar_flags;
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
    std::erase_if(pending_queries_, [=](auto &query) -> bool { return query->queryEndAt() == seqId; });
  }

  while (encoder_index) {
    auto current = encoders[encoder_count - encoder_index];
    switch (current->type) {
    case EncoderType::Render: {
      auto data = static_cast<RenderEncoderData *>(current);
      if (data->use_visibility_result) {
        assert(visibility_readback);
        data->descriptor->setVisibilityResultBuffer(visibility_readback->visibility_result_heap);
      }
      auto encoder = cmdbuf->renderCommandEncoder(data->descriptor.ptr());
      RenderCommandContext ctx{encoder, data->dsv_planar_flags};
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
        data->pretess_cmds.execute(ctx);
        encoder->memoryBarrier(MTL::BarrierScopeBuffers, MTL::RenderStageMesh, MTL::RenderStageVertex);
      }
      data->cmds.execute(ctx);
      ctx.encoder->endEncoding();
      data->~RenderEncoderData();
      break;
    }
    case EncoderType::Compute: {
      auto data = static_cast<ComputeEncoderData *>(current);
      ComputeCommandContext ctx{cmdbuf->computeCommandEncoder(), {}, queue_.emulated_cmd};
      ctx.encoder->setBuffer(gpu_buffer_, 0, 29);
      ctx.encoder->setBuffer(gpu_buffer_, 0, 30);
      data->cmds.execute(ctx);
      ctx.encoder->endEncoding();
      data->~ComputeEncoderData();
      break;
    }
    case EncoderType::Blit: {
      auto data = static_cast<BlitEncoderData *>(current);
      BlitCommandContext ctx{cmdbuf->blitCommandEncoder()};
      data->cmds.execute(ctx);
      ctx.encoder->endEncoding();
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
        auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        MTL::Texture *texture = data->texture;
        if (data->clear_dsv) {
          if (data->clear_dsv & 1) {
            auto attachmentz = enc_descriptor->depthAttachment();
            attachmentz->setClearDepth(data->depth_stencil.first);
            attachmentz->setTexture(texture);
            attachmentz->setLoadAction(MTL::LoadActionClear);
            attachmentz->setStoreAction(MTL::StoreActionStore);
            attachmentz->setSlice(0);
            attachmentz->setLevel(0);
            attachmentz->setDepthPlane(0);
          }
          if (data->clear_dsv & 2) {
            auto attachmentz = enc_descriptor->stencilAttachment();
            attachmentz->setClearStencil(data->depth_stencil.second);
            attachmentz->setTexture(texture);
            attachmentz->setLoadAction(MTL::LoadActionClear);
            attachmentz->setStoreAction(MTL::StoreActionStore);
            attachmentz->setSlice(0);
            attachmentz->setLevel(0);
            attachmentz->setDepthPlane(0);
          }
          enc_descriptor->setRenderTargetHeight(texture->height());
          enc_descriptor->setRenderTargetWidth(texture->width());
        } else {
          auto attachmentz = enc_descriptor->colorAttachments()->object(0);
          attachmentz->setClearColor(data->color);
          attachmentz->setTexture(texture);
          attachmentz->setLoadAction(MTL::LoadActionClear);
          attachmentz->setStoreAction(MTL::StoreActionStore);
          attachmentz->setSlice(0);
          attachmentz->setLevel(0);
          attachmentz->setDepthPlane(0);
        }
        enc_descriptor->setRenderTargetArrayLength(data->array_length);
        auto enc = cmdbuf->renderCommandEncoder(enc_descriptor);
        enc->setLabel(NS::String::string("ClearPass", NS::ASCIIStringEncoding));
        enc->endEncoding();
      }
      data->~ClearEncoderData();
      break;
    }
    case EncoderType::Resolve: {
      auto data = static_cast<ResolveEncoderData *>(current);
      {
        auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        auto attachmentz = enc_descriptor->colorAttachments()->object(0);
        attachmentz->setTexture(data->src);
        attachmentz->setLoadAction(MTL::LoadActionLoad);
        attachmentz->setStoreAction(MTL::StoreActionMultisampleResolve);
        attachmentz->setSlice(data->src_slice);
        attachmentz->setResolveTexture(data->dst);
        attachmentz->setResolveLevel(data->dst_level);
        attachmentz->setResolveSlice(data->dst_slice);

        auto enc = cmdbuf->renderCommandEncoder(enc_descriptor);
        enc->setLabel(NS::String::string("ResolvePass", NS::ASCIIStringEncoding));
        enc->endEncoding();
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
      cmdbuf->encodeSignalEvent(data->event, data->value);
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

  cmdbuf->encodeSignalEvent(queue_.event, event_seq_id);

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

      if (render->descriptor->renderTargetArrayLength() != clear->array_length)
        break;

      if (clear->clear_dsv) {
        if (auto depth_attachment = isClearDepthSignatureMatched(clear, render)) {
          if (depth_attachment->loadAction() == MTL::LoadActionLoad) {
            depth_attachment->setClearDepth(clear->depth_stencil.first);
            depth_attachment->setLoadAction(MTL::LoadActionClear);
            if (depth_attachment->storeAction() != MTL::StoreActionDontCare)
              render->tex_write.merge(clear->tex_write);
          }
          clear->clear_dsv &= ~1;
        }
        if (auto stencil_attachment = isClearStencilSignatureMatched(clear, render)) {
          if (stencil_attachment->loadAction() == MTL::LoadActionLoad) {
            stencil_attachment->setClearStencil(clear->depth_stencil.second);
            stencil_attachment->setLoadAction(MTL::LoadActionClear);
            if (stencil_attachment->storeAction() != MTL::StoreActionDontCare)
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
          if (attachment->loadAction() == MTL::LoadActionLoad) {
            attachment->setLoadAction(MTL::LoadActionClear);
            attachment->setClearColor(clear->color);
            if (attachment->storeAction() != MTL::StoreActionDontCare)
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
        if (render->descriptor->depthAttachment()->texture() == clear->texture.ptr()) {
          render->descriptor->depthAttachment()->setStoreAction(MTL::StoreActionDontCare);
        }
        if (render->descriptor->stencilAttachment()->texture() == clear->texture.ptr()) {
          render->descriptor->stencilAttachment()->setStoreAction(MTL::StoreActionDontCare);
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
        auto a0 = r0->descriptor->colorAttachments()->object(i);
        auto a1 = r1->descriptor->colorAttachments()->object(i);
        a1->setLoadAction(a0->loadAction());
        a1->setClearColor(a0->clearColor());
      }

      r1->descriptor->depthAttachment()->setLoadAction(r0->descriptor->depthAttachment()->loadAction());
      r1->descriptor->depthAttachment()->setClearDepth(r0->descriptor->depthAttachment()->clearDepth());
      r1->descriptor->stencilAttachment()->setLoadAction(r0->descriptor->stencilAttachment()->loadAction());
      r1->descriptor->stencilAttachment()->setClearStencil(r0->descriptor->stencilAttachment()->clearStencil());

      r0->cmds.append(std::move(r1->cmds));
      r1->cmds = std::move(r0->cmds);
      r0->pretess_cmds.append(std::move(r1->pretess_cmds));
      r1->pretess_cmds = std::move(r0->pretess_cmds);
      r1->use_tessellation = r0->use_tessellation || r1->use_tessellation;
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
  if (r0->descriptor->renderTargetArrayLength() != r1->descriptor->renderTargetArrayLength())
    return false;
  if (r0->dsv_planar_flags & 1) {
    if (r0->descriptor->depthAttachment()->texture() != r1->descriptor->depthAttachment()->texture())
      return false;
    if (r0->descriptor->depthAttachment()->storeAction() != MTL::StoreActionStore)
      return false;
    if (r1->descriptor->depthAttachment()->loadAction() != MTL::LoadActionLoad)
      return false;
  }
  if (r0->dsv_planar_flags & 2) {
    if (r0->descriptor->stencilAttachment()->texture() != r1->descriptor->stencilAttachment()->texture())
      return false;
    if (r0->descriptor->stencilAttachment()->storeAction() != MTL::StoreActionStore)
      return false;
    if (r1->descriptor->stencilAttachment()->loadAction() != MTL::LoadActionLoad)
      return false;
  }
  for (unsigned i = 0; i < r0->render_target_count; i++) {
    auto a0 = r0->descriptor->colorAttachments()->object(i);
    auto a1 = r1->descriptor->colorAttachments()->object(i);
    if (a0->texture() != a1->texture())
      return false;
    if (a0->depthPlane() != a1->depthPlane())
      return false;
    if (!a0->texture())
      continue;
    if (a0->storeAction() != MTL::StoreActionStore)
      return false;
    if (a1->loadAction() != MTL::LoadActionLoad)
      return false;
  }
  return true;
}

MTL::RenderPassColorAttachmentDescriptor *
ArgumentEncodingContext::isClearColorSignatureMatched(ClearEncoderData *clear, RenderEncoderData *render) {
  for (unsigned i = 0; i < render->render_target_count; i++) {
    auto attachment = render->descriptor->colorAttachments()->object(i);
    if (attachment->texture() == clear->texture.ptr()) {
      return attachment;
    }
  }
  return nullptr;
}

MTL::RenderPassDepthAttachmentDescriptor *
ArgumentEncodingContext::isClearDepthSignatureMatched(ClearEncoderData *clear, RenderEncoderData *render) {
  if((clear->clear_dsv & 1) == 0)
    return nullptr;
  auto depth_attachment = render->descriptor->depthAttachment();
  if (depth_attachment->texture() != clear->texture.ptr())
    return nullptr;
  return depth_attachment;
}

MTL::RenderPassStencilAttachmentDescriptor *
ArgumentEncodingContext::isClearStencilSignatureMatched(ClearEncoderData *clear, RenderEncoderData *render) {
  if ((clear->clear_dsv & 2) == 0)
    return nullptr;
  auto stencil_attachment = render->descriptor->stencilAttachment();
  if (stencil_attachment->texture() != clear->texture.ptr())
    return nullptr;
  return stencil_attachment;
}

} // namespace dxmt