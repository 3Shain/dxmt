#pragma once

#include "Metal.hpp"
#include "com/com_guid.hpp"
#include "d3d11_device.hpp"
#include "d3d11_input_layout.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_state_object.hpp"
#include "util_hash.hpp"

struct MTL_GRAPHICS_PIPELINE_DESC {
  ManagedShader VertexShader;
  ManagedShader HullShader;
  ManagedShader DomainShader;
  ManagedShader GeometryShader;
  ManagedShader PixelShader;
  IMTLD3D11BlendState *BlendState;
  ManagedInputLayout InputLayout;
  IMTLD3D11StreamOutputLayout *SOLayout;
  UINT NumColorAttachments;
  WMTPixelFormat ColorAttachmentFormats[8];
  WMTPixelFormat DepthStencilFormat;
  WMTPrimitiveTopologyClass TopologyClass;
  bool RasterizationEnabled;
  uint8_t SampleCount;
  bool GSStripTopology;
  SM50_INDEX_BUFFER_FORAMT IndexBufferFormat;
  uint32_t SampleMask;
  uint32_t GSPassthrough;
};

struct MTL_COMPUTE_PIPELINE_DESC {
  ManagedShader ComputeShader;
};

struct MTL_COMPILED_GRAPHICS_PIPELINE {
  WMT::RenderPipelineState PipelineState;
};

struct MTL_COMPILED_COMPUTE_PIPELINE {
  WMT::ComputePipelineState PipelineState;
};

struct MTL_COMPILED_TESSELLATION_PIPELINE {
  WMT::RenderPipelineState MeshPipelineState;
  WMT::RenderPipelineState RasterizationPipelineState;
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

#ifdef DXMT_NATIVE
DEFINE_GUID(IID_IMTLCompiledGraphicsPipeline, 0x7ee15804, 0x8604, 0x41fc, 0xad, 0x0c, 0x4e, 0xcf, 0x97, 0xe2, 0xe6, 0xfe);
__CRT_UUID_DECL(IMTLCompiledGraphicsPipeline, 0x7ee15804, 0x8604, 0x41fc, 0xad, 0x0c, 0x4e, 0xcf, 0x97, 0xe2, 0xe6, 0xfe);
#endif

DEFINE_COM_INTERFACE("3b26b8d3-56ca-4d0f-9f63-ca8d305ff07e",
                     IMTLCompiledComputePipeline)
    : public IMTLThreadpoolWork {
  virtual void SubmitWork() = 0;
  virtual bool IsReady() = 0;
  virtual void GetPipeline(MTL_COMPILED_COMPUTE_PIPELINE *
                           pComputePipeline) = 0;
};

#ifdef DXMT_NATIVE
DEFINE_GUID(IID_IMTLCompiledComputePipeline, 0x3b26b8d3, 0x56ca, 0x4d0f, 0x9f, 0x63, 0xca, 0x8d, 0x30, 0x5f, 0xf0, 0x7e);
__CRT_UUID_DECL(IMTLCompiledComputePipeline, 0x3b26b8d3, 0x56ca, 0x4d0f, 0x9f, 0x63, 0xca, 0x8d, 0x30, 0x5f, 0xf0, 0x7e);
#endif

DEFINE_COM_INTERFACE("f5075e27-fd85-4c5a-9031-d438f859e6e9",
                     IMTLCompiledTessellationPipeline)
    : public IMTLThreadpoolWork {
  virtual void SubmitWork() = 0;
  virtual bool IsReady() = 0;
  virtual void GetPipeline(MTL_COMPILED_TESSELLATION_PIPELINE *
                           pTessellationPipeline) = 0;
};

#ifdef DXMT_NATIVE
DEFINE_GUID(IID_IMTLCompiledTessellationPipeline, 0xf5075e27, 0xfd85, 0x4c5a, 0x90, 0x31, 0xd4, 0x38, 0xf8, 0x59, 0xe6, 0xe9);
__CRT_UUID_DECL(IMTLCompiledTessellationPipeline, 0xf5075e27, 0xfd85, 0x4c5a, 0x90, 0x31, 0xd4, 0x38, 0xf8, 0x59, 0xe6, 0xe9);
#endif

DEFINE_COM_INTERFACE("0a86aadc-260d-40a0-afed-659408a84ffb",
                     IMTLCompiledGeometryPipeline)
    : public IMTLThreadpoolWork {
  virtual void SubmitWork() = 0;
  virtual bool IsReady() = 0;
  virtual void GetPipeline(MTL_COMPILED_GRAPHICS_PIPELINE *
                           pGeometryPipeline) = 0;
};

#ifdef DXMT_NATIVE
DEFINE_GUID(IID_IMTLCompiledGeometryPipeline, 0x0a86aadc, 0x260d, 0x40a0, 0xaf, 0xed, 0x65, 0x94, 0x08, 0xa8, 0x4f, 0xfb);
__CRT_UUID_DECL(IMTLCompiledGeometryPipeline, 0x0a86aadc, 0x260d, 0x40a0, 0xaf, 0xed, 0x65, 0x94, 0x08, 0xa8, 0x4f, 0xfb);
#endif

namespace std {
template <> struct hash<MTL_GRAPHICS_PIPELINE_DESC> {
  size_t operator()(const MTL_GRAPHICS_PIPELINE_DESC &v) const noexcept {
    dxmt::HashState state;
    state.add((size_t)v.VertexShader);
    state.add((size_t)v.PixelShader);
    state.add((size_t)v.HullShader);
    state.add((size_t)v.DomainShader);
    state.add((size_t)v.GeometryShader);
    state.add((size_t)v.InputLayout);
    /* don't add blend */
    // state.add((size_t)v.BlendState);
    state.add((size_t)v.DepthStencilFormat);
    state.add((size_t)v.TopologyClass);
    state.add((size_t)v.IndexBufferFormat);
    state.add((size_t)v.GSStripTopology);
    state.add((size_t)v.SampleMask);
    state.add((size_t)v.GSPassthrough);
    state.add((size_t)v.SampleCount);
    state.add((size_t)v.NumColorAttachments);
    for (unsigned i = 0; i < v.NumColorAttachments; i++) {
      state.add(v.ColorAttachmentFormats[i]);
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
    if (x.BlendState != y.BlendState) {
      D3D11_BLEND_DESC1 x_, y_;
      x.BlendState->GetDesc1(&x_);
      y.BlendState->GetDesc1(&y_);
      if (x_.IndependentBlendEnable != y_.IndependentBlendEnable)
        return false;
      if (x_.AlphaToCoverageEnable != y_.AlphaToCoverageEnable)
        return false;
      uint32_t num_blend_target =
          x_.IndependentBlendEnable ? x.NumColorAttachments : 1;
      for (unsigned i = 0; i < num_blend_target; i++) {
        auto &blend_target_x = x_.RenderTarget[i];
        auto &blend_target_y = y_.RenderTarget[i];
        if (blend_target_x.RenderTargetWriteMask !=
            blend_target_y.RenderTargetWriteMask)
          return false;
        if (blend_target_x.BlendEnable != blend_target_y.BlendEnable)
          return false;
        if (blend_target_x.LogicOpEnable != blend_target_y.LogicOpEnable)
          return false;
        if (blend_target_x.BlendEnable) {
          if (blend_target_x.BlendOp != blend_target_y.BlendOp)
            return false;
          if (blend_target_x.BlendOpAlpha != blend_target_y.BlendOpAlpha)
            return false;
          if (blend_target_x.SrcBlend != blend_target_y.SrcBlend)
            return false;
          if (blend_target_x.SrcBlendAlpha != blend_target_y.SrcBlendAlpha)
            return false;
          if (blend_target_x.DestBlend != blend_target_y.DestBlend)
            return false;
          if (blend_target_x.DestBlendAlpha != blend_target_y.DestBlendAlpha)
            return false;
        }
        if (blend_target_x.LogicOpEnable) {
          if (blend_target_x.LogicOp != blend_target_y.LogicOp)
            return false;
        }
      }
    }
    return (x.VertexShader == y.VertexShader) &&
           (x.PixelShader == y.PixelShader) && (x.HullShader == y.HullShader) &&
           (x.DomainShader == y.DomainShader) && 
           (x.GeometryShader == y.GeometryShader) &&
           (x.InputLayout == y.InputLayout) &&
           (x.DepthStencilFormat == y.DepthStencilFormat) &&
           (x.TopologyClass == y.TopologyClass) &&
           (x.RasterizationEnabled == y.RasterizationEnabled) &&
           (x.GSStripTopology == y.GSStripTopology) &&
           (x.SampleCount == y.SampleCount) &&
           (x.IndexBufferFormat == y.IndexBufferFormat) &&
           (x.SampleMask == y.SampleMask) &&
           (x.GSPassthrough == y.GSPassthrough);
  }
};
} // namespace std

namespace dxmt {

Com<IMTLCompiledGraphicsPipeline> CreateGraphicsPipeline(
    MTLD3D11Device *pDevice, MTL_GRAPHICS_PIPELINE_DESC* pDesc);

Com<IMTLCompiledComputePipeline>
CreateComputePipeline(MTLD3D11Device *pDevice, ManagedShader ComputeShader);

Com<IMTLCompiledTessellationPipeline>
CreateTessellationPipeline(MTLD3D11Device *pDevice,
                           MTL_GRAPHICS_PIPELINE_DESC *pDesc);

Com<IMTLCompiledGeometryPipeline>
CreateGeometryPipeline(MTLD3D11Device *pDevice,
                           MTL_GRAPHICS_PIPELINE_DESC *pDesc);

}; // namespace dxmt