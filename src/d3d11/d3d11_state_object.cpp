#include "d3d11_state_object.hpp"
#include "d3d11_device.hpp"

namespace dxmt {

#pragma region SamplerState

//-----------------------------------------------------------------------------
// Sampler State
//-----------------------------------------------------------------------------

MTLD3D11SamplerState::MTLD3D11SamplerState(IMTLD3D11Device *device,
                                           MTL::SamplerState *sampler_state,
                                           const D3D11_SAMPLER_DESC &desc)
    : MTLD3D11StateObject<ID3D11SamplerState>(device), desc_(desc),
      metal_sampler_state_(sampler_state) {}

MTLD3D11SamplerState::~MTLD3D11SamplerState() {}

HRESULT STDMETHODCALLTYPE
MTLD3D11SamplerState::QueryInterface(REFIID riid, void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
      riid == __uuidof(ID3D11SamplerState)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(ID3D11SamplerState), riid)) {
    Logger::warn("D3D11SamplerState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
  }

  return E_NOINTERFACE;
}

void STDMETHODCALLTYPE
MTLD3D11SamplerState::GetDesc(D3D11_SAMPLER_DESC *pDesc) {
  *pDesc = desc_;
}

#pragma endregion

#pragma region BlendState

//-----------------------------------------------------------------------------
// Blend State
//-----------------------------------------------------------------------------
MTLD3D11BlendState::MTLD3D11BlendState(IMTLD3D11Device *device,
                                       const D3D11_BLEND_DESC1 &desc)
    : MTLD3D11StateObject<ID3D11BlendState1>(device), desc_(desc) {}

MTLD3D11BlendState::~MTLD3D11BlendState() {}

void STDMETHODCALLTYPE MTLD3D11BlendState::GetDesc(D3D11_BLEND_DESC *pDesc) {
  pDesc->IndependentBlendEnable = desc_.IndependentBlendEnable;
  pDesc->AlphaToCoverageEnable = desc_.AlphaToCoverageEnable;
  for (size_t i = 0; i < 8; i++) {
    pDesc->RenderTarget[i].RenderTargetWriteMask =
        desc_.RenderTarget[i].RenderTargetWriteMask;
    pDesc->RenderTarget[i].BlendEnable = desc_.RenderTarget[i].BlendEnable;
    pDesc->RenderTarget[i].BlendOp = desc_.RenderTarget[i].BlendOp;
    pDesc->RenderTarget[i].BlendOpAlpha = desc_.RenderTarget[i].BlendOpAlpha;
    pDesc->RenderTarget[i].SrcBlend = desc_.RenderTarget[i].SrcBlend;
    pDesc->RenderTarget[i].DestBlend = desc_.RenderTarget[i].SrcBlendAlpha;
    pDesc->RenderTarget[i].SrcBlendAlpha = desc_.RenderTarget[i].DestBlend;
    pDesc->RenderTarget[i].DestBlendAlpha =
        desc_.RenderTarget[i].DestBlendAlpha;
  }
}

void STDMETHODCALLTYPE MTLD3D11BlendState::GetDesc1(D3D11_BLEND_DESC1 *pDesc) {
  *pDesc = desc_;
}

HRESULT STDMETHODCALLTYPE MTLD3D11BlendState::QueryInterface(const IID &riid,
                                                             void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
      riid == __uuidof(ID3D11DepthStencilState)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(ID3D11DepthStencilState), riid)) {
    WARN("Unknown interface query", str::format(riid));
  }

  return E_NOINTERFACE;
}

void MTLD3D11BlendState::SetupMetalPipelineDescriptor(
    MTL::RenderPipelineDescriptor *render_pipeline_descriptor) {
  for (unsigned rt = 0; rt < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; rt++) {
    auto i = desc_.IndependentBlendEnable ? rt : 0;
    if (desc_.RenderTarget[i].LogicOpEnable) {
      ERR("OutputMerger LogicOp is not supported");
      continue;
    }
    auto attachment_desc =
        render_pipeline_descriptor->colorAttachments()->object(rt);
    attachment_desc->setAlphaBlendOperation(
        g_blend_op_map[desc_.RenderTarget[i].BlendOpAlpha]);
    attachment_desc->setRgbBlendOperation(
        g_blend_op_map[desc_.RenderTarget[i].BlendOp]);
    attachment_desc->setBlendingEnabled(desc_.RenderTarget[i].BlendEnable);
    attachment_desc->setSourceAlphaBlendFactor(
        g_blend_factor_map[desc_.RenderTarget[i].SrcBlendAlpha]);
    attachment_desc->setSourceRGBBlendFactor(
        g_blend_factor_map[desc_.RenderTarget[i].SrcBlend]);
    attachment_desc->setDestinationAlphaBlendFactor(
        g_blend_factor_map[desc_.RenderTarget[i].DestBlendAlpha]);
    attachment_desc->setDestinationRGBBlendFactor(
        g_blend_factor_map[desc_.RenderTarget[i].DestBlend]);
    attachment_desc->setWriteMask(
        g_color_write_mask_map[desc_.RenderTarget[i].RenderTargetWriteMask]);
  }
  render_pipeline_descriptor->setAlphaToCoverageEnabled(
      desc_.AlphaToCoverageEnable);
}

#pragma endregion

#pragma region RasterizerState

//-----------------------------------------------------------------------------
// Rasterizer State
//-----------------------------------------------------------------------------

MTLD3D11RasterizerState::MTLD3D11RasterizerState(
    IMTLD3D11Device *device, const D3D11_RASTERIZER_DESC1 &desc)
    : MTLD3D11StateObject<ID3D11RasterizerState1>(device), m_desc(desc) {}

MTLD3D11RasterizerState::~MTLD3D11RasterizerState() {}

HRESULT
STDMETHODCALLTYPE
MTLD3D11RasterizerState::QueryInterface(REFIID riid, void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
      riid == __uuidof(ID3D11RasterizerState) ||
      riid == __uuidof(ID3D11RasterizerState1)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(ID3D11RasterizerState), riid)) {
    Logger::warn(
        "D3D11RasterizerState::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
  }

  return E_NOINTERFACE;
}

void STDMETHODCALLTYPE
MTLD3D11RasterizerState::GetDesc(D3D11_RASTERIZER_DESC *pDesc) {
  pDesc->FillMode = m_desc.FillMode;
  pDesc->CullMode = m_desc.CullMode;
  pDesc->FrontCounterClockwise = m_desc.FrontCounterClockwise;
  pDesc->DepthBias = m_desc.DepthBias;
  pDesc->DepthBiasClamp = m_desc.DepthBiasClamp;
  pDesc->SlopeScaledDepthBias = m_desc.SlopeScaledDepthBias;
  pDesc->DepthClipEnable = m_desc.DepthClipEnable;
  pDesc->ScissorEnable = m_desc.ScissorEnable;
  pDesc->MultisampleEnable = m_desc.MultisampleEnable;
  pDesc->AntialiasedLineEnable = m_desc.AntialiasedLineEnable;
}

void STDMETHODCALLTYPE
MTLD3D11RasterizerState::GetDesc1(D3D11_RASTERIZER_DESC1 *pDesc) {
  *pDesc = m_desc;
}

void STDMETHODCALLTYPE MTLD3D11RasterizerState::SetupRasterizerState(
    MTL::RenderCommandEncoder *encoder) {

  if (m_desc.FillMode == D3D11_FILL_SOLID) {
    encoder->setTriangleFillMode(MTL::TriangleFillMode::TriangleFillModeFill);
  } else {
    encoder->setTriangleFillMode(MTL::TriangleFillMode::TriangleFillModeLines);
  }

  switch (m_desc.CullMode) {
  case D3D11_CULL_BACK:
    encoder->setCullMode(MTL::CullModeBack);
    break;
  case D3D11_CULL_FRONT:
    encoder->setCullMode(MTL::CullModeFront);
    break;
  case D3D11_CULL_NONE:
    encoder->setCullMode(MTL::CullModeNone);
    break;
  }

  // https://github.com/gpuweb/gpuweb/issues/2100#issuecomment-924536243
  if (m_desc.DepthClipEnable) {
    encoder->setDepthClipMode(MTL::DepthClipMode::DepthClipModeClip);
  } else {
    encoder->setDepthClipMode(MTL::DepthClipMode::DepthClipModeClamp);
  }

  encoder->setDepthBias(m_desc.DepthBias, m_desc.SlopeScaledDepthBias,
                        m_desc.DepthBiasClamp);

  if (m_desc.FrontCounterClockwise) {
    encoder->setFrontFacingWinding(MTL::Winding::WindingCounterClockwise);
  }

  if (m_desc.AntialiasedLineEnable) {
    // nop , we don't support this
  }

  if (m_desc.MultisampleEnable) {
    // used in combination with AntialiasedLineEnable, not supported yet.
  }

  // m_desc.ScissorEnable : handled outside

  // m_desc.ForcedSampleCount : not supported?
}

bool MTLD3D11RasterizerState::IsScissorEnabled() {
  return m_desc.ScissorEnable;
}

#pragma endregion

#pragma region DepthStencilState
//-----------------------------------------------------------------------------
// Depth Stencil State
//-----------------------------------------------------------------------------
MTLD3D11DepthStencilState::MTLD3D11DepthStencilState(
    IMTLD3D11Device *device, MTL::DepthStencilState *state,
    const D3D11_DEPTH_STENCIL_DESC &desc)
    : MTLD3D11StateObject<ID3D11DepthStencilState>(device), m_desc(desc),
      metal_depth_stencil_state_(state) {}

MTLD3D11DepthStencilState::~MTLD3D11DepthStencilState() {}

void STDMETHODCALLTYPE
MTLD3D11DepthStencilState::GetDesc(D3D11_DEPTH_STENCIL_DESC *pDesc) {
  *pDesc = m_desc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D11DepthStencilState::QueryInterface(const IID &riid, void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
      riid == __uuidof(ID3D11DepthStencilState)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(ID3D11DepthStencilState), riid)) {
    WARN("Unknown interface query", str::format(riid));
  }

  return E_NOINTERFACE;
}

void STDMETHODCALLTYPE MTLD3D11DepthStencilState::SetupDepthStencilState(
    MTL::RenderCommandEncoder *encoder) {
  encoder->setDepthStencilState(metal_depth_stencil_state_.ptr());
}

#pragma endregion

} // namespace dxmt
