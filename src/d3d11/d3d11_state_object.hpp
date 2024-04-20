#pragma once

#include "Metal/MTLRenderPipeline.hpp"
#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_1.h"

DEFINE_COM_INTERFACE("77f0bbd5-2be7-4e9e-ad61-70684ff19e01",
                     IMTLD3D11SamplerState)
    : public ID3D11SamplerState {
  virtual MTL::SamplerState *GetSamplerState() = 0;
};

DEFINE_COM_INTERFACE("03629ed8-bcdd-4582-8997-3817209a34f4",
                     IMTLD3D11RasterizerState)
    : public ID3D11RasterizerState1 {
  virtual void SetupRasterizerState(MTL::RenderCommandEncoder * encoder) = 0;
};

DEFINE_COM_INTERFACE("b01aaffa-b4d3-478a-91be-6195f215aaba",
                     IMTLD3D11DepthStencilState)
    : public ID3D11DepthStencilState {
  virtual MTL::DepthStencilState *GetDepthStencilState() = 0;
};

DEFINE_COM_INTERFACE("279a1d66-2fc1-460c-a0a7-a7a5f2b7a48f",
                     IMTLD3D11BlendState)
    : public ID3D11BlendState1 {
  virtual void SetupMetalPipelineDescriptor(MTL::RenderPipelineDescriptor *
                                            render_pipeline_descriptor) = 0;
};

namespace dxmt {

HRESULT CreateSamplerState(ID3D11Device *pDevice,
                           const D3D11_SAMPLER_DESC *pSamplerDesc,
                           ID3D11SamplerState **ppSamplerState);

HRESULT CreateBlendState(ID3D11Device *pDevice,
                         const D3D11_BLEND_DESC1 *pBlendStateDesc,
                         ID3D11BlendState1 **ppBlendState);

HRESULT CreateDepthStencilState(ID3D11Device *pDevice,
                                const D3D11_DEPTH_STENCIL_DESC *pDesc,
                                ID3D11DepthStencilState **ppDepthStencilState);

HRESULT CreateRasterizerState(ID3D11Device *pDevice,
                              const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
                              ID3D11RasterizerState1 **ppRasterizerState);

Com<IMTLD3D11RasterizerState>
CreateDefaultRasterizerState(ID3D11Device *pDevice);

Com<IMTLD3D11DepthStencilState>
CreateDefaultDepthStencilState(ID3D11Device *pDevice);

Com<IMTLD3D11BlendState>
CreateDefaultBlendState(ID3D11Device *pDevice);

} // namespace dxmt