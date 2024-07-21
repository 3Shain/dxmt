#pragma once

#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "com/com_guid.hpp"
#include "d3d11_device.hpp"
#include "d3d11_input_layout.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_state_object.hpp"

struct MTL_COMPILED_GRAPHICS_PIPELINE {
  MTL::RenderPipelineState *PipelineState = nullptr;
};

struct MTL_COMPILED_COMPUTE_PIPELINE {
  MTL::ComputePipelineState *PipelineState;
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

namespace dxmt {

Com<IMTLCompiledGraphicsPipeline> CreateGraphicsPipeline(
    IMTLD3D11Device *pDevice, IMTLCompiledShader *pVertexShader,
    IMTLCompiledShader *pPixelShader, IMTLD3D11InputLayout *pInputLayout,
    IMTLD3D11BlendState *pBlendState, UINT NumRTVs,
    MTL::PixelFormat const *RTVFormats, MTL::PixelFormat DepthStencilFormat);

Com<IMTLCompiledComputePipeline>
CreateComputePipeline(IMTLD3D11Device *pDevice,
                      IMTLCompiledShader *pComputeShader);

}; // namespace dxmt