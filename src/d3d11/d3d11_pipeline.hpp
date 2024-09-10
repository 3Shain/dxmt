#pragma once

#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "com/com_guid.hpp"
#include "d3d11_device.hpp"
#include "d3d11_input_layout.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_state_object.hpp"
#include "util_hash.hpp"

struct MTL_GRAPHICS_PIPELINE_DESC {
  IMTLD3D11Shader *VertexShader;
  IMTLD3D11Shader *HullShader;
  IMTLD3D11Shader *DomainShader;
  IMTLD3D11Shader *PixelShader;
  IMTLD3D11BlendState *BlendState;
  IMTLD3D11InputLayout *InputLayout;
  UINT NumColorAttachments;
  MTL::PixelFormat ColorAttachmentFormats[8];
  MTL::PixelFormat DepthStencilFormat;
  bool RasterizationEnabled;
  SM50_INDEX_BUFFER_FORAMT IndexBufferFormat;
  uint32_t SampleMask;
};

struct MTL_COMPILED_GRAPHICS_PIPELINE {
  MTL::RenderPipelineState *PipelineState = nullptr;
};

struct MTL_COMPILED_COMPUTE_PIPELINE {
  MTL::ComputePipelineState *PipelineState;
};

struct MTL_COMPILED_TESSELLATION_PIPELINE {
  MTL::RenderPipelineState *MeshPipelineState;
  MTL::RenderPipelineState *RasterizationPipelineState;
  uint32_t NumControlPointOutputElement;
  uint32_t NumPatchConstantOutputScalar;
  uint32_t ThreadsPerPatch;
};

DEFINE_COM_INTERFACE("7ee15804-8604-41fc-ad0c-4ecf97e2e6fe",
                     IMTLCompiledGraphicsPipeline)
    : public IMTLThreadpoolWork {
  virtual void SubmitWork() = 0;
  virtual bool IsReady() = 0;
  /**
  NOTE: the current thread is blocked if it's not ready
   */
  virtual void GetPipeline(MTL_COMPILED_GRAPHICS_PIPELINE *
                           pGraphicsPipeline) = 0;
};

DEFINE_COM_INTERFACE("3b26b8d3-56ca-4d0f-9f63-ca8d305ff07e",
                     IMTLCompiledComputePipeline)
    : public IMTLThreadpoolWork {
  virtual void SubmitWork() = 0;
  virtual bool IsReady() = 0;
  virtual void GetPipeline(MTL_COMPILED_COMPUTE_PIPELINE *
                           pComputePipeline) = 0;
};

DEFINE_COM_INTERFACE("f5075e27-fd85-4c5a-9031-d438f859e6e9",
                     IMTLCompiledTessellationPipeline)
    : public IMTLThreadpoolWork {
  virtual void SubmitWork() = 0;
  virtual bool IsReady() = 0;
  virtual void GetPipeline(MTL_COMPILED_TESSELLATION_PIPELINE *
                           pTessellationPipeline) = 0;
};

namespace std {
template <> struct hash<MTL_GRAPHICS_PIPELINE_DESC> {
  size_t operator()(const MTL_GRAPHICS_PIPELINE_DESC &v) const noexcept {
    dxmt::HashState state;
    state.add((size_t)v.VertexShader);
    state.add((size_t)v.PixelShader);
    state.add((size_t)v.HullShader);
    state.add((size_t)v.DomainShader);
    /* these pointers is safe to be used as hash input */
    state.add((size_t)v.InputLayout);
    state.add((size_t)v.BlendState);
    state.add((size_t)v.DepthStencilFormat);
    state.add((size_t)v.IndexBufferFormat);
    state.add((size_t)v.SampleMask);
    state.add((size_t)v.NumColorAttachments);
    for (unsigned i = 0; i < std::size(v.ColorAttachmentFormats); i++) {
      state.add(i < v.NumColorAttachments ? v.ColorAttachmentFormats[i]
                                          : MTL::PixelFormatInvalid);
    }
    return state;
  };
};
template <> struct equal_to<MTL_GRAPHICS_PIPELINE_DESC> {
  bool operator()(const MTL_GRAPHICS_PIPELINE_DESC &x,
                  const MTL_GRAPHICS_PIPELINE_DESC &y) const {
    if (x.NumColorAttachments != y.NumColorAttachments)
      return false;
    for (unsigned i = 0; i < x.NumColorAttachments; i++) {
      if (x.ColorAttachmentFormats[i] != y.ColorAttachmentFormats[i])
        return false;
    }
    return (x.BlendState == y.BlendState) &&
           (x.VertexShader == y.VertexShader) &&
           (x.PixelShader == y.PixelShader) && (x.HullShader == y.HullShader) &&
           (x.DomainShader == y.DomainShader) &&
           (x.InputLayout == y.InputLayout) &&
           (x.DepthStencilFormat == y.DepthStencilFormat) &&
           (x.RasterizationEnabled == y.RasterizationEnabled) &&
           (x.IndexBufferFormat == y.IndexBufferFormat)&&
           (x.SampleMask == y.SampleMask);
  }
};
} // namespace std

namespace dxmt {

Com<IMTLCompiledGraphicsPipeline> CreateGraphicsPipeline(
    IMTLD3D11Device *pDevice, MTL_GRAPHICS_PIPELINE_DESC* pDesc);

Com<IMTLCompiledComputePipeline>
CreateComputePipeline(IMTLD3D11Device *pDevice,
                      IMTLCompiledShader *pComputeShader);

Com<IMTLCompiledTessellationPipeline>
CreateTessellationPipeline(IMTLD3D11Device *pDevice,
                           MTL_GRAPHICS_PIPELINE_DESC *pDesc);

}; // namespace dxmt