#include "d3d11_device.hpp"
#include "d3d11_private.h"
#include "d3d11_context_impl.cpp"
#include "dxmt_command_queue.hpp"

namespace dxmt {

struct DeferredContextInternalState {};

using DeferredContextBase = MTLD3D11DeviceContextImplBase<DeferredContextInternalState>;

template <>
template <typename Fn>
void
DeferredContextBase::EmitCommand(Fn &&fn) {
  IMPLEMENT_ME;
}

template <>
BindingRef
DeferredContextBase::Use(IMTLBindable *bindable) {
  IMPLEMENT_ME;
}

template <>
BindingRef
DeferredContextBase::Use(IMTLD3D11RenderTargetView *rtv) {
  IMPLEMENT_ME;
}

template <>
BindingRef
DeferredContextBase::Use(IMTLD3D11DepthStencilView *dsv) {
  IMPLEMENT_ME;
}

template <>
ENCODER_INFO *
DeferredContextBase::GetLastEncoder() {
  IMPLEMENT_ME;
}
template <>
ENCODER_CLEARPASS_INFO *
DeferredContextBase::MarkClearPass() {
  IMPLEMENT_ME;
}
template <>
ENCODER_RENDER_INFO *
DeferredContextBase::MarkRenderPass() {
  IMPLEMENT_ME;
}
template <>
ENCODER_INFO *
DeferredContextBase::MarkPass(EncoderKind kind) {
  IMPLEMENT_ME;
}

template <>
template <typename T>
moveonly_list<T>
DeferredContextBase::AllocateCommandData(size_t n) {
  IMPLEMENT_ME;
}

template <>
void
DeferredContextBase::UpdateUAVCounter(IMTLD3D11UnorderedAccessView *uav, uint32_t value) {
  IMPLEMENT_ME;
};

template <>
std::tuple<void *, MTL::Buffer *, uint64_t>
DeferredContextBase::AllocateStagingBuffer(size_t size, size_t alignment) {
  IMPLEMENT_ME;
}

template <>
bool
DeferredContextBase::UseCopyDestination(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  IMPLEMENT_ME;
}
template <>
bool
DeferredContextBase::UseCopySource(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  IMPLEMENT_ME;
}

template <>
BindingRef
DeferredContextBase::UseImmediate(IMTLBindable *bindable) {
  // never
  return BindingRef();
}

template <>
template <ShaderType stage, bool Tessellation>
bool
DeferredContextBase::UploadShaderStageResourceBinding() {
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

  return false; // TODO

  //   auto currentChunkId = ctx_state.cmd_queue.CurrentSeqId();

  //   auto ConstantBufferCount = reflection->NumConstantBuffers;
  //   auto BindingCount = reflection->NumArguments;
  //   auto ArgumentTableQwords = reflection->ArgumentTableQwords;
  //   CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  //   auto encoderId = chk->current_encoder_id();

  //   auto useResource = [&](BindingRef &&res, MTL_BINDABLE_RESIDENCY_MASK residencyMask) {
  //     chk->emit([res = std::move(res), residencyMask](CommandChunk::context &ctx) {
  //       switch (stage) {
  //       case ShaderType::Vertex:
  //       case ShaderType::Pixel:
  //       case ShaderType::Hull:
  //       case ShaderType::Domain:
  //         ctx.render_encoder->useResource(
  //             res.resource(&ctx), GetUsageFromResidencyMask(residencyMask), GetStagesFromResidencyMask(residencyMask)
  //         );
  //         if (res.withBackedBuffer()) {
  //           ctx.render_encoder->useResource(
  //               res.buffer(), GetUsageFromResidencyMask(residencyMask), GetStagesFromResidencyMask(residencyMask)
  //           );
  //         }
  //         break;
  //       case ShaderType::Compute:
  //         ctx.compute_encoder->useResource(res.resource(&ctx), GetUsageFromResidencyMask(residencyMask));
  //         if (res.withBackedBuffer()) {
  //           ctx.compute_encoder->useResource(res.buffer(), GetUsageFromResidencyMask(residencyMask));
  //         }
  //         break;
  //       case ShaderType::Geometry:
  //         D3D11_ASSERT(0 && "Not implemented");
  //         break;
  //       }
  //     });
  //   };
  //   if (ConstantBufferCount && dirty_cbuffer) {

  //     auto [heap, offset] = chk->allocate_gpu_heap(ConstantBufferCount << 3, 16);
  //     uint64_t *write_to_it = chk->gpu_argument_heap_contents + (offset >> 3);

  //     for (unsigned i = 0; i < ConstantBufferCount; i++) {
  //       auto &arg = reflection->ConstantBuffers[i];
  //       auto slot = arg.SM50BindingSlot;
  //       MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
  //       SIMPLE_RESIDENCY_TRACKER *pTracker;
  //       switch (arg.Type) {
  //       case SM50BindingType::ConstantBuffer: {
  //         if (!ShaderStage.ConstantBuffers.test_bound(slot)) {
  //           ERR("expect constant buffer at slot ", slot, " but none is bound.");
  //           return false;
  //         }
  //         auto &cbuf = ShaderStage.ConstantBuffers[slot];
  //         auto argbuf = cbuf.Buffer->GetArgumentData(&pTracker);
  //         write_to_it[arg.StructurePtrOffset] = argbuf.buffer() + (cbuf.FirstConstant << 4);
  //         ShaderStage.ConstantBuffers.clear_dirty(slot);
  //         pTracker->CheckResidency(encoderId, GetResidencyMask<Tessellation>(stage, true, false), &newResidencyMask);
  //         if (newResidencyMask) {
  //           useResource(Use(cbuf.Buffer), newResidencyMask);
  //         }
  //         break;
  //       }
  //       default:
  //         D3D11_ASSERT(0 && "unreachable");
  //       }
  //     }

  //     /* kConstantBufferTableBinding = 29 */
  //     chk->emit([offset](CommandChunk::context &ctx) {
  //       if constexpr (stage == ShaderType::Vertex) {
  //         if constexpr (Tessellation) {
  //           ctx.render_encoder->setObjectBufferOffset(offset, 29);
  //         } else {
  //           ctx.render_encoder->setVertexBufferOffset(offset, 29);
  //         }
  //       } else if constexpr (stage == ShaderType::Pixel) {
  //         ctx.render_encoder->setFragmentBufferOffset(offset, 29);
  //       } else if constexpr (stage == ShaderType::Compute) {
  //         ctx.compute_encoder->setBufferOffset(offset, 29);
  //       } else if constexpr (stage == ShaderType::Hull) {
  //         ctx.render_encoder->setMeshBufferOffset(offset, 29);
  //       } else if constexpr (stage == ShaderType::Domain) {
  //         ctx.render_encoder->setVertexBufferOffset(offset, 29);
  //       } else {
  //         D3D11_ASSERT(0 && "Not implemented");
  //       }
  //     });
  //   }

  //   if (BindingCount && (dirty_sampler || dirty_srv || dirty_uav)) {
  //     auto [heap, offset] = chk->allocate_gpu_heap(ArgumentTableQwords * 8, 16);
  //     uint64_t *write_to_it = chk->gpu_argument_heap_contents + (offset >> 3);

  //     for (unsigned i = 0; i < BindingCount; i++) {
  //       auto &arg = reflection->Arguments[i];
  //       auto slot = arg.SM50BindingSlot;
  //       MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
  //       SIMPLE_RESIDENCY_TRACKER *pTracker;
  //       switch (arg.Type) {
  //       case SM50BindingType::ConstantBuffer: {
  //         D3D11_ASSERT(0 && "unreachable");
  //       }
  //       case SM50BindingType::Sampler: {
  //         if (!ShaderStage.Samplers.test_bound(slot)) {
  //           ERR("expect sample at slot ", slot, " but none is bound.");
  //           return false;
  //         }
  //         auto &sampler = ShaderStage.Samplers[slot];
  //         write_to_it[arg.StructurePtrOffset] = sampler.Sampler->GetArgumentHandle();
  //         write_to_it[arg.StructurePtrOffset + 1] = (uint64_t)std::bit_cast<uint32_t>(sampler.Sampler->GetLODBias());
  //         ShaderStage.Samplers.clear_dirty(slot);
  //         break;
  //       }
  //       case SM50BindingType::SRV: {
  //         if (!ShaderStage.SRVs.test_bound(slot)) {
  //           // TODO: debug only
  //           // ERR("expect shader resource at slot ", slot, " but none is
  //           // bound.");
  //           write_to_it[arg.StructurePtrOffset] = 0;
  //           write_to_it[arg.StructurePtrOffset + 1] = 0;
  //           break;
  //         }
  //         auto &srv = ShaderStage.SRVs[slot];
  //         auto arg_data = srv.SRV->GetArgumentData(&pTracker);
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
  //           write_to_it[arg.StructurePtrOffset] = arg_data.buffer();
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
  //           if (arg_data.requiresContext()) {
  //             chk->emit([=, ref = srv.SRV](CommandChunk::context &ctx) {
  //               write_to_it[arg.StructurePtrOffset] = arg_data.texture(&ctx);
  //             });
  //           } else {
  //             write_to_it[arg.StructurePtrOffset] = arg_data.texture();
  //           }
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
  //           write_to_it[arg.StructurePtrOffset + 1] = arg_data.width();
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP) {
  //           write_to_it[arg.StructurePtrOffset + 1] = std::bit_cast<uint32_t>(arg_data.min_lod());
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET) {
  //           write_to_it[arg.StructurePtrOffset + 1] = arg_data.element_offset();
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
  //           D3D11_ASSERT(0 && "srv can not have counter associated");
  //         }
  //         ShaderStage.SRVs.clear_dirty(slot);
  //         pTracker->CheckResidency(encoderId, GetResidencyMask<Tessellation>(stage, true, false), &newResidencyMask);
  //         if (newResidencyMask) {
  //           useResource(srv.SRV->UseBindable(currentChunkId), newResidencyMask);
  //         }
  //         break;
  //       }
  //       case SM50BindingType::UAV: {
  //         if constexpr (stage == ShaderType::Vertex) {
  //           ERR("uav in vertex shader! need to workaround");
  //           return false;
  //         }
  //         // FIXME: currently only pixel shader use uav from OM
  //         // REFACTOR NEEDED
  //         // TODO: consider separately handle uav
  //         if (!UAVBindingSet.test_bound(arg.SM50BindingSlot)) {
  //           // ERR("expect uav at slot ", arg.SM50BindingSlot,
  //           //     " but none is bound.");
  //           write_to_it[arg.StructurePtrOffset] = 0;
  //           if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
  //             write_to_it[arg.StructurePtrOffset + 1] = 0;
  //           }
  //           if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
  //             write_to_it[arg.StructurePtrOffset + 2] = 0;
  //           }
  //           break;
  //         }
  //         auto &uav = UAVBindingSet[arg.SM50BindingSlot];
  //         auto arg_data = uav.View->GetArgumentData(&pTracker);
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
  //           write_to_it[arg.StructurePtrOffset] = arg_data.buffer();
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
  //           if (arg_data.requiresContext()) {
  //             chk->emit([=, ref = uav.View](CommandChunk::context &ctx) {
  //               write_to_it[arg.StructurePtrOffset] = arg_data.texture(&ctx);
  //             });
  //           } else {
  //             write_to_it[arg.StructurePtrOffset] = arg_data.texture();
  //           }
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
  //           write_to_it[arg.StructurePtrOffset + 1] = arg_data.width();
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP) {
  //           write_to_it[arg.StructurePtrOffset + 1] = std::bit_cast<uint32_t>(arg_data.min_lod());
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET) {
  //           write_to_it[arg.StructurePtrOffset + 1] = arg_data.element_offset();
  //         }
  //         if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
  //           auto counter_handle = arg_data.counter();
  //           if (counter_handle != DXMT_NO_COUNTER) {
  //             auto counter = ctx_state.cmd_queue.counter_pool.GetCounter(counter_handle);
  //             chk->emit([counter](CommandChunk::context &ctx) {
  //               switch (stage) {
  //               case ShaderType::Vertex:
  //               case ShaderType::Pixel:
  //               case ShaderType::Hull:
  //               case ShaderType::Domain:
  //                 ctx.render_encoder->useResource(counter.Buffer, MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
  //                 break;
  //               case ShaderType::Compute:
  //                 ctx.compute_encoder->useResource(counter.Buffer, MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
  //                 break;
  //               case ShaderType::Geometry:
  //                 D3D11_ASSERT(0 && "Not implemented");
  //                 break;
  //               }
  //             });
  //             write_to_it[arg.StructurePtrOffset + 2] = counter.Buffer->gpuAddress() + counter.Offset;
  //           } else {
  //             ERR("use uninitialized counter!");
  //             write_to_it[arg.StructurePtrOffset + 2] = 0;
  //           };
  //         }
  //         UAVBindingSet.clear_dirty(slot);
  //         pTracker->CheckResidency(
  //             encoderId,
  //             GetResidencyMask<Tessellation>(
  //                 stage,
  //                 // FIXME: don't use literal constant...
  //                 (arg.Flags >> 10) & 1, (arg.Flags >> 10) & 2
  //             ),
  //             &newResidencyMask
  //         );
  //         if (newResidencyMask) {
  //           useResource(uav.View->UseBindable(currentChunkId), newResidencyMask);
  //         }
  //         break;
  //       }
  //       }
  //     }

  //     /* kArgumentBufferBinding = 30 */
  //     chk->emit([offset](CommandChunk::context &ctx) {
  //       if constexpr (stage == ShaderType::Vertex) {
  //         if constexpr (Tessellation) {
  //           ctx.render_encoder->setObjectBufferOffset(offset, 30);
  //         } else {
  //           ctx.render_encoder->setVertexBufferOffset(offset, 30);
  //         }
  //       } else if constexpr (stage == ShaderType::Pixel) {
  //         ctx.render_encoder->setFragmentBufferOffset(offset, 30);
  //       } else if constexpr (stage == ShaderType::Compute) {
  //         ctx.compute_encoder->setBufferOffset(offset, 30);
  //       } else if constexpr (stage == ShaderType::Hull) {
  //         ctx.render_encoder->setMeshBufferOffset(offset, 30);
  //       } else if constexpr (stage == ShaderType::Domain) {
  //         ctx.render_encoder->setVertexBufferOffset(offset, 30);
  //       } else {
  //         D3D11_ASSERT(0 && "Not implemented");
  //       }
  //     });
  //   }
  //   return true;
}

/**
it's just about vertex buffer
- index buffer and input topology are provided in draw commands
*/
template <>
void
DeferredContextBase::UpdateVertexBuffer() {
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

  //   uint32_t max_slot = 32 - __builtin_clz(slot_mask);
  //   uint32_t num_slots = __builtin_popcount(slot_mask);

  //   CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  //   auto encoderId = chk->current_encoder_id();
  //   auto [heap, offset] = chk->allocate_gpu_heap(16 * num_slots, 16);
  //   VERTEX_BUFFER_ENTRY *entries = (VERTEX_BUFFER_ENTRY *)(((char *)heap->contents()) + offset);
  //   for (unsigned slot = 0, index = 0; slot < max_slot; slot++) {
  //     if (!(slot_mask & (1 << slot)))
  //       continue;
  //     if (!VertexBuffers.test_bound(slot)) {
  //       entries[index].buffer_handle = 0;
  //       entries[index].stride = 0;
  //       entries[index++].length = 0;
  //       continue;
  //     }
  //     auto &state = VertexBuffers[slot];
  //     MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
  //     SIMPLE_RESIDENCY_TRACKER *pTracker;
  //     auto handle = state.Buffer->GetArgumentData(&pTracker);
  //     entries[index].buffer_handle = handle.buffer() + state.Offset;
  //     entries[index].stride = state.Stride;
  //     entries[index++].length = handle.width() > state.Offset ? handle.width() - state.Offset : 0;
  //     pTracker->CheckResidency(
  //         encoderId,
  //         cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady
  //             ? GetResidencyMask<true>(ShaderType::Vertex, true, false)
  //             : GetResidencyMask<false>(ShaderType::Vertex, true, false),
  //         &newResidencyMask
  //     );
  //     if (newResidencyMask) {
  //       chk->emit([res = Use(state.Buffer), newResidencyMask](CommandChunk::context &ctx) {
  //         ctx.render_encoder->useResource(
  //             res.buffer(), GetUsageFromResidencyMask(newResidencyMask), GetStagesFromResidencyMask(newResidencyMask)
  //         );
  //       });
  //     }
  //   };
  //   if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady) {
  //     EmitCommand([offset](CommandChunk::context &ctx) { ctx.render_encoder->setObjectBufferOffset(offset, 16); });
  //   }
  //   if (cmdbuf_state == CommandBufferState::RenderPipelineReady) {
  //     EmitCommand([offset](CommandChunk::context &ctx) { ctx.render_encoder->setVertexBufferOffset(offset, 16); });
  //   }
}

class MTLD3D11DeferredContext : public DeferredContextBase {
public:
  MTLD3D11DeferredContext(MTLD3D11Device *pDevice) : DeferredContextBase(pDevice, ctx_state), ctx_state() {}

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
        IMPLEMENT_ME;
        // dynamic->RotateBuffer(device);
        // auto bind_flag = dynamic->GetBindFlag();
        // if (bind_flag & D3D11_BIND_VERTEX_BUFFER) {
        //   state_.InputAssembler.VertexBuffers.set_dirty();
        // }
        // if (bind_flag & D3D11_BIND_INDEX_BUFFER) {
        //   dirty_state.set(DirtyState::IndexBuffer);
        // }
        // if (bind_flag & D3D11_BIND_CONSTANT_BUFFER) {
        //   for (auto &stage : state_.ShaderStages) {
        //     stage.ConstantBuffers.set_dirty();
        //   }
        // }
        // if (bind_flag & D3D11_BIND_SHADER_RESOURCE) {
        //   for (auto &stage : state_.ShaderStages) {
        //     stage.SRVs.set_dirty();
        //   }
        // }
        // Out.pData = dynamic->GetMappedMemory(&Out.RowPitch, &Out.DepthPitch);
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        IMPLEMENT_ME;
        // Out.pData = dynamic->GetMappedMemory(&Out.RowPitch, &Out.DepthPitch);
        break;
      }
      }
      if (pMappedResource) {
        *pMappedResource = Out;
      }
      return S_OK;
    }
    return E_FAIL;
  }

  void
  Unmap(ID3D11Resource *pResource, UINT Subresource) override {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      return;
    }
    // if (auto staging = com_cast<IMTLD3D11Staging>(pResource)) {
    //   staging->Unmap(Subresource);
    //   return;
    // };
    // D3D11_ASSERT(0 && "unknown mapped resource (USAGE_DEFAULT?)");
    IMPLEMENT_ME;
  }

  void
  Begin(ID3D11Asynchronous *pAsync) override {
    // in theory pAsync could be any of them: { Query, Predicate, Counter }.
    // However `Predicate` and `Counter` are not supported at all
    // D3D11_QUERY_DESC desc;
    // ((ID3D11Query *)pAsync)->GetDesc(&desc);
    // switch (desc.Query) {
    // case D3D11_QUERY_EVENT:
    //   break;
    // case D3D11_QUERY_OCCLUSION: {
    //   if (auto observer = ((IMTLD3DOcclusionQuery *)pAsync)->Begin(NextOcclusionQuerySeq())) {
    //     cmd_queue.RegisterVisibilityResultObserver(observer);
    //   }
    //   break;
    // }
    // case D3D11_QUERY_TIMESTAMP:
    // case D3D11_QUERY_TIMESTAMP_DISJOINT: {
    //   // ignore
    //   break;
    // }
    // default:
    //   ERR("Unknown query type ", desc.Query);
    //   break;
    // }
    IMPLEMENT_ME;
  }

  // See Begin()
  void
  End(ID3D11Asynchronous *pAsync) override {
    IMPLEMENT_ME;
    // D3D11_QUERY_DESC desc;
    // ((ID3D11Query *)pAsync)->GetDesc(&desc);
    // switch (desc.Query) {
    // case D3D11_QUERY_EVENT:
    //   ((IMTLD3DEventQuery *)pAsync)->Issue(cmd_queue.EncodedWorkFinishAt());
    //   break;
    // case D3D11_QUERY_OCCLUSION: {
    //   ((IMTLD3DOcclusionQuery *)pAsync)->End(NextOcclusionQuerySeq());
    //   promote_flush = true;
    //   break;
    // }
    // case D3D11_QUERY_TIMESTAMP:
    // case D3D11_QUERY_TIMESTAMP_DISJOINT: {
    //   // ignore
    //   break;
    // }
    // default:
    //   ERR("Unknown query type ", desc.Query);
    //   break;
    // }
  }

  void
  FlushInternal(
      std::function<void(MTL::CommandBuffer *)> &&beforeCommit, std::function<void(void)> &&onFinished, bool present_
  ) final {
    // nop
  }

  HRESULT
  GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags) override {
    return E_FAIL;
  }

  void
  Flush() override {
    // nop
  }

  void
  ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState) override{IMPLEMENT_ME}

  HRESULT FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList) override {
    IMPLEMENT_ME;
  }

  void
  CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView) override{
      IMPLEMENT_ME
  }

  D3D11_DEVICE_CONTEXT_TYPE GetType() override {
    return D3D11_DEVICE_CONTEXT_DEFERRED;
  }

  virtual void WaitUntilGPUIdle() override {
    // nop
  };

  uint64_t
  NextOcclusionQuerySeq() override {
    IMPLEMENT_ME;
  };

  void
  BumpOcclusionQueryOffset() override {
    IMPLEMENT_ME;
  }

  void Commit() override {
    // nop
  };

private:
  DeferredContextInternalState ctx_state;
  std::atomic<uint32_t> refcount = 0;
};

std::unique_ptr<MTLD3D11DeviceContextBase>
InitializeDeferredContext(MTLD3D11Device *pDevice) {
  return std::make_unique<MTLD3D11DeferredContext>(pDevice);
}

}; // namespace dxmt