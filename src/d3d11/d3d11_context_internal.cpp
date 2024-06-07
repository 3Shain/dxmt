
/**
This file is included by d3d11_context.cpp
and should be not used as a compilation unit

since it is for internal use only
(and I don't want to deal with several thousands line of code)
*/
#include "d3d11_context_state.hpp"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_private.h"
#include "dxmt_command_queue.hpp"
#include "util_flags.hpp"

namespace dxmt {

class ContextInternal {

public:
  ContextInternal(IMTLD3D11Device *pDevice, D3D11ContextState &state,
                  CommandQueue &cmd_queue)
      : device(pDevice), state_(state), cmd_queue(cmd_queue) {
    default_rasterizer_state = CreateDefaultRasterizerState(pDevice);
    default_depth_stencil_state = CreateDefaultDepthStencilState(pDevice);
    default_blend_state = CreateDefaultBlendState(pDevice);
  }

#pragma region ShaderCommon

  template <ShaderType Type, typename IShader>
  void SetShader(IShader *pShader, ID3D11ClassInstance *const *ppClassInstances,
                 UINT NumClassInstances) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (pShader) {
      if (auto expected = com_cast<IMTLD3D11Shader>(pShader)) {
        ShaderStage.Shader = std::move(expected);
      } else {
        assert(0 && "wtf");
      }
    } else {
      ShaderStage.Shader = nullptr;
    }

    if (NumClassInstances)
      ERR("Class instances not supported");

    if (Type == ShaderType::Compute) {
      InvalidateComputePipeline();
    } else {
      InvalidateGraphicPipeline();
    }
  }

  template <ShaderType Type, typename IShader>
  void GetShader(IShader **ppShader, ID3D11ClassInstance **ppClassInstances,
                 UINT *pNumClassInstances) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (ppShader) {
      if (ShaderStage.Shader) {
        ShaderStage.Shader->QueryInterface(IID_PPV_ARGS(ppShader));
      } else {
        *ppShader = NULL;
      }
    }
    if (ppClassInstances) {
      *ppClassInstances = NULL;
    }
    if (pNumClassInstances) {
      *pNumClassInstances = 0;
    }
  }

  template <ShaderType Type>
  void SetConstantBuffer(UINT StartSlot, UINT NumBuffers,
                         ID3D11Buffer *const *ppConstantBuffers,
                         const UINT *pFirstConstant,
                         const UINT *pNumConstants) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    for (unsigned i = StartSlot; i < StartSlot + NumBuffers; i++) {
      auto pConstantBuffer = ppConstantBuffers[i - StartSlot];
      if (pConstantBuffer) {
        auto maybeExist = ShaderStage.ConstantBuffers.insert({i, {}});
        // either old value or inserted empty value
        auto &entry = maybeExist.first->second;
        entry.Buffer = nullptr; // dereference old
        entry.FirstConstant =
            pFirstConstant ? pFirstConstant[i - StartSlot] : 0;
        entry.NumConstants = pNumConstants ? pNumConstants[i - StartSlot]
                                           : 4096; // FIXME: too much
        if (auto dynamic = com_cast<IMTLDynamicBindable>(pConstantBuffer)) {
          dynamic->GetBindable(&entry.Buffer,
                               [this]() { binding_ready.clr(Type); });
        } else if (auto expected = com_cast<IMTLBindable>(pConstantBuffer)) {
          entry.Buffer = std::move(expected);
        } else {
          assert(0 && "wtf");
        }
      } else {
        // BIND NULL
        ShaderStage.ConstantBuffers.erase(i);
      }
    }

    binding_ready.clr(Type);
  }

  template <ShaderType Type>
  void GetConstantBuffer(UINT StartSlot, UINT NumBuffers,
                         ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant,
                         UINT *pNumConstants) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    for (auto i = 0u; i < NumBuffers; i++) {
      if (ShaderStage.ConstantBuffers.contains(StartSlot + i)) {
        auto &cb = ShaderStage.ConstantBuffers[StartSlot + i];
        if (ppConstantBuffers) {
          cb.Buffer->GetLogicalResourceOrView(
              IID_PPV_ARGS(&ppConstantBuffers[i]));
        }
        if (pFirstConstant) {
          pFirstConstant[i] = cb.FirstConstant;
        }
        if (pNumConstants) {
          pNumConstants[i] = cb.NumConstants;
        }
      } else {
        if (ppConstantBuffers) {
          ppConstantBuffers[i] = nullptr;
        }
        // FIXME: should reset FirstConstant and NumConstants?
      }
    }
  }

  template <ShaderType Type>
  void
  SetShaderResource(UINT StartSlot, UINT NumViews,
                    ID3D11ShaderResourceView *const *ppShaderResourceViews) {

    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    for (unsigned Slot = StartSlot; Slot < StartSlot + NumViews; Slot++) {
      auto pView = ppShaderResourceViews[Slot - StartSlot];
      if (pView) {
        auto maybeExist = ShaderStage.SRVs.insert({Slot, {}});
        // either old value or inserted empty value
        auto &entry = maybeExist.first->second;
        entry = nullptr;
        if (auto dynamic = com_cast<IMTLDynamicBindable>(pView)) {
          dynamic->GetBindable(&entry, [this]() { binding_ready.clr(Type); });
        } else if (auto expected = com_cast<IMTLBindable>(pView)) {
          entry = std::move(expected);
        } else {
          assert(0 && "wtf");
        }
      } else {
        // BIND NULL
        ShaderStage.SRVs.erase(Slot);
      }
    }

    binding_ready.clr(Type);
  }

  template <ShaderType Type>
  void GetShaderResource(UINT StartSlot, UINT NumViews,
                         ID3D11ShaderResourceView **ppShaderResourceViews) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (!ppShaderResourceViews)
      return;
    for (auto i = 0u; i < NumViews; i++) {
      if (ShaderStage.SRVs.contains(StartSlot + i)) {
        ShaderStage.SRVs[i]->GetLogicalResourceOrView(
            IID_PPV_ARGS(&ppShaderResourceViews[i]));
      } else {
        ppShaderResourceViews[i] = nullptr;
      }
    }
  }

  template <ShaderType Type>
  void SetSamplers(UINT StartSlot, UINT NumSamplers,
                   ID3D11SamplerState *const *ppSamplers) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    for (unsigned Slot = StartSlot; Slot < StartSlot + NumSamplers; Slot++) {
      auto pSampler = ppSamplers[Slot - StartSlot];
      if (pSampler) {
        auto maybeExist = ShaderStage.Samplers.insert({Slot, {}});
        // either old value or inserted empty value
        auto &entry = maybeExist.first->second;
        if (auto expected = com_cast<IMTLD3D11SamplerState>(pSampler)) {
          entry = std::move(expected);
        } else {
          assert(0 && "wtf");
        }
      } else {
        // BIND NULL
        ShaderStage.Samplers.erase(Slot);
      }
    }

    binding_ready.clr(Type);
  }

  template <ShaderType Type>
  void GetSamplers(UINT StartSlot, UINT NumSamplers,
                   ID3D11SamplerState **ppSamplers) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    if (ppSamplers) {
      for (unsigned Slot = StartSlot; Slot < StartSlot + NumSamplers; Slot++) {
        if (ShaderStage.Samplers.contains(Slot)) {
          ppSamplers[Slot - StartSlot] = ShaderStage.Samplers[Slot].ref();
        } else {
          ppSamplers[Slot - StartSlot] = nullptr;
        }
      }
    }
  }

#pragma endregion

#pragma region CopyResource

  void CopyBuffer(ID3D11Buffer *pDstResource, ID3D11Buffer *pSrcResource) {
    D3D11_BUFFER_DESC dst_desc;
    D3D11_BUFFER_DESC src_desc;
    pDstResource->GetDesc(&dst_desc);
    pSrcResource->GetDesc(&src_desc);
    MTL_BIND_RESOURCE dst_bind;
    UINT bytes_per_row, bytes_per_image;
    MTL_BIND_RESOURCE src_bind;
    auto currentChunkId = cmd_queue.CurrentSeqId();
    if (auto staging_dst = com_cast<IMTLD3D11Staging>(pDstResource)) {
      staging_dst->UseCopyDestination(0, currentChunkId, &dst_bind,
                                      &bytes_per_row, &bytes_per_image);
      if (auto staging_src = com_cast<IMTLD3D11Staging>(pSrcResource)) {
        // wtf
        assert(0 && "TODO: copy between staging?");
      } else if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        // might be a dynamic, default or immutable buffer
        src->GetBoundResource(&src_bind);
        EmitBlitCommand<true>(
            [dst = Obj(dst_bind.Buffer),
             src = Obj(src_bind.Buffer)](MTL::BlitCommandEncoder *encoder) {
              encoder->copyFromBuffer(src, 0, dst, 0, src->length());
            });
      }

    } else if (dst_desc.Usage == D3D11_USAGE_DEFAULT) {
      auto dst = com_cast<IMTLBindable>(pDstResource);
      assert(dst);
      dst->GetBoundResource(&dst_bind);
      if (auto staging_src = com_cast<IMTLD3D11Staging>(pSrcResource)) {
        // copy from staging to default
        staging_src->UseCopySource(0, currentChunkId, &src_bind, &bytes_per_row,
                                   &bytes_per_image);
        EmitBlitCommand<true>(
            [dst = Obj(dst_bind.Buffer),
             src = Obj(src_bind.Buffer)](MTL::BlitCommandEncoder *encoder) {
              encoder->copyFromBuffer(src, 0, dst, 0, src->length());
            });
      } else if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        // on-device copy
        src->GetBoundResource(&src_bind);
        assert(src_bind.Type == MTL_BIND_BUFFER_UNBOUNDED);
        EmitBlitCommand<true>(
            [dst = Obj(dst_bind.Buffer),
             src = Obj(src_bind.Buffer)](MTL::BlitCommandEncoder *encoder) {
              encoder->copyFromBuffer(src, 0, dst, 0, src->length());
            });
      }
    }
  }

#pragma endregion

#pragma region CommandEncoder Maintain State

  /**
  Render pass can be invalidated by reasons:
  - render target changes (including depth stencil)
  All pass can be invalidated by reasons:
  - a encoder type switch
  - flush/present
  */
  void InvalidateCurrentPass() {
    CommandChunk *chk = cmd_queue.CurrentChunk();
    switch (cmdbuf_state) {
    case CommandBufferState::Idle:
      break;
    case CommandBufferState::RenderEncoderActive:
    case CommandBufferState::RenderPipelineReady:
      chk->emit([](CommandChunk::context &ctx) {
        ctx.render_encoder->endEncoding();
        ctx.render_encoder = nullptr;
      });
      break;
    case CommandBufferState::ComputeEncoderActive:
    case CommandBufferState::ComputePipelineReady:
      chk->emit([](CommandChunk::context &ctx) {
        ctx.compute_encoder->endEncoding();
        ctx.compute_encoder = nullptr;
      });
      break;
    case CommandBufferState::BlitEncoderActive:
      chk->emit([](CommandChunk::context &ctx) {
        ctx.blit_encoder->endEncoding();
        ctx.blit_encoder = nullptr;
      });
      break;
    }
    binding_ready.clrAll();

    cmdbuf_state = CommandBufferState::Idle;
  }

  /**
  Render pipeline can be invalidate by reasons:
  - shader program changes
  - input layout changes (if using metal vertex descriptor)
  - blend state changes
  - tessellation state changes (not implemented yet)
  - (when layered rendering) input primitive topology changes
  -
  */
  void InvalidateGraphicPipeline() {
    if (cmdbuf_state != CommandBufferState::RenderPipelineReady)
      return;
    cmdbuf_state = CommandBufferState::RenderEncoderActive;
  }

  /**
  Compute pipeline can be invalidate by reasons:
  - shader program changes
  TOOD: maybe we don't need this, since compute shader can be pre-compiled
  */
  void InvalidateComputePipeline() {
    if (cmdbuf_state != CommandBufferState::ComputePipelineReady)
      return;
    cmdbuf_state = CommandBufferState::ComputeEncoderActive;
  }

  /**
  Switch to render encoder and set all states (expect for pipeline state)
  */
  void SwitchToRenderEncoder() {
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return;
    if (cmdbuf_state == CommandBufferState::RenderEncoderActive)
      return;
    InvalidateCurrentPass();

    // should assume: render target is properly set
    {
      /* Setup RenderCommandEncoder */
      CommandChunk *chk = cmd_queue.CurrentChunk();

      struct RENDER_TARGET_STATE {
        Obj<MTL::Texture> Texture;
        UINT RenderTargetIndex;
        UINT MipSlice;
        UINT ArrayIndex;
        MTL::PixelFormat PixelFormat;
      };
      auto rtvs =
          chk->reserve_vector<RENDER_TARGET_STATE>(state_.OutputMerger.NumRTVs);
      for (unsigned i = 0; i < state_.OutputMerger.NumRTVs; i++) {
        auto &rtv = state_.OutputMerger.RTVs[i];
        if (rtv) {
          rtvs.push_back(
              {rtv->GetCurrentTexture(), i, 0, 0, rtv->GetPixelFormat()});
          assert(rtv->GetPixelFormat() != MTL::PixelFormatInvalid);
        } else {
          rtvs.push_back({nullptr, i, 0, 0, MTL::PixelFormatInvalid});
        }
      }
      struct DEPTH_STENCIL_STATE {
        Obj<MTL::Texture> Texture;
        UINT MipSlice;
        UINT ArrayIndex;
        MTL::PixelFormat PixelFormat;
      };
      auto &dsv = state_.OutputMerger.DSV;
      chk->emit(
          [rtvs = std::move(rtvs),
           dsv = DEPTH_STENCIL_STATE{
               dsv != nullptr ? dsv->GetCurrentTexture() : nullptr, 0, 0,
               dsv != nullptr
                   ? dsv->GetPixelFormat()
                   : MTL::PixelFormatInvalid}](CommandChunk::context &ctx) {
            auto renderPassDescriptor =
                MTL::RenderPassDescriptor::renderPassDescriptor();
            for (auto &rtv : rtvs) {
              if (rtv.PixelFormat == MTL::PixelFormatInvalid)
                continue;
              auto colorAttachment =
                  renderPassDescriptor->colorAttachments()->object(
                      rtv.RenderTargetIndex);
              colorAttachment->setTexture(rtv.Texture);
              colorAttachment->setLevel(rtv.MipSlice);
              colorAttachment->setSlice(rtv.ArrayIndex);
              colorAttachment->setLoadAction(MTL::LoadActionLoad);
              colorAttachment->setStoreAction(MTL::StoreActionStore);
            };
            if (dsv.Texture) {
              // TODO: ...should know more about load/store behavior
              auto depthAttachment = renderPassDescriptor->depthAttachment();
              depthAttachment->setTexture(dsv.Texture);
              depthAttachment->setLevel(dsv.MipSlice);
              depthAttachment->setSlice(dsv.ArrayIndex);
              depthAttachment->setLoadAction(MTL::LoadActionLoad);
              depthAttachment->setStoreAction(MTL::StoreActionStore);

              // TODO: should know if depth buffer has stencil bits
              // auto stencilAttachment =
              // renderPassDescriptor->stencilAttachment();
              // stencilAttachment->setTexture(dsv.Texture);
              // stencilAttachment->setLevel(dsv.MipSlice);
              // stencilAttachment->setSlice(dsv.ArrayIndex);
              // stencilAttachment->setLoadAction(MTL::LoadActionLoad);
              // stencilAttachment->setStoreAction(MTL::StoreActionStore);
            }
            ctx.render_encoder =
                ctx.cmdbuf->renderCommandEncoder(renderPassDescriptor);
          });
    }

    cmdbuf_state = CommandBufferState::RenderEncoderActive;

    EmitSetDepthStencilState<true>();
    EmitSetRasterizerState<true>();
    EmitSetScissor<true>();
    EmitSetViewport<true>();
    EmitSetIAState<true>();
  }

  /**
  Switch to blit encoder
  */
  void SwitchToBlitEncoder() {
    if (cmdbuf_state == CommandBufferState::BlitEncoderActive)
      return;
    InvalidateCurrentPass();

    {
      /* Setup ComputeCommandEncoder */
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([](CommandChunk::context &ctx) {
        ctx.blit_encoder = ctx.cmdbuf->blitCommandEncoder();
      });
    }

    cmdbuf_state = CommandBufferState::BlitEncoderActive;
  }

  /**
  Switch to compute encoder and set all states (expect for pipeline state)
  */
  void SwitchToComputeEncoder() {
    if (cmdbuf_state == CommandBufferState::ComputePipelineReady)
      return;
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive)
      return;
    InvalidateCurrentPass();

    {
      /* Setup ComputeCommandEncoder */
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([](CommandChunk::context &ctx) {
        ctx.compute_encoder = ctx.cmdbuf->computeCommandEncoder();
      });
    }

    cmdbuf_state = CommandBufferState::ComputeEncoderActive;
  }

  /**
  Assume we have all things needed to build PSO
  If the current encoder is not a render encoder, switch to it.
  */
  void FinalizeCurrentRenderPipeline() {
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return;
    assert(state_.InputAssembler.InputLayout && "");

    SwitchToRenderEncoder();

    CommandChunk *chk = cmd_queue.CurrentChunk();

    Com<IMTLCompiledGraphicsPipeline> pipeline;
    Com<IMTLCompiledShader> vs, ps;
    state_.ShaderStages[(UINT)ShaderType::Vertex]
        .Shader //
        ->GetCompiledShader(NULL, &vs);
    state_.ShaderStages[(UINT)ShaderType::Pixel]
        .Shader //
        ->GetCompiledShader(NULL, &ps);
    MTL_GRAPHICS_PIPELINE_DESC pipelineDesc;
    pipelineDesc.VertexShader = vs.ptr();
    pipelineDesc.PixelShader = ps.ptr();
    pipelineDesc.InputLayout = state_.InputAssembler.InputLayout.ptr();
    pipelineDesc.NumColorAttachments = state_.OutputMerger.NumRTVs;
    for (unsigned i = 0; i < pipelineDesc.NumColorAttachments; i++) {
      auto &rtv = state_.OutputMerger.RTVs[i];
      if (rtv) {
        pipelineDesc.ColorAttachmentFormats[i] =
            state_.OutputMerger.RTVs[i]->GetPixelFormat();
      } else {
        pipelineDesc.ColorAttachmentFormats[i] = MTL::PixelFormatInvalid;
      }
    }
    pipelineDesc.BlendState = state_.OutputMerger.BlendState
                                  ? state_.OutputMerger.BlendState.ptr()
                                  : default_blend_state.ptr();
    pipelineDesc.DepthStencilFormat =
        state_.OutputMerger.DSV ? state_.OutputMerger.DSV->GetPixelFormat()
                                : MTL::PixelFormatInvalid;

    device->CreateGraphicsPipeline(&pipelineDesc, &pipeline);

    chk->emit([pso = std::move(pipeline)](CommandChunk::context &ctx) {
      MTL_COMPILED_GRAPHICS_PIPELINE GraphicsPipeline;
      pso->GetPipeline(&GraphicsPipeline); // may block
      ctx.render_encoder->setRenderPipelineState(
          GraphicsPipeline.PipelineState);
    });

    // FIXME: ...ugly
    MTL::ArgumentEncoder *encoder;
    state_.ShaderStages[(UINT)ShaderType::Vertex].Shader->GetArgumentEncoderRef(
        &encoder);
    if (encoder) {
      auto [heap, offset] = chk->inspect_gpu_heap();
      chk->emit([heap, offset](CommandChunk::context &ctx) {
        ctx.render_encoder->setVertexBuffer(heap, offset,
                                            30 /* kArgumentBufferBinding */);
      });
    }
    state_.ShaderStages[(UINT)ShaderType::Pixel].Shader->GetArgumentEncoderRef(
        &encoder);
    if (encoder) {
      auto [heap, offset] = chk->inspect_gpu_heap();
      chk->emit([heap, offset](CommandChunk::context &ctx) {
        ctx.render_encoder->setFragmentBuffer(heap, offset,
                                              30 /* kArgumentBufferBinding */);
      });
    }

    cmdbuf_state = CommandBufferState::RenderPipelineReady;
  }

  void PreDraw() {
    FinalizeCurrentRenderPipeline();
    UploadShaderStageResourceBinding<ShaderType::Vertex>();
    UploadShaderStageResourceBinding<ShaderType::Pixel>();
  }

  /**
  Assume we have all things needed to build PSO
  If the current encoder is not a compute encoder, switch to it.
  */
  bool FinalizeCurrentComputePipeline() {
    if (cmdbuf_state == CommandBufferState::ComputePipelineReady)
      return true;

    SwitchToComputeEncoder();

    Com<IMTLCompiledShader> shader;
    state_.ShaderStages[(UINT)ShaderType::Compute]
        .Shader //
        ->GetCompiledShader(NULL, &shader);
    if (!shader) {
      ERR("Shader not found?");
      return false;
    }
    MTL_COMPILED_SHADER shader_data;
    shader->GetShader(&shader_data);
    Com<IMTLCompiledComputePipeline> pipeline;
    device->CreateComputePipeline(shader.ptr(), &pipeline);

    CommandChunk *chk = cmd_queue.CurrentChunk();
    chk->emit(
        [pso = std::move(pipeline),
         tg_size = MTL::Size::Make(shader_data.Reflection->ThreadgroupSize[0],
                                   shader_data.Reflection->ThreadgroupSize[1],
                                   shader_data.Reflection->ThreadgroupSize[2])](
            CommandChunk::context &ctx) {
          MTL_COMPILED_COMPUTE_PIPELINE ComputePipeline;
          pso->GetPipeline(&ComputePipeline); // may block
          ctx.compute_encoder->setComputePipelineState(
              ComputePipeline.PipelineState);
          ctx.cs_threadgroup_size = tg_size;
        });

    if (shader_data.Reflection->NumArguments) {
      auto [heap, offset] = chk->inspect_gpu_heap();
      chk->emit([heap, offset](CommandChunk::context &ctx) {
        ctx.compute_encoder->setBuffer(heap, offset,
                                       30 /* kArgumentBufferBinding */);
      });
    }

    cmdbuf_state = CommandBufferState::ComputePipelineReady;
    return true;
  }

  bool PreDispatch() {
    if (!FinalizeCurrentComputePipeline()) {
      return false;
    }
    UploadShaderStageResourceBinding<ShaderType::Compute>();
    return true;
  }

  template <ShaderType stage, bool Force = false>
  void UploadShaderStageResourceBinding() {
    if (!Force && binding_ready.test(stage))
      return;

    auto &ShaderStage = state_.ShaderStages[(UINT)stage];

    MTL_SHADER_REFLECTION reflection;
    ShaderStage.Shader->GetReflection(&reflection);
    auto BindingCount = reflection.NumArguments;
    MTL::ArgumentEncoder *encoder;
    ShaderStage.Shader->GetArgumentEncoderRef(&encoder);

    if (encoder) {
      CommandChunk *chk = cmd_queue.CurrentChunk();

      auto [heap, offset] = chk->allocate_gpu_heap(encoder->encodedLength(),
                                                   encoder->alignment());
      encoder->setArgumentBuffer(heap, offset);

      auto useResource = [&](MTL::Resource *res, MTL::ResourceUsage usage) {
        chk->emit([res, usage](CommandChunk::context &ctx) {
          switch (stage) {
          case ShaderType::Vertex:
            ctx.render_encoder->useResource(res, usage, MTL::RenderStageVertex);
            break;
          case ShaderType::Pixel:
            ctx.render_encoder->useResource(res, usage,
                                            MTL::RenderStageFragment);
            break;
          case ShaderType::Compute:
            ctx.compute_encoder->useResource(res, usage);
            break;
          case ShaderType::Geometry:
          case ShaderType::Hull:
          case ShaderType::Domain:
            assert(0 && "Not implemented");
            break;
          }
        });
      };

      for (unsigned i = 0; i < BindingCount; i++) {
        auto &arg = reflection.Arguments[i];
        switch (arg.Type) {
        case SM50BindingType::ConstantBuffer: {
          if (!ShaderStage.ConstantBuffers.contains(arg.SM50BindingSlot))
            break;
          auto &cbuf = ShaderStage.ConstantBuffers[arg.SM50BindingSlot];
          MTL_BIND_RESOURCE m;
          cbuf.Buffer->GetBoundResource(&m);
          encoder->setBuffer(m.Buffer, 0, GetArgumentIndex(arg));
          useResource(m.Buffer, MTL::ResourceUsageRead);
          break;
        }
        case SM50BindingType::Sampler: {
          if (!ShaderStage.Samplers.contains(arg.SM50BindingSlot))
            break;

          auto &sampler = ShaderStage.Samplers[arg.SM50BindingSlot];
          assert(sampler);
          encoder->setSamplerState(sampler->GetSamplerState(),
                                   GetArgumentIndex(arg));
          break;
        }
        case SM50BindingType::SRV: {
          if (!ShaderStage.SRVs.contains(arg.SM50BindingSlot))
            break;
          auto &srv = ShaderStage.SRVs[arg.SM50BindingSlot];
          MTL_BIND_RESOURCE m;
          srv->GetBoundResource(&m);
          switch (m.Type) {
          case MTL_BIND_BUFFER_UNBOUNDED: {
            encoder->setBuffer(m.Buffer, 0, GetArgumentIndex(arg));
            useResource(m.Buffer, MTL::ResourceUsageRead);
            break;
          }
          case MTL_BIND_TEXTURE: {
            encoder->setTexture(m.Texture, GetArgumentIndex(arg));
            useResource(m.Texture, MTL::ResourceUsageRead);
            break;
          }
          case MTL_BIND_BUFFER_BOUNDED:
          case MTL_BIND_UAV_WITH_COUNTER:
            WARN("Unknown SRV binding");
            break;
          }
          break;
        }
        case SM50BindingType::UAV: {
          if constexpr (stage == ShaderType::Compute) {
            if (!state_.ComputeStageUAV.UAVs.contains(arg.SM50BindingSlot))
              break;
            auto &uav = state_.ComputeStageUAV.UAVs[arg.SM50BindingSlot];
            MTL_BIND_RESOURCE m;
            uav.View->GetBoundResource(&m);
            switch (m.Type) {
            case MTL_BIND_BUFFER_UNBOUNDED: {
              encoder->setBuffer(m.Buffer, 0, GetArgumentIndex(arg));
              useResource(m.Buffer,
                          MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
              break;
            }
            case MTL_BIND_TEXTURE: {
              encoder->setTexture(m.Texture, GetArgumentIndex(arg));
              useResource(m.Texture,
                          MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
              break;
            }
            case MTL_BIND_BUFFER_BOUNDED:
            case MTL_BIND_UAV_WITH_COUNTER:
              WARN("Unknown UAV binding");
              break;
            }
          } else {
            if (!state_.OutputMerger.UAVs.contains(arg.SM50BindingSlot))
              break;
            // auto &uav = state_.OutputMerger.UAVs[arg.SM50BindingSlot];
            assert(0 && "TODO");
          }
          break;
        }
        }
      }

      /* kArgumentBufferBinding = 30 */
      chk->emit([offset](CommandChunk::context &ctx) {
        if constexpr (stage == ShaderType::Vertex) {
          ctx.render_encoder->setVertexBufferOffset(offset, 30);
        } else if constexpr (stage == ShaderType::Pixel) {
          ctx.render_encoder->setFragmentBufferOffset(offset, 30);
        } else if constexpr (stage == ShaderType::Compute) {
          ctx.compute_encoder->setBufferOffset(offset, 30);
        } else {
          assert(0 && "Not implemented");
        }
      });
    }
    binding_ready.set(stage);
  }

  template <bool Force = false, typename Fn> void EmitRenderCommand(Fn &&fn) {
    if (Force) {
      SwitchToRenderEncoder();
    }
    if (cmdbuf_state == CommandBufferState::RenderEncoderActive ||
        cmdbuf_state == CommandBufferState::RenderPipelineReady) {
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        fn(ctx.render_encoder.ptr());
      });
    }
  };

  template <bool Force = false, typename Fn> void EmitComputeCommand(Fn &&fn) {
    if (Force) {
      SwitchToComputeEncoder();
    }
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive ||
        cmdbuf_state == CommandBufferState::ComputePipelineReady) {
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        fn(ctx.compute_encoder.ptr(), ctx.cs_threadgroup_size);
      });
    }
  };

  template <bool Force = false, typename Fn> void EmitBlitCommand(Fn &&fn) {
    if (Force) {
      SwitchToBlitEncoder();
    }
    if (cmdbuf_state == CommandBufferState::BlitEncoderActive) {
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        fn(ctx.blit_encoder.ptr());
      });
    }
  };

  template <bool Force = false> void EmitSetDepthStencilState() {
    auto state = state_.OutputMerger.DepthStencilState
                     ? state_.OutputMerger.DepthStencilState
                     : default_depth_stencil_state;
    EmitRenderCommand<Force>(
        [state, stencil_ref = state_.OutputMerger.StencilRef](
            MTL::RenderCommandEncoder *encoder) {
          encoder->setDepthStencilState(state->GetDepthStencilState());
          encoder->setStencilReferenceValue(stencil_ref);
        });
  }

  template <bool Force = false> void EmitSetRasterizerState() {
    auto state = state_.Rasterizer.RasterizerState
                     ? state_.Rasterizer.RasterizerState
                     : default_rasterizer_state;
    EmitRenderCommand<Force>([state](MTL::RenderCommandEncoder *encoder) {
      state->SetupRasterizerState(encoder);
    });
  }

  template <bool Force = false> void EmitSetScissor() {
    CommandChunk *chk = cmd_queue.CurrentChunk();
    auto scissors = chk->reserve_vector<MTL::ScissorRect>(
        state_.Rasterizer.NumScissorRects);
    for (unsigned i = 0; i < state_.Rasterizer.NumScissorRects; i++) {
      auto &d3dRect = state_.Rasterizer.scissor_rects[i];
      scissors.push_back({(UINT)d3dRect.left, (UINT)d3dRect.top,
                          (UINT)d3dRect.right - d3dRect.left,
                          (UINT)d3dRect.bottom - d3dRect.top});
    }
    EmitRenderCommand<Force>(
        [scissors = std::move(scissors)](MTL::RenderCommandEncoder *encoder) {
          if (scissors.size() == 0)
            return;
          encoder->setScissorRects(scissors.data(), scissors.size());
        });
  }

  template <bool Force = false> void EmitSetViewport() {

    CommandChunk *chk = cmd_queue.CurrentChunk();
    auto viewports =
        chk->reserve_vector<MTL::Viewport>(state_.Rasterizer.NumViewports);
    for (unsigned i = 0; i < state_.Rasterizer.NumViewports; i++) {
      auto &d3dViewport = state_.Rasterizer.viewports[i];
      viewports.push_back({d3dViewport.TopLeftX, d3dViewport.TopLeftY,
                           d3dViewport.Width, d3dViewport.Height,
                           d3dViewport.MinDepth, d3dViewport.MaxDepth});
    }
    EmitRenderCommand<Force>(
        [viewports = std::move(viewports)](MTL::RenderCommandEncoder *encoder) {
          encoder->setViewports(viewports.data(), viewports.size());
        });
  }

  /**
  it's just about vertex buffer
  - index buffer and input topology are provided in draw commands
  */
  template <bool Force = false> void EmitSetIAState() {
    CommandChunk *chk = cmd_queue.CurrentChunk();
    struct VERTEX_BUFFER_STATE {
      Obj<MTL::Buffer> buffer;
      UINT offset;
      UINT stride;
    };
    auto vbs = chk->reserve_vector<std::pair<UINT, VERTEX_BUFFER_STATE>>(
        state_.InputAssembler.VertexBuffers.size());
    for (auto &[index, state] : state_.InputAssembler.VertexBuffers) {
      MTL_BIND_RESOURCE r;
      assert(state.Buffer);
      state.Buffer->GetBoundResource(&r);
      switch (r.Type) {
      case MTL_BIND_BUFFER_UNBOUNDED:
        vbs.push_back({index, {r.Buffer, state.Offset, state.Stride}});
        break;
      case MTL_BIND_TEXTURE:
      case MTL_BIND_BUFFER_BOUNDED:
      case MTL_BIND_UAV_WITH_COUNTER:
        assert(0 && "unexpected vertex buffer binding type");
        break;
      }
    };
    EmitRenderCommand<Force>(
        [vbs = std::move(vbs)](MTL::RenderCommandEncoder *encoder) {
          for (auto &[index, state] : vbs) {
            encoder->setVertexBuffer(state.buffer.ptr(), state.offset,
                                     state.stride, index);
          };
          // TODO: deal with old redunant binding (I guess it doesn't hurt
        });
  }

  template <bool Force = false> void EmitBlendFactorAndStencilRef() {
    EmitRenderCommand<Force>([r = state_.OutputMerger.BlendFactor[0],
                              g = state_.OutputMerger.BlendFactor[1],
                              b = state_.OutputMerger.BlendFactor[2],
                              a = state_.OutputMerger.BlendFactor[3],
                              stencil_ref = state_.OutputMerger.StencilRef](
                                 MTL::RenderCommandEncoder *encoder) {
      encoder->setBlendColor(r, g, b, a);
      encoder->setStencilReferenceValue(stencil_ref);
    });
  }

#pragma endregion

  /**
  Valid transition:
  * -> Idle
  Idle -> RenderEncoderActive
  Idle -> ComputeEncoderActive
  Idle -> BlitEncoderActive
  RenderEncoderActive <-> RenderPipelineReady
  ComputeEncoderActive <-> CoputePipelineReady
  */
  enum class CommandBufferState {
    Idle,
    RenderEncoderActive,
    RenderPipelineReady,
    ComputeEncoderActive,
    ComputePipelineReady,
    BlitEncoderActive
  };

#pragma region Default State

private:
  /**
  Don't bind them to state or provide to API consumer
  (use COM just in case it's referenced by encoding thread)
  ...tricky
  */
  Com<IMTLD3D11RasterizerState> default_rasterizer_state;
  Com<IMTLD3D11DepthStencilState> default_depth_stencil_state;
  Com<IMTLD3D11BlendState> default_blend_state;

#pragma endregion

private:
  IMTLD3D11Device *device;
  D3D11ContextState &state_;
  CommandBufferState cmdbuf_state;
  Flags<ShaderType> binding_ready;
  CommandQueue &cmd_queue;
};

} // namespace dxmt