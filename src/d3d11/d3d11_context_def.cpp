#include "d3d11_device.hpp"
#include "d3d11_private.h"
#include "d3d11_query.hpp"
#define __DXMT_DISABLE_OCCLUSION_QUERY__ 1
#include "d3d11_context_impl.cpp"
#include "dxmt_command_queue.hpp"

namespace dxmt {

struct DeferredContextInternalState {
  Com<MTLD3D11CommandList> current_cmdlist;
};

using DeferredContextBase = MTLD3D11DeviceContextImplBase<DeferredContextInternalState>;

template <>
template <typename Fn>
void
DeferredContextBase::EmitCommand(Fn &&fn) {
  ctx_state.current_cmdlist->EmitCommand(std::forward<Fn>(fn));
}

template <>
BindingRef
DeferredContextBase::Use(IMTLBindable *bindable) {
  return ctx_state.current_cmdlist->Use(bindable);
}

template <>
BindingRef
DeferredContextBase::Use(IMTLD3D11RenderTargetView *rtv) {
  return ctx_state.current_cmdlist->Use(rtv);
}

template <>
BindingRef
DeferredContextBase::Use(IMTLD3D11DepthStencilView *dsv) {
  return ctx_state.current_cmdlist->Use(dsv);
}

template <>
BindingRef
DeferredContextBase::Use(IMTLDynamicBuffer *dynamic_buffer) {
  return ctx_state.current_cmdlist->Use(dynamic_buffer);
}

template <>
ENCODER_INFO *
DeferredContextBase::GetLastEncoder() {
  return ctx_state.current_cmdlist->GetLastEncoder();
}
template <>
ENCODER_CLEARPASS_INFO *
DeferredContextBase::MarkClearPass() {
  return ctx_state.current_cmdlist->MarkClearPass();
}
template <>
ENCODER_RENDER_INFO *
DeferredContextBase::MarkRenderPass() {
  return ctx_state.current_cmdlist->MarkRenderPass();
}
template <>
ENCODER_INFO *
DeferredContextBase::MarkPass(EncoderKind kind) {
  return ctx_state.current_cmdlist->MarkPass(kind);
}

template <>
template <typename T>
moveonly_list<T>
DeferredContextBase::AllocateCommandData(size_t n) {
  return ctx_state.current_cmdlist->AllocateCommandData<T>(n);
}

template <>
void
DeferredContextBase::UpdateUAVCounter(IMTLD3D11UnorderedAccessView *uav, uint32_t value) {
  IMPLEMENT_ME;
};

template <>
std::tuple<void *, MTL::Buffer *, uint64_t>
DeferredContextBase::AllocateStagingBuffer(size_t size, size_t alignment) {
  return ctx_state.current_cmdlist->AllocateStagingBuffer(size, alignment);
}

template <>
bool
DeferredContextBase::UseCopyDestination(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  pResource->UseCopyDestination(Subresource, 0, pBuffer, pBytesPerRow, pBytesPerImage);
  ctx_state.current_cmdlist->EmitEvent([staging = Com(pResource), Subresource,
                                        buffer_def = pBuffer->Buffer](auto &ctx) {
    MTL_STAGING_RESOURCE buffer_imm;
    uint32_t __, ___;
    staging->UseCopyDestination(Subresource, ctx.cmd_queue.CurrentSeqId(), &buffer_imm, &__, &___);
    D3D11_ASSERT(buffer_def == buffer_imm.Buffer);
  });
  return true;
}

template <>
bool
DeferredContextBase::UseCopySource(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  pResource->UseCopySource(Subresource, 0, pBuffer, pBytesPerRow, pBytesPerImage);
  ctx_state.current_cmdlist->EmitEvent([staging = Com(pResource), Subresource,
                                        buffer_def = pBuffer->Buffer](auto &ctx) {
    MTL_STAGING_RESOURCE buffer_imm;
    uint32_t __, ___;
    staging->UseCopySource(Subresource, ctx.cmd_queue.CurrentSeqId(), &buffer_imm, &__, &___);
    D3D11_ASSERT(buffer_def == buffer_imm.Buffer);
  });
  return true;
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

  auto &cmd_list = ctx_state.current_cmdlist;
  auto ConstantBufferCount = reflection->NumConstantBuffers;
  auto BindingCount = reflection->NumArguments;
  auto ArgumentTableQwords = reflection->ArgumentTableQwords;
  auto encoderId = cmd_list->GetLastEncoder()->encoder_id;

  auto useResource = [&](IMTLBindable *bindable, MTL_BINDABLE_RESIDENCY_MASK residencyMask) {
    cmd_list->EmitCommand([res = Use(bindable), residencyMask](CommandChunk::context &ctx) {
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
    auto offset = cmd_list->ReserveGpuHeap(ConstantBufferCount << 3, 16);

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
        auto index = cmd_list->GetArgumentDataId(cbuf.Buffer.ptr(), &pTracker);
        cmd_list->EmitEvent([offset, index, arg_offset = arg.StructurePtrOffset,
                             first_constant = cbuf.FirstConstant](auto &ctx) {
          uint64_t *write_to_it = ctx.gpu_argument_heap_contents + (offset >> 3);
          auto &argbuf = ctx.argument_table[index];
          write_to_it[arg_offset] = argbuf.buffer() + (first_constant << 4);
        });
        ShaderStage.ConstantBuffers.clear_dirty(slot);
        pTracker->CheckResidency(encoderId, GetResidencyMask<Tessellation>(stage, true, false), &newResidencyMask);
        if (newResidencyMask) {
          useResource(cbuf.Buffer.ptr(), newResidencyMask);
        }
        break;
      }
      default:
        D3D11_ASSERT(0 && "unreachable");
      }
    }

    /* kConstantBufferTableBinding = 29 */
    cmd_list->EmitCommand([offset](CommandChunk::context &ctx) {
      if constexpr (stage == ShaderType::Vertex) {
        if constexpr (Tessellation) {
          ctx.render_encoder->setObjectBufferOffset(offset + ctx.offset_base, 29);
        } else {
          ctx.render_encoder->setVertexBufferOffset(offset + ctx.offset_base, 29);
        }
      } else if constexpr (stage == ShaderType::Pixel) {
        ctx.render_encoder->setFragmentBufferOffset(offset + ctx.offset_base, 29);
      } else if constexpr (stage == ShaderType::Compute) {
        ctx.compute_encoder->setBufferOffset(offset + ctx.offset_base, 29);
      } else if constexpr (stage == ShaderType::Hull) {
        ctx.render_encoder->setMeshBufferOffset(offset + ctx.offset_base, 29);
      } else if constexpr (stage == ShaderType::Domain) {
        ctx.render_encoder->setVertexBufferOffset(offset + ctx.offset_base, 29);
      } else {
        D3D11_ASSERT(0 && "Not implemented");
      }
    });
  }

  if (BindingCount && (dirty_sampler || dirty_srv || dirty_uav)) {
    auto offset = cmd_list->ReserveGpuHeap(ArgumentTableQwords * 8, 16);

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
        cmd_list->EmitEvent([offset, sampler = sampler.Sampler, arg_offset = arg.StructurePtrOffset](auto &ctx) {
          uint64_t *write_to_it = ctx.gpu_argument_heap_contents + (offset >> 3);
          write_to_it[arg_offset] = sampler->GetArgumentHandle();
          write_to_it[arg_offset + 1] = (uint64_t)std::bit_cast<uint32_t>(sampler->GetLODBias());
        });
        ShaderStage.Samplers.clear_dirty(slot);
        break;
      }
      case SM50BindingType::SRV: {
        if (!ShaderStage.SRVs.test_bound(slot)) {
          // TODO: debug only
          // ERR("expect shader resource at slot ", slot, " but none is
          // bound.");
          cmd_list->EmitEvent([offset, arg_offset = arg.StructurePtrOffset](auto &ctx) {
            uint64_t *write_to_it = ctx.gpu_argument_heap_contents + (offset >> 3);
            write_to_it[arg_offset] = 0;
            write_to_it[arg_offset + 1] = 0;
          });
          break;
        }
        auto &srv = ShaderStage.SRVs[slot];
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
          pTracker = (SIMPLE_RESIDENCY_TRACKER *)(~0uLL);
        } else {
          pTracker = nullptr;
        }
        auto index = cmd_list->GetArgumentDataId(srv.SRV.ptr(), &pTracker);
        cmd_list->EmitEvent([offset, index, arg_offset = arg.StructurePtrOffset, arg_flags = arg.Flags](auto &ctx) {
          uint64_t *write_to_it = ctx.gpu_argument_heap_contents + (offset >> 3);
          auto &arg_data = ctx.argument_table[index];
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
            write_to_it[arg_offset] = arg_data.buffer();
          }
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
            if (arg_data.requiresContext()) {
              IMPLEMENT_ME;
              // chk->emit([=, ref = srv.SRV](CommandChunk::context &ctx) {
              //   write_to_it[arg_offset] = arg_data.texture(&ctx);
              // });
            } else {
              write_to_it[arg_offset] = arg_data.texture();
            }
          }
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
            write_to_it[arg_offset + 1] = arg_data.width();
          }
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP) {
            write_to_it[arg_offset + 1] = std::bit_cast<uint32_t>(arg_data.min_lod());
          }
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET) {
            write_to_it[arg_offset + 1] = arg_data.tbuffer_descriptor();
          }
        });
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
          D3D11_ASSERT(0 && "srv can not have counter associated");
        }
        ShaderStage.SRVs.clear_dirty(slot);
        pTracker->CheckResidency(encoderId, GetResidencyMask<Tessellation>(stage, true, false), &newResidencyMask);
        if (newResidencyMask) {
          useResource(srv.SRV.ptr(), newResidencyMask);
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
          cmd_list->EmitEvent([offset, arg_offset = arg.StructurePtrOffset, arg_flags = arg.Flags](auto &ctx) {
            uint64_t *write_to_it = ctx.gpu_argument_heap_contents + (offset >> 3);
            write_to_it[arg_offset] = 0;
            if (arg_flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
              write_to_it[arg_offset + 1] = 0;
            }
            if (arg_flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
              write_to_it[arg_offset + 2] = 0;
            }
          });
          break;
        }
        auto &uav = UAVBindingSet[arg.SM50BindingSlot];
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
          pTracker = (SIMPLE_RESIDENCY_TRACKER *)(~0uLL);
        } else {
          pTracker = nullptr;
        }
        auto index = cmd_list->GetArgumentDataId(uav.View.ptr(), &pTracker);
        cmd_list->EmitEvent([offset, index, arg_offset = arg.StructurePtrOffset, arg_flags = arg.Flags](auto &ctx) {
          uint64_t *write_to_it = ctx.gpu_argument_heap_contents + (offset >> 3);
          auto &arg_data = ctx.argument_table[index];
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
            write_to_it[arg_offset] = arg_data.buffer();
          }
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
            if (arg_data.requiresContext()) {
              IMPLEMENT_ME;
              // chk->emit([=, ref = uav.View](CommandChunk::context &ctx) {
              //   write_to_it[arg.StructurePtrOffset] = arg_data.texture(&ctx);
              // });
            } else {
              write_to_it[arg_offset] = arg_data.texture();
            }
          }
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
            write_to_it[arg_offset + 1] = arg_data.width();
          }
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP) {
            write_to_it[arg_offset + 1] = std::bit_cast<uint32_t>(arg_data.min_lod());
          }
          if (arg_flags & MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET) {
            write_to_it[arg_offset + 1] = arg_data.tbuffer_descriptor();
          }
        });
        if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
          D3D11_ASSERT(0 && "Unhandled uav counter on deferred context");
          // auto counter_handle = arg_data.counter();
          // if (counter_handle != DXMT_NO_COUNTER) {
          //   auto counter = ctx_state.cmd_queue.counter_pool.GetCounter(counter_handle);
          //   chk->emit([counter](CommandChunk::context &ctx) {
          //     switch (stage) {
          //     case ShaderType::Vertex:
          //     case ShaderType::Pixel:
          //     case ShaderType::Hull:
          //     case ShaderType::Domain:
          //       ctx.render_encoder->useResource(counter.Buffer, MTL::ResourceUsageRead |
          //       MTL::ResourceUsageWrite); break;
          //     case ShaderType::Compute:
          //       ctx.compute_encoder->useResource(counter.Buffer, MTL::ResourceUsageRead |
          //       MTL::ResourceUsageWrite); break;
          //     case ShaderType::Geometry:
          //       D3D11_ASSERT(0 && "Not implemented");
          //       break;
          //     }
          //   });
          //   write_to_it[arg.StructurePtrOffset + 2] = counter.Buffer->gpuAddress() + counter.Offset;
          // } else {
          //   ERR("use uninitialized counter!");
          //   write_to_it[arg.StructurePtrOffset + 2] = 0;
          // };
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
          useResource(uav.View.ptr(), newResidencyMask);
        }
        break;
      }
      }
    }

    /* kArgumentBufferBinding = 30 */
    EmitCommand([offset](CommandChunk::context &ctx) {
      if constexpr (stage == ShaderType::Vertex) {
        if constexpr (Tessellation) {
          ctx.render_encoder->setObjectBufferOffset(offset + ctx.offset_base, 30);
        } else {
          ctx.render_encoder->setVertexBufferOffset(offset + ctx.offset_base, 30);
        }
      } else if constexpr (stage == ShaderType::Pixel) {
        ctx.render_encoder->setFragmentBufferOffset(offset + ctx.offset_base, 30);
      } else if constexpr (stage == ShaderType::Compute) {
        ctx.compute_encoder->setBufferOffset(offset + ctx.offset_base, 30);
      } else if constexpr (stage == ShaderType::Hull) {
        ctx.render_encoder->setMeshBufferOffset(offset + ctx.offset_base, 30);
      } else if constexpr (stage == ShaderType::Domain) {
        ctx.render_encoder->setVertexBufferOffset(offset + ctx.offset_base, 30);
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

  auto &cmd_list = ctx_state.current_cmdlist;

  uint32_t max_slot = 32 - __builtin_clz(slot_mask);
  uint32_t num_slots = __builtin_popcount(slot_mask);

  struct OMG {
    uint32_t arg_index;
    uint32_t stride;
    uint32_t offset;
  };

  auto vertex_resource = cmd_list->AllocateCommandData<OMG>(num_slots);

  for (unsigned slot = 0, index = 0; slot < max_slot; slot++) {
    if (!(slot_mask & (1 << slot)))
      continue;
    if (!VertexBuffers.test_bound(slot)) {
      vertex_resource[index++] = {~0u, 0, 0};
      continue;
    }
    auto &state = VertexBuffers[slot];
    MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
    SIMPLE_RESIDENCY_TRACKER *pTracker;
    vertex_resource[index++] = {cmd_list->GetArgumentDataId(state.Buffer.ptr(), &pTracker), state.Stride, state.Offset};
    pTracker->CheckResidency(
        cmd_list->GetLastEncoder()->encoder_id,
        cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady
            ? GetResidencyMask<true>(ShaderType::Vertex, true, false)
            : GetResidencyMask<false>(ShaderType::Vertex, true, false),
        &newResidencyMask
    );
    if (newResidencyMask) {
      cmd_list->EmitCommand([res = Use(state.Buffer), newResidencyMask](CommandChunk::context &ctx) {
        ctx.render_encoder->useResource(
            res.buffer(), GetUsageFromResidencyMask(newResidencyMask), GetStagesFromResidencyMask(newResidencyMask)
        );
      });
    }
  };

  auto offset = cmd_list->ReserveGpuHeap(16 * num_slots, 16);

  cmd_list->EmitEvent([offset, vertex_resource = std::move(vertex_resource)](auto &event_ctx) {
    VERTEX_BUFFER_ENTRY *entries = (VERTEX_BUFFER_ENTRY *)(((char *)event_ctx.gpu_argument_heap_contents) + offset);
    unsigned index = 0;
    for (auto vertex_buffer : vertex_resource.span()) {
      if (vertex_buffer.arg_index == ~0u) {
        entries[index].buffer_handle = 0;
        entries[index].stride = 0;
        entries[index++].length = 0;
      } else {
        auto &handle = event_ctx.argument_table[vertex_buffer.arg_index];
        entries[index].buffer_handle = handle.buffer() + vertex_buffer.offset;
        entries[index].stride = vertex_buffer.stride;
        entries[index++].length = handle.width() > vertex_buffer.offset ? handle.width() - vertex_buffer.offset : 0;
      }
    }
  });
  if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady) {
    cmd_list->EmitCommand([offset](CommandChunk::context &ctx) {
      ctx.render_encoder->setObjectBufferOffset(offset + ctx.offset_base, 16);
    });
  }
  if (cmdbuf_state == CommandBufferState::RenderPipelineReady) {
    cmd_list->EmitCommand([offset](CommandChunk::context &ctx) {
      ctx.render_encoder->setVertexBufferOffset(offset + ctx.offset_base, 16);
    });
  }
}

class MTLD3D11DeferredContext : public DeferredContextBase {
public:
  MTLD3D11DeferredContext(MTLD3D11Device *pDevice, UINT ContextFlags) :
      DeferredContextBase(pDevice, ctx_state),
      ctx_state(),
      context_flag(ContextFlags) {
    device->CreateCommandList((ID3D11CommandList **)&ctx_state.current_cmdlist);
  }

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
    if (unlikely(!refCount)) {
      this->m_parent->Release();
      delete this;
    }

    return refCount;
  }

  HRESULT
  Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags,
      D3D11_MAPPED_SUBRESOURCE *pMappedResource) override {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      auto &cmd_list = ctx_state.current_cmdlist;
      D3D11_MAPPED_SUBRESOURCE Out;
      switch (MapType) {
      case D3D11_MAP_READ:
      case D3D11_MAP_WRITE:
      case D3D11_MAP_READ_WRITE:
        return E_INVALIDARG;
      case D3D11_MAP_WRITE_DISCARD: {
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
        auto size = dynamic->GetSize(&Out.RowPitch, &Out.DepthPitch);
        Out.pData = cmd_list->AllocateDynamicTempBuffer(dynamic.ptr(), size);
        cmd_list->EmitEvent([addr = Out.pData, size, dynamic = Com(dynamic)](MTLD3D11CommandList::EventContext &ctx) {
          dynamic->RotateBuffer(ctx.device);
          uint32_t _, __;
          auto buffer_dst = dynamic->GetMappedMemory(&_, &__);
          memcpy(buffer_dst, addr, size);
        });
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        dynamic->GetSize(&Out.RowPitch, &Out.DepthPitch);
        Out.pData = cmd_list->GetDynamicTempBuffer(dynamic.ptr());
        if (!Out.pData)
          return E_FAIL;
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
    IMPLEMENT_ME;
  }

  void
  Begin(ID3D11Asynchronous *pAsync) override {
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      auto query = static_cast<IMTLD3DOcclusionQuery *>(pAsync);
      ctx_state.current_cmdlist->EmitEvent([query = Com(query), offset = vro_state.getNextReadOffset(
                                                                )](MTLD3D11CommandList::EventContext &ctx) {
        query->Begin(ctx.cmd_queue.CurrentSeqId(), ctx.visibility_result_offset + offset);
        // FIXME: remove duplication
        ctx.pending_occlusion_queries.push_back(query);
      });
      active_occlusion_queries.insert(query);
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Deferred context: unhandled query type ", desc.Query);
      break;
    }
  }

  void
  End(ID3D11Asynchronous *pAsync) override {
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      ctx_state.current_cmdlist->EmitEvent([event = Com((IMTLD3DEventQuery *)pAsync
                                            )](MTLD3D11CommandList::EventContext &ctx) {
        // granularity is command list
        event->Issue(ctx.cmd_queue.CurrentSeqId());
      });
      promote_flush = true;
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      auto query = static_cast<IMTLD3DOcclusionQuery *>(pAsync);
      if (active_occlusion_queries.erase(query) == 0)
        return; // invalid
      ctx_state.current_cmdlist->EmitEvent([query = Com(query), offset = vro_state.getNextReadOffset(
                                                                )](MTLD3D11CommandList::EventContext &ctx) {
        query->End(ctx.cmd_queue.CurrentSeqId(), ctx.visibility_result_offset + offset);
      });
      promote_flush = true;
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Deferred context: unhandled query type ", desc.Query);
      break;
    }
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
    InvalidateCurrentPass();
    ctx_state.current_cmdlist->promote_flush = promote_flush;
    ctx_state.current_cmdlist->num_visibility_result = vro_state.reset();
    if(!active_occlusion_queries.empty()) {
      ERR("Occlusion query End() is not properly called before FinishCommandList()");
      active_occlusion_queries.clear();
    }
    *ppCommandList = std::move(ctx_state.current_cmdlist);
    promote_flush = false;
    device->CreateCommandList((ID3D11CommandList **)&ctx_state.current_cmdlist);
    if (!RestoreDeferredContextState)
      ClearState();
    return S_OK;
  }

  void
  CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView) override{
      IMPLEMENT_ME
  }

  D3D11_DEVICE_CONTEXT_TYPE GetType() override {
    return D3D11_DEVICE_CONTEXT_DEFERRED;
  }

  virtual void WaitUntilGPUIdle() override{
      // nop
  };

  void Commit() override{
      // nop
  };

private:
  DeferredContextInternalState ctx_state;
  std::atomic<uint32_t> refcount = 0;
  UINT context_flag;
};

Com<MTLD3D11DeviceContextBase>
CreateDeferredContext(MTLD3D11Device *pDevice, UINT ContextFlags) {
  return new MTLD3D11DeferredContext(pDevice, ContextFlags);
}

thread_local const BindingRef *BindingRef::lut;

class MTLD3D11CommandListPool : public MTLD3D11CommandListPoolBase {
public:
  MTLD3D11CommandListPool(MTLD3D11Device *device) : device(device){};

  ~MTLD3D11CommandListPool(){

  };

  void
  RecycleCommandList(ID3D11CommandList *cmdlist) override {
    std::unique_lock<dxmt::mutex> lock(mutex);
    free_commandlist.push_back((MTLD3D11CommandList *)cmdlist);
  };

  void
  CreateCommandList(ID3D11CommandList **ppCommandList) override {
    std::unique_lock<dxmt::mutex> lock(mutex);
    if (!free_commandlist.empty()) {
      free_commandlist.back()->QueryInterface(IID_PPV_ARGS(ppCommandList));
      free_commandlist.pop_back();
      return;
    }
    auto instance = std::make_unique<MTLD3D11CommandList>(device, this, 0);
    *ppCommandList = ref(instance.get());
    instances.push_back(std::move(instance));
    return;
  };

private:
  std::vector<MTLD3D11CommandList *> free_commandlist;
  std::vector<std::unique_ptr<MTLD3D11CommandList>> instances;
  MTLD3D11Device *device;
  dxmt::mutex mutex;
};

std::unique_ptr<MTLD3D11CommandListPoolBase>
InitializeCommandListPool(MTLD3D11Device *device) {
  return std::make_unique<MTLD3D11CommandListPool>(device);
}

}; // namespace dxmt