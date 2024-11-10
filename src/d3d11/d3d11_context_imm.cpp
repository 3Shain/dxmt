#include "d3d11_query.hpp"
#include "dxmt_command_queue.hpp"
#include "d3d11_context_impl.cpp"

namespace dxmt {
struct ContextInternalState {
  CommandQueue &cmd_queue;
};

using ImmediateContextBase = MTLD3D11DeviceContextImplBase<ContextInternalState>;

template <>
template <typename Fn>
void
ImmediateContextBase::EmitCommand(Fn &&fn) {
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) { std::invoke(fn, ctx); });
}

template <>
BindingRef
ImmediateContextBase::Use(IMTLBindable *bindable) {
  return bindable->UseBindable(ctx_state.cmd_queue.CurrentSeqId());
}

template <>
BindingRef
ImmediateContextBase::Use(IMTLD3D11RenderTargetView *rtv) {
  return rtv->UseBindable(ctx_state.cmd_queue.CurrentSeqId());
}

template <>
BindingRef
ImmediateContextBase::Use(IMTLD3D11DepthStencilView *dsv) {
  return dsv->UseBindable(ctx_state.cmd_queue.CurrentSeqId());
}

template <>
BindingRef
ImmediateContextBase::Use(IMTLDynamicBuffer *dynamic_buffer) {
  return dynamic_buffer->GetCurrentBufferBinding();
}

template <>
ENCODER_INFO *
ImmediateContextBase::GetLastEncoder() {
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  return chk->get_last_encoder();
}
template <>
ENCODER_CLEARPASS_INFO *
ImmediateContextBase::MarkClearPass() {
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  return chk->mark_clear_pass();
}
template <>
ENCODER_RENDER_INFO *
ImmediateContextBase::MarkRenderPass() {
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  return chk->mark_render_pass();
}
template <>
ENCODER_INFO *
ImmediateContextBase::MarkPass(EncoderKind kind) {
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  return chk->mark_pass(kind);
}

template <>
template <typename T>
moveonly_list<T>
ImmediateContextBase::AllocateCommandData(size_t n) {
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  return moveonly_list<T>((T *)chk->allocate_cpu_heap(sizeof(T) * n, alignof(T)), n);
}

template <>
void
ImmediateContextBase::UpdateUAVCounter(IMTLD3D11UnorderedAccessView *uav, uint32_t value) {
  auto currentChunkId = ctx_state.cmd_queue.CurrentSeqId();
  auto new_counter_handle = ctx_state.cmd_queue.counter_pool.AllocateCounter(currentChunkId, value);
  // it's possible that old_counter_handle == new_counter_handle
  // if the uav doesn't support counter
  // thus it becomes essentially a no-op.
  auto old_counter_handle = uav->SwapCounter(new_counter_handle);
  if (old_counter_handle != DXMT_NO_COUNTER) {
    ctx_state.cmd_queue.counter_pool.DiscardCounter(currentChunkId, old_counter_handle);
  }
};

template <>
std::tuple<void *, MTL::Buffer *, uint64_t>
ImmediateContextBase::AllocateStagingBuffer(size_t size, size_t alignment) {
  return ctx_state.cmd_queue.AllocateStagingBuffer(size, alignment);
}

template <>
bool
ImmediateContextBase::UseCopyDestination(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  return pResource->UseCopyDestination(
      Subresource, ctx_state.cmd_queue.CurrentSeqId(), pBuffer, pBytesPerRow, pBytesPerImage
  );
}
template <>
bool
ImmediateContextBase::UseCopySource(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  return pResource->UseCopySource(
      Subresource, ctx_state.cmd_queue.CurrentSeqId(), pBuffer, pBytesPerRow, pBytesPerImage
  );
}

template <>
BindingRef
ImmediateContextBase::UseImmediate(IMTLBindable *bindable) {
  if (bindable->GetContentionState(ctx_state.cmd_queue.CoherentSeqId()))
    return BindingRef();
  return bindable->UseBindable(ctx_state.cmd_queue.CurrentSeqId());
}

template <>
template <ShaderType stage, bool Tessellation>
bool
ImmediateContextBase::UploadShaderStageResourceBinding() {
  auto &ShaderStage = state_.ShaderStages[(UINT)stage];
  if (!ShaderStage.Shader) {
    return true;
  }
  auto &UAVBindingSet = stage == ShaderType::Compute ? state_.ComputeStageUAV.UAVs : state_.OutputMerger.UAVs;

  const MTL_SHADER_REFLECTION *reflection = &ShaderStage.Shader->GetManagedShader()->reflection();

  bool dirty_cbuffer = ShaderStage.ConstantBuffers.any_dirty_masked(reflection->ConstantBufferSlotMask);
  bool dirty_sampler = ShaderStage.Samplers.any_dirty_masked(reflection->SamplerSlotMask);
  bool dirty_srv = ShaderStage.SRVs.any_dirty_masked(reflection->SRVSlotMaskHi, reflection->SRVSlotMaskLo);
  bool dirty_uav = UAVBindingSet.any_dirty_masked(reflection->UAVSlotMask);
  if (!dirty_cbuffer && !dirty_sampler && !dirty_srv && !dirty_uav)
    return true;

  auto currentChunkId = ctx_state.cmd_queue.CurrentSeqId();

  auto ConstantBufferCount = reflection->NumConstantBuffers;
  auto BindingCount = reflection->NumArguments;
  auto ArgumentTableQwords = reflection->ArgumentTableQwords;
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  auto encoderId = chk->current_encoder_id();

  auto useResource = [&](BindingRef &&res, MTL_BINDABLE_RESIDENCY_MASK residencyMask) {
    chk->emit([res = std::move(res), residencyMask](CommandChunk::context &ctx) {
      switch (stage) {
      case ShaderType::Vertex:
      case ShaderType::Pixel:
      case ShaderType::Hull:
      case ShaderType::Domain:
        ctx.render_encoder->useResource(
            res.resource(&ctx), GetUsageFromResidencyMask(residencyMask), GetStagesFromResidencyMask(residencyMask)
        );
        if (res.withBackedBuffer()) {
          ctx.render_encoder->useResource(
              res.buffer(), GetUsageFromResidencyMask(residencyMask), GetStagesFromResidencyMask(residencyMask)
          );
        }
        break;
      case ShaderType::Compute:
        ctx.compute_encoder->useResource(res.resource(&ctx), GetUsageFromResidencyMask(residencyMask));
        if (res.withBackedBuffer()) {
          ctx.compute_encoder->useResource(res.buffer(), GetUsageFromResidencyMask(residencyMask));
        }
        break;
      case ShaderType::Geometry:
        D3D11_ASSERT(0 && "Not implemented");
        break;
      }
    });
  };
  if (ConstantBufferCount && dirty_cbuffer) {

    auto [heap, offset] = chk->allocate_gpu_heap(ConstantBufferCount << 3, 16);
    uint64_t *write_to_it = chk->gpu_argument_heap_contents + (offset >> 3);

    for (unsigned i = 0; i < ConstantBufferCount; i++) {
      auto &arg = reflection->ConstantBuffers[i];
      auto slot = arg.SM50BindingSlot;
      MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
      SIMPLE_RESIDENCY_TRACKER *pTracker;
      switch (arg.Type) {
      case SM50BindingType::ConstantBuffer: {
        if (!ShaderStage.ConstantBuffers.test_bound(slot)) {
          ERR("expect constant buffer at slot ", slot, " but none is bound.");
          return false;
        }
        auto &cbuf = ShaderStage.ConstantBuffers[slot];
        auto argbuf = cbuf.Buffer->GetArgumentData(&pTracker);
        write_to_it[arg.StructurePtrOffset] = argbuf.buffer() + (cbuf.FirstConstant << 4);
        ShaderStage.ConstantBuffers.clear_dirty(slot);
        pTracker->CheckResidency(encoderId, GetResidencyMask<Tessellation>(stage, true, false), &newResidencyMask);
        if (newResidencyMask) {
          useResource(Use(cbuf.Buffer), newResidencyMask);
        }
        break;
      }
      default:
        D3D11_ASSERT(0 && "unreachable");
      }
    }

    /* kConstantBufferTableBinding = 29 */
    chk->emit([offset](CommandChunk::context &ctx) {
      if constexpr (stage == ShaderType::Vertex) {
        if constexpr (Tessellation) {
          ctx.render_encoder->setObjectBufferOffset(offset, 29);
        } else {
          ctx.render_encoder->setVertexBufferOffset(offset, 29);
        }
      } else if constexpr (stage == ShaderType::Pixel) {
        ctx.render_encoder->setFragmentBufferOffset(offset, 29);
      } else if constexpr (stage == ShaderType::Compute) {
        ctx.compute_encoder->setBufferOffset(offset, 29);
      } else if constexpr (stage == ShaderType::Hull) {
        ctx.render_encoder->setMeshBufferOffset(offset, 29);
      } else if constexpr (stage == ShaderType::Domain) {
        ctx.render_encoder->setVertexBufferOffset(offset, 29);
      } else {
        D3D11_ASSERT(0 && "Not implemented");
      }
    });
  }

  if (BindingCount && (dirty_sampler || dirty_srv || dirty_uav)) {
    auto [heap, offset] = chk->allocate_gpu_heap(ArgumentTableQwords * 8, 16);
    uint64_t *write_to_it = chk->gpu_argument_heap_contents + (offset >> 3);

    for (unsigned i = 0; i < BindingCount; i++) {
      auto &arg = reflection->Arguments[i];
      auto slot = arg.SM50BindingSlot;
      MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
      SIMPLE_RESIDENCY_TRACKER *pTracker;
      switch (arg.Type) {
      case SM50BindingType::ConstantBuffer: {
        D3D11_ASSERT(0 && "unreachable");
      }
      case SM50BindingType::Sampler: {
        if (!ShaderStage.Samplers.test_bound(slot)) {
          ERR("expect sample at slot ", slot, " but none is bound.");
          return false;
        }
        auto &sampler = ShaderStage.Samplers[slot];
        write_to_it[arg.StructurePtrOffset] = sampler.Sampler->GetArgumentHandle();
        write_to_it[arg.StructurePtrOffset + 1] = (uint64_t)std::bit_cast<uint32_t>(sampler.Sampler->GetLODBias());
        ShaderStage.Samplers.clear_dirty(slot);
        break;
      }
      case SM50BindingType::SRV: {
        if (!ShaderStage.SRVs.test_bound(slot)) {
          // TODO: debug only
          // ERR("expect shader resource at slot ", slot, " but none is bound.");
          write_to_it[arg.StructurePtrOffset] = 0;
          write_to_it[arg.StructurePtrOffset + 1] = 0;
          break;
        }
        auto &srv = ShaderStage.SRVs[slot];
        auto arg_data = srv.SRV->GetArgumentData(&pTracker);
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
          write_to_it[arg.StructurePtrOffset] = arg_data.buffer();
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
          if (arg_data.requiresContext()) {
            chk->emit([=, ref = srv.SRV](CommandChunk::context &ctx) {
              write_to_it[arg.StructurePtrOffset] = arg_data.texture(&ctx);
            });
          } else {
            write_to_it[arg.StructurePtrOffset] = arg_data.texture();
          }
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
          write_to_it[arg.StructurePtrOffset + 1] = arg_data.width();
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP) {
          write_to_it[arg.StructurePtrOffset + 1] = std::bit_cast<uint32_t>(arg_data.min_lod());
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET) {
          write_to_it[arg.StructurePtrOffset + 1] = arg_data.element_offset();
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
          D3D11_ASSERT(0 && "srv can not have counter associated");
        }
        ShaderStage.SRVs.clear_dirty(slot);
        pTracker->CheckResidency(encoderId, GetResidencyMask<Tessellation>(stage, true, false), &newResidencyMask);
        if (newResidencyMask) {
          useResource(srv.SRV->UseBindable(currentChunkId), newResidencyMask);
        }
        break;
      }
      case SM50BindingType::UAV: {
        if constexpr (stage == ShaderType::Vertex) {
          ERR("uav in vertex shader! need to workaround");
          return false;
        }
        // FIXME: currently only pixel shader use uav from OM
        // REFACTOR NEEDED
        // TODO: consider separately handle uav
        if (!UAVBindingSet.test_bound(arg.SM50BindingSlot)) {
          // ERR("expect uav at slot ", arg.SM50BindingSlot,
          //     " but none is bound.");
          write_to_it[arg.StructurePtrOffset] = 0;
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
            write_to_it[arg.StructurePtrOffset + 1] = 0;
          }
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
            write_to_it[arg.StructurePtrOffset + 2] = 0;
          }
          break;
        }
        auto &uav = UAVBindingSet[arg.SM50BindingSlot];
        auto arg_data = uav.View->GetArgumentData(&pTracker);
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
          write_to_it[arg.StructurePtrOffset] = arg_data.buffer();
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
          if (arg_data.requiresContext()) {
            chk->emit([=, ref = uav.View](CommandChunk::context &ctx) {
              write_to_it[arg.StructurePtrOffset] = arg_data.texture(&ctx);
            });
          } else {
            write_to_it[arg.StructurePtrOffset] = arg_data.texture();
          }
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
          write_to_it[arg.StructurePtrOffset + 1] = arg_data.width();
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP) {
          write_to_it[arg.StructurePtrOffset + 1] = std::bit_cast<uint32_t>(arg_data.min_lod());
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET) {
          write_to_it[arg.StructurePtrOffset + 1] = arg_data.element_offset();
        }
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
          auto counter_handle = arg_data.counter();
          if (counter_handle != DXMT_NO_COUNTER) {
            auto counter = ctx_state.cmd_queue.counter_pool.GetCounter(counter_handle);
            chk->emit([counter](CommandChunk::context &ctx) {
              switch (stage) {
              case ShaderType::Vertex:
              case ShaderType::Pixel:
              case ShaderType::Hull:
              case ShaderType::Domain:
                ctx.render_encoder->useResource(counter.Buffer, MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
                break;
              case ShaderType::Compute:
                ctx.compute_encoder->useResource(counter.Buffer, MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
                break;
              case ShaderType::Geometry:
                D3D11_ASSERT(0 && "Not implemented");
                break;
              }
            });
            write_to_it[arg.StructurePtrOffset + 2] = counter.Buffer->gpuAddress() + counter.Offset;
          } else {
            ERR("use uninitialized counter!");
            write_to_it[arg.StructurePtrOffset + 2] = 0;
          };
        }
        UAVBindingSet.clear_dirty(slot);
        pTracker->CheckResidency(
            encoderId,
            GetResidencyMask<Tessellation>(
                stage,
                // FIXME: don't use literal constant...
                (arg.Flags >> 10) & 1, (arg.Flags >> 10) & 2
            ),
            &newResidencyMask
        );
        if (newResidencyMask) {
          useResource(uav.View->UseBindable(currentChunkId), newResidencyMask);
        }
        break;
      }
      }
    }

    /* kArgumentBufferBinding = 30 */
    chk->emit([offset](CommandChunk::context &ctx) {
      if constexpr (stage == ShaderType::Vertex) {
        if constexpr (Tessellation) {
          ctx.render_encoder->setObjectBufferOffset(offset, 30);
        } else {
          ctx.render_encoder->setVertexBufferOffset(offset, 30);
        }
      } else if constexpr (stage == ShaderType::Pixel) {
        ctx.render_encoder->setFragmentBufferOffset(offset, 30);
      } else if constexpr (stage == ShaderType::Compute) {
        ctx.compute_encoder->setBufferOffset(offset, 30);
      } else if constexpr (stage == ShaderType::Hull) {
        ctx.render_encoder->setMeshBufferOffset(offset, 30);
      } else if constexpr (stage == ShaderType::Domain) {
        ctx.render_encoder->setVertexBufferOffset(offset, 30);
      } else {
        D3D11_ASSERT(0 && "Not implemented");
      }
    });
  }
  return true;
}

/**
it's just about vertex buffer
- index buffer and input topology are provided in draw commands
*/
template <>
void
ImmediateContextBase::UpdateVertexBuffer() {
  if (!state_.InputAssembler.InputLayout)
    return;

  uint32_t slot_mask = state_.InputAssembler.InputLayout->GetManagedInputLayout()->input_slot_mask();
  if (slot_mask == 0) // effectively empty input layout
    return;

  auto &VertexBuffers = state_.InputAssembler.VertexBuffers;
  if (!VertexBuffers.any_dirty_masked((uint64_t)slot_mask))
    return;

  struct VERTEX_BUFFER_ENTRY {
    uint64_t buffer_handle;
    uint32_t stride;
    uint32_t length;
  };

  uint32_t max_slot = 32 - __builtin_clz(slot_mask);
  uint32_t num_slots = __builtin_popcount(slot_mask);

  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  auto encoderId = chk->current_encoder_id();
  auto [heap, offset] = chk->allocate_gpu_heap(16 * num_slots, 16);
  VERTEX_BUFFER_ENTRY *entries = (VERTEX_BUFFER_ENTRY *)(((char *)heap->contents()) + offset);
  for (unsigned slot = 0, index = 0; slot < max_slot; slot++) {
    if (!(slot_mask & (1 << slot)))
      continue;
    if (!VertexBuffers.test_bound(slot)) {
      entries[index].buffer_handle = 0;
      entries[index].stride = 0;
      entries[index++].length = 0;
      continue;
    }
    auto &state = VertexBuffers[slot];
    MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
    SIMPLE_RESIDENCY_TRACKER *pTracker;
    auto handle = state.Buffer->GetArgumentData(&pTracker);
    entries[index].buffer_handle = handle.buffer() + state.Offset;
    entries[index].stride = state.Stride;
    entries[index++].length = handle.width() > state.Offset ? handle.width() - state.Offset : 0;
    pTracker->CheckResidency(
        encoderId,
        cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady
            ? GetResidencyMask<true>(ShaderType::Vertex, true, false)
            : GetResidencyMask<false>(ShaderType::Vertex, true, false),
        &newResidencyMask
    );
    if (newResidencyMask) {
      chk->emit([res = Use(state.Buffer), newResidencyMask](CommandChunk::context &ctx) {
        ctx.render_encoder->useResource(
            res.buffer(), GetUsageFromResidencyMask(newResidencyMask), GetStagesFromResidencyMask(newResidencyMask)
        );
      });
    }
  };
  if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady) {
    EmitCommand([offset](CommandChunk::context &ctx) { ctx.render_encoder->setObjectBufferOffset(offset, 16); });
  }
  if (cmdbuf_state == CommandBufferState::RenderPipelineReady) {
    EmitCommand([offset](CommandChunk::context &ctx) { ctx.render_encoder->setVertexBufferOffset(offset, 16); });
  }
}

class MTLD3D11ImmediateContext : public ImmediateContextBase {
public:
  MTLD3D11ImmediateContext(MTLD3D11Device *pDevice, CommandQueue &cmd_queue) :
      ImmediateContextBase(pDevice, ctx_state),
      cmd_queue(cmd_queue),
      ctx_state({cmd_queue}) {}

  ULONG STDMETHODCALLTYPE
  AddRef() override {
    uint32_t refCount = this->refcount++;
    if (unlikely(!refCount))
      this->m_parent->AddRef();

    return refCount + 1;
  }

  ULONG STDMETHODCALLTYPE
  Release() override {
    uint32_t refCount = --this->refcount;
    D3D11_ASSERT(refCount != ~0u && "try to release a 0 reference object");
    if (unlikely(!refCount))
      this->m_parent->Release();

    return refCount;
  }

  HRESULT
  Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags,
      D3D11_MAPPED_SUBRESOURCE *pMappedResource) override {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      D3D11_MAPPED_SUBRESOURCE Out;
      switch (MapType) {
      case D3D11_MAP_READ:
      case D3D11_MAP_WRITE:
      case D3D11_MAP_READ_WRITE:
        return E_INVALIDARG;
      case D3D11_MAP_WRITE_DISCARD: {
        dynamic->RotateBuffer(device);
        auto bind_flag = dynamic->GetBindFlag();
        if (bind_flag & D3D11_BIND_VERTEX_BUFFER) {
          state_.InputAssembler.VertexBuffers.set_dirty();
        }
        if (bind_flag & D3D11_BIND_INDEX_BUFFER) {
          dirty_state.set(DirtyState::IndexBuffer);
        }
        if (bind_flag & D3D11_BIND_CONSTANT_BUFFER) {
          for (auto &stage : state_.ShaderStages) {
            stage.ConstantBuffers.set_dirty();
          }
        }
        if (bind_flag & D3D11_BIND_SHADER_RESOURCE) {
          for (auto &stage : state_.ShaderStages) {
            stage.SRVs.set_dirty();
          }
        }
        Out.pData = dynamic->GetMappedMemory(&Out.RowPitch, &Out.DepthPitch);
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        Out.pData = dynamic->GetMappedMemory(&Out.RowPitch, &Out.DepthPitch);
        break;
      }
      }
      if (pMappedResource) {
        *pMappedResource = Out;
      }
      return S_OK;
    }
    if (auto staging = com_cast<IMTLD3D11Staging>(pResource)) {
      while (true) {
        auto coh = cmd_queue.CoherentSeqId();
        auto ret = staging->TryMap(Subresource, coh, MapType, pMappedResource);
        if (ret < 0) {
          return E_FAIL;
        }
        if (ret == 0) {
          TRACE("staging map ready");
          return S_OK;
        }
        if (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT) {
          return DXGI_ERROR_WAS_STILL_DRAWING;
        }
        // even it's in a while loop
        // only the first flush will have effect
        // and the following calls are essentially no-op
        Flush();
        TRACE("staging map block");
        cmd_queue.FIXME_YieldUntilCoherenceBoundaryUpdate(coh);
      };
    };
    D3D11_ASSERT(0 && "unknown mapped resource (USAGE_DEFAULT?)");
    IMPLEMENT_ME;
  }

  void
  Unmap(ID3D11Resource *pResource, UINT Subresource) override {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      return;
    }
    if (auto staging = com_cast<IMTLD3D11Staging>(pResource)) {
      staging->Unmap(Subresource);
      return;
    };
    D3D11_ASSERT(0 && "unknown mapped resource (USAGE_DEFAULT?)");
    IMPLEMENT_ME;
  }

  void
  Begin(ID3D11Asynchronous *pAsync) override {
    // in theory pAsync could be any of them: { Query, Predicate, Counter }.
    // However `Predicate` and `Counter` are not supported at all
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      auto query = static_cast<IMTLD3DOcclusionQuery *>(pAsync);
      query->Begin(cmd_queue.CurrentSeqId(), vro_state.getNextReadOffset());
      active_occlusion_queries.insert(query);
      // FIXME: remove duplication
      pending_occlusion_queries.push_back(query);
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      break;
    }
  }

  // See Begin()
  void
  End(ID3D11Asynchronous *pAsync) override {
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      ((IMTLD3DEventQuery *)pAsync)->Issue(cmd_queue.EncodedWorkFinishAt());
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      auto query = static_cast<IMTLD3DOcclusionQuery *>(pAsync);
      if (active_occlusion_queries.erase(query) == 0)
        return; // invalid
      query->End(cmd_queue.CurrentSeqId(), vro_state.getNextReadOffset());
      promote_flush = true;
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      break;
    }
  }

  void
  FlushInternal(
      std::function<void(MTL::CommandBuffer *)> &&beforeCommit, std::function<void(void)> &&onFinished, bool present_
  ) final {
    if (InvalidateCurrentPass(true)) {
      D3D11_ASSERT(0 && "unexpected commit");
    }
    cmd_queue.CurrentChunk()->emit([bc = std::move(beforeCommit), _ = DestructorWrapper(
                                                                      [of = std::move(onFinished)]() { of(); }, nullptr
                                                                  )](CommandChunk::context &ctx) { bc(ctx.cmdbuf); });
    Commit();
    if (present_) {
      cmd_queue.PresentBoundary();
    }
  }

  HRESULT
  GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags) override {
    if (!pAsync || (DataSize && !pData))
      return E_INVALIDARG;

    // Allow dataSize to be zero
    if (DataSize && DataSize != pAsync->GetDataSize())
      return E_INVALIDARG;

    if (GetDataFlags != D3D11_ASYNC_GETDATA_DONOTFLUSH) {
      Flush();
    }

    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT: {
      BOOL null_data;
      BOOL *data_ptr = pData ? (BOOL *)pData : &null_data;
      return ((IMTLD3DEventQuery *)pAsync)->GetData(data_ptr, cmd_queue.CoherentSeqId());
    }
    case D3D11_QUERY_OCCLUSION: {
      uint64_t null_data;
      uint64_t *data_ptr = pData ? (uint64_t *)pData : &null_data;
      return ((IMTLD3DOcclusionQuery *)pAsync)->GetData(data_ptr);
    }
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      BOOL null_data;
      BOOL *data_ptr = pData ? (BOOL *)pData : &null_data;
      return ((IMTLD3DOcclusionQuery *)pAsync)->GetData(data_ptr);
    }
    case D3D11_QUERY_TIMESTAMP: {
      if (pData) {
        (*static_cast<uint64_t *>(pData)) = 0;
      }
      return S_OK;
    }
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      if (pData) {
        (*static_cast<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT *>(pData)) = {1, TRUE};
      }
      return S_OK;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      return E_FAIL;
    }
  }

  void
  Flush() override {
    if (cmd_queue.CurrentChunk()->has_no_work_encoded_yet()) {
      return;
    }
    promote_flush = true;
    InvalidateCurrentPass();
  }

  void
  ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState) override {
    auto cmd_list = static_cast<MTLD3D11CommandList *>(pCommandList);

    if (cmd_list->Noop())
      return;

    CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();

    std::vector<ArgumentData> argument_table(cmd_list->num_argument_data);
    std::vector<BindingRef> bindingref_table(cmd_list->num_bindingref);

    auto [heap, offset] = chk->allocate_gpu_heap(cmd_list->gpu_arugment_heap_offset, 16);

    uint64_t visibility_offset = vro_state.preserveCount(cmd_list->num_visibility_result);

    chk->mark_pass(EncoderKind::ExecuteCommandList);

    MTLD3D11CommandList::EventContext ctx{
        cmd_queue,
        device,
        (uint64_t *)((char *)heap->contents() + offset),
        argument_table,
        bindingref_table,
        visibility_offset,
        pending_occlusion_queries
    };
    cmd_list->ProcessEvents(ctx);

    InvalidateCurrentPass();

    chk->emit([cmd_list = Com(cmd_list), table = std::move(bindingref_table), offset, visibility_offset](auto &ctx) {
      const BindingRef *old_lut;
      BindingRef::SetLookupTable(table.data(), &old_lut);
      auto offset_base = ctx.offset_base;
      auto visibility_offset_base = ctx.visibility_offset_base;
      ctx.offset_base = offset;
      ctx.visibility_offset_base = visibility_offset;
      cmd_list->EncodeCommands(ctx);
      ctx.offset_base = offset_base;
      ctx.visibility_offset_base = visibility_offset_base;
      BindingRef::SetLookupTable(old_lut, nullptr);
    });

    if (!RestoreContextState)
      ClearState();

    if (cmd_list->promote_flush)
      Flush();
  }

  HRESULT
  FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList) override {
    return DXGI_ERROR_INVALID_CALL;
  }

  void
  CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView)
      override {
    if (auto dst_bind = com_cast<IMTLBindable>(pDstBuffer)) {
      if (auto uav = com_cast<IMTLD3D11UnorderedAccessView>(pSrcView)) {
        auto counter_handle = uav->SwapCounter(DXMT_NO_COUNTER);
        if (counter_handle == DXMT_NO_COUNTER) {
          return;
        }
        auto counter = cmd_queue.counter_pool.GetCounter(counter_handle);
        EmitBlitCommand<true>([counter, DstAlignedByteOffset,
                               dst = Use(dst_bind)](MTL::BlitCommandEncoder *enc, auto &ctx) {
          enc->copyFromBuffer(counter.Buffer, counter.Offset, dst.buffer(), DstAlignedByteOffset, 4);
        });
      }
    }
  }

  D3D11_DEVICE_CONTEXT_TYPE
  GetType() override {
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }

  virtual void
  WaitUntilGPUIdle() override {
    uint64_t seq = cmd_queue.CurrentSeqId();
    FlushInternal([](auto) {}, []() {}, false);
    cmd_queue.WaitCPUFence(seq);
  };

  class VisibilityResultReadback {
  public:
    VisibilityResultReadback(
        uint64_t seq_id, uint64_t num_results, std::vector<Com<IMTLD3DOcclusionQuery>> &&queries, CommandChunk *chk
    ) :
        seq_id(seq_id),
        num_results(num_results),
        queries(std::move(queries)),
        chk(chk) {}
    ~VisibilityResultReadback() {
      for (auto query : queries) {
        query->Issue(seq_id, (uint64_t *)chk->visibility_result_heap->contents(), num_results);
      }
    }

    VisibilityResultReadback(const VisibilityResultReadback &) = delete;
    VisibilityResultReadback(VisibilityResultReadback &&) = default;

    uint64_t seq_id;
    uint64_t num_results;
    std::vector<Com<IMTLD3DOcclusionQuery>> queries;
    CommandChunk *chk;
  };

  void
  Commit() override {
    promote_flush = false;
    D3D11_ASSERT(cmdbuf_state == CommandBufferState::Idle);
    CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
    if (auto count = vro_state.reset()) {
      if (count > kOcclusionSampleCount && (count << 3) > chk->visibility_result_heap->length()) {
        auto old_heap = std::move(chk->visibility_result_heap);
        chk->visibility_result_heap = transfer(device->GetMTLDevice()->newBuffer(count << 3, old_heap->resourceOptions()));
      }
      chk->emit([_ = VisibilityResultReadback(
                     cmd_queue.CurrentSeqId(), count, std::move(pending_occlusion_queries), chk
                 )](auto) {});
      pending_occlusion_queries = {};
      if (!active_occlusion_queries.empty()) {
        WARN("A chunk is commit while the visibility result is still queried");
        for (auto s : active_occlusion_queries) {
          pending_occlusion_queries.push_back(s);
        }
      }
    }
    /** FIXME: it might be unnecessary? */
    chk->emit([device = Com<MTLD3D11Device, false>(device)](CommandChunk::context &ctx) {});
    ctx_state.cmd_queue.CommitCurrentChunk();
  };

private:
  std::vector<Com<IMTLD3DOcclusionQuery>> pending_occlusion_queries;
  CommandQueue &cmd_queue;
  ContextInternalState ctx_state;
  std::atomic<uint32_t> refcount = 0;
};

std::unique_ptr<MTLD3D11DeviceContextBase>
InitializeImmediateContext(MTLD3D11Device *pDevice, CommandQueue &cmd_queue) {
  return std::make_unique<MTLD3D11ImmediateContext>(pDevice, cmd_queue);
}

}; // namespace dxmt