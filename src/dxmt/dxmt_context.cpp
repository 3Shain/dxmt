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
        encoded_buffer[arg.StructurePtrOffset] = 0;
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
      __builtin_unreachable();
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
      __builtin_unreachable();
    }
    case SM50BindingType::Sampler: {
      auto slot = 16 * unsigned(stage) + arg.SM50BindingSlot;
      if (!sampler_[slot].sampler) {
        encoded_buffer[arg.StructurePtrOffset] = 0;
        encoded_buffer[arg.StructurePtrOffset + 1] = (uint64_t)std::bit_cast<uint32_t>(0.0f);
        break;
      }
      encoded_buffer[arg.StructurePtrOffset] = sampler_[slot].sampler->gpuResourceID()._impl;
      // TODO
      encoded_buffer[arg.StructurePtrOffset + 1] = (uint64_t)std::bit_cast<uint32_t>(0.0f);
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
          encoded_buffer[arg.StructurePtrOffset] =
              access(srv.texture, srv.viewId, DXMT_ENCODER_RESOURCE_ACESS_READ)->gpuResourceID()._impl;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
          makeResident<stage, TessellationDraw>(srv.texture.ptr(), srv.viewId);
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
          encoded_buffer[arg.StructurePtrOffset] = access(uav.texture, uav.viewId, access_flags)->gpuResourceID()._impl;
          encoded_buffer[arg.StructurePtrOffset + 1] = 0;
          makeResident<stage, TessellationDraw>(uav.texture.ptr(), uav.viewId, read, write);
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

  encoder_current = encoder_info;
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
  encoder_info->clear_dsv = flag;
  encoder_info->texture = texture->view(viewId);
  encoder_info->depth_stencil = {depth, stencil};
  encoder_info->array_length = arrayLength;

  encoder_current = encoder_info;
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

  encoder_current = encoder_info;
  endPass();
};

void
ArgumentEncodingContext::present(Rc<Texture> &texture, CA::MetalDrawable *drawable, double after) {
  assert(!encoder_current);
  auto encoder_info = allocate<PresentData>();
  encoder_info->type = EncoderType::Present;
  encoder_info->id = nextEncoderId();
  encoder_info->backbuffer = texture->current()->texture();
  encoder_info->drawable = drawable;
  encoder_info->after = after;

  encoder_current = encoder_info;
  endPass();
}

RenderEncoderData *
ArgumentEncodingContext::startRenderPass(Obj<MTL::RenderPassDescriptor> &&descriptor, uint32_t dsv_planar_flags) {
  assert(!encoder_current);
  auto encoder_info = allocate<RenderEncoderData>();
  encoder_info->type = EncoderType::Render;
  encoder_info->id = nextEncoderId();
  encoder_info->descriptor = std::move(descriptor);
  encoder_info->dsv_planar_flags = dsv_planar_flags;
  encoder_current = encoder_info;

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
  return encoder_info;
}

EncoderData *
ArgumentEncodingContext::startBlitPass() {
  assert(!encoder_current);
  auto encoder_info = allocate<BlitEncoderData>();
  encoder_info->type = EncoderType::Blit;
  encoder_info->id = nextEncoderId();
  encoder_current = encoder_info;

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
  auto [_, buffer, offset] = queue.AllocateTempBuffer(seq_id_, size, alignment);
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

  if (active_visibility_query_count_) {
    render_encoder->use_visibility_result = 1;
    if (auto offset = vro_state_.getNextWriteOffset(true); offset != ~0uLL)
      encodeRenderCommand([offset](RenderCommandContext &ctx) {
        ctx.encoder->setVisibilityResultMode(MTL::VisibilityResultModeCounting, offset << 3);
      });
  } else {
    if (vro_state_.getNextWriteOffset(false) != ~0uLL && render_encoder->use_visibility_result)
      encodeRenderCommand([](RenderCommandContext &ctx) {
        ctx.encoder->setVisibilityResultMode(MTL::VisibilityResultModeDisabled, 0);
      });
  }
}

std::unique_ptr<VisibilityResultReadback>
ArgumentEncodingContext::flushCommands(MTL::CommandBuffer *cmdbuf, uint64_t seqId) {
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
    EncoderData *prev;
    for (j = 1; j < encoder_count; j++) {
      for (i = j; i > 0; i--) {
        prev = encoders[i - 1];
        switch (checkEncoderRelation(prev, encoders[i])) {
        case DXMT_ENCODER_LIST_OP_SYNCHRONIZE:
          goto end;
        case DXMT_ENCODER_LIST_OP_SWAP:
          encoders[i - 1] = encoders[i];
          encoders[i] = prev;
          break;
        case DXMT_ENCODER_LIST_OP_COALESCE:
          /* ignore */
          encoders[i - 1] = encoders[i];
          encoders[i] = prev;
          prev->type = EncoderType::Null;
          break;
        }
      }
    end:;
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
      ComputeCommandContext ctx{cmdbuf->computeCommandEncoder(), {}, queue.emulated_cmd};
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
      data->cmds.~CommandList();
      ctx.encoder->endEncoding();
      data->~BlitEncoderData();
      break;
    }
    case EncoderType::Present: {
      auto data = static_cast<PresentData *>(current);
      auto drawable = data->drawable.ptr();
      queue.emulated_cmd.PresentToDrawable(cmdbuf, data->backbuffer, drawable->texture());
      if (data->after > 0)
        cmdbuf->presentDrawableAfterMinimumDuration(drawable, data->after);
      else
        cmdbuf->presentDrawable(drawable);
      data->~PresentData();
      break;
    }
    case EncoderType::Clear: {
      auto data = static_cast<ClearEncoderData *>(current);
      if (data->skipped)
        break;
      {
        auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        MTL::Texture *texture = data->texture;
        if (data->clear_dsv) {
          uint32_t planar_flags = DepthStencilPlanarFlags(texture->pixelFormat());
          if (data->clear_dsv & planar_flags & 1) {
            auto attachmentz = enc_descriptor->depthAttachment();
            attachmentz->setClearDepth(data->depth_stencil.first);
            attachmentz->setTexture(texture);
            attachmentz->setLoadAction(MTL::LoadActionClear);
            attachmentz->setStoreAction(MTL::StoreActionStore);
            attachmentz->setSlice(0);
            attachmentz->setLevel(0);
            attachmentz->setDepthPlane(0);
          }
          if (data->clear_dsv & planar_flags & 2) {
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
      break;
    }
    case EncoderType::Resolve: {
      auto data = static_cast<ResolveEncoderData *>(current);
      if (data->skipped)
        break;
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

  return visibility_readback;
}

DXMT_ENCODER_LIST_OP
ArgumentEncodingContext::checkEncoderRelation(EncoderData *former, EncoderData *latter) {
  if (former->type == EncoderType::Null)
    return DXMT_ENCODER_LIST_OP_SWAP;
  if (latter->type == EncoderType::Null)
    return DXMT_ENCODER_LIST_OP_SYNCHRONIZE;

  while (former->type != latter->type) {
    if (former->type == EncoderType::Clear && latter->type == EncoderType::Render) {
      auto render = reinterpret_cast<RenderEncoderData *>(latter);
      auto clear = reinterpret_cast<ClearEncoderData *>(former);

      if (render->descriptor->renderTargetArrayLength() != clear->array_length)
        break;

      if (clear->clear_dsv) {
        auto depth_attachment = render->descriptor->depthAttachment();
        auto stencil_attachment = render->descriptor->stencilAttachment();
        if (depth_attachment->texture() != clear->texture.ptr() &&
            stencil_attachment->texture() != clear->texture.ptr())
          break;
        if (clear->clear_dsv & 1 && depth_attachment->loadAction() == MTL::LoadActionLoad) {
          depth_attachment->setClearDepth(clear->depth_stencil.first);
          depth_attachment->setLoadAction(MTL::LoadActionClear);
        }
        if (clear->clear_dsv & 2 && stencil_attachment->loadAction() == MTL::LoadActionLoad) {
          stencil_attachment->setClearStencil(clear->depth_stencil.second);
          stencil_attachment->setLoadAction(MTL::LoadActionClear);
        }
        clear->~ClearEncoderData();
        clear->next = nullptr;
        clear->type = EncoderType::Null;
        return DXMT_ENCODER_LIST_OP_COALESCE;
      } else {
        for (unsigned i = 0; i < 8 /* FIXME: optimize constant */; i++) {
          auto attachment = render->descriptor->colorAttachments()->object(i);
          if (attachment->texture() == clear->texture.ptr() && attachment->loadAction() == MTL::LoadActionLoad) {
            attachment->setLoadAction(MTL::LoadActionClear);
            attachment->setClearColor(clear->color);
            clear->~ClearEncoderData();
            clear->next = nullptr;
            clear->type = EncoderType::Null;
            return DXMT_ENCODER_LIST_OP_COALESCE;
          }
        }
      }
      break;
    }
    return hasDataDependency(latter, former) ? DXMT_ENCODER_LIST_OP_SYNCHRONIZE : DXMT_ENCODER_LIST_OP_SWAP;
  }

  return DXMT_ENCODER_LIST_OP_SYNCHRONIZE; // TODO
}

bool
ArgumentEncodingContext::hasDataDependency(EncoderData *from, EncoderData *to) {
  return true; // TODO
}

} // namespace dxmt