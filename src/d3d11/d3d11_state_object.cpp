#include "d3d11_private.h"
#include "d3d11_state_object.hpp"
#include "Metal/MTLDepthStencil.hpp"
#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include "d3d11_device_child.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLSampler.hpp"
#include "objc_pointer.hpp"
#include <cassert>

namespace dxmt {

constexpr MTL::CompareFunction kCompareFunctionMap[] = {
    MTL::CompareFunction::CompareFunctionNever, // padding 0
    MTL::CompareFunction::CompareFunctionNever, // 1 - 1
    MTL::CompareFunction::CompareFunctionLess,
    MTL::CompareFunction::CompareFunctionEqual,
    MTL::CompareFunction::CompareFunctionLessEqual,
    MTL::CompareFunction::CompareFunctionGreater,
    MTL::CompareFunction::CompareFunctionNotEqual,
    MTL::CompareFunction::CompareFunctionGreaterEqual,
    MTL::CompareFunction::CompareFunctionAlways // 8 - 1
};

constexpr MTL::SamplerAddressMode kAddressModeMap[] = {
    // MTL: Texture coordinates wrap to the other side of the texture,
    // effectively keeping only the fractional part of the texture coordinate.

    // MSDN: Tile the texture at every (u,v) integer junction. For example, for
    // u values between 0 and 3, the texture is repeated three times.
    MTL::SamplerAddressMode::SamplerAddressModeRepeat, // 1 - 1

    // MTL:Between -1.0 and 1.0, the texture coordinates are mirrored across the
    // axis; outside -1.0 and 1.0, the image is repeated.

    // MSDN: Flip the texture at every (u,v) integer junction. For u values
    // between 0 and 1, for example, the texture is addressed normally; between
    // 1 and 2, the texture is flipped (mirrored); between 2 and 3, the texture
    // is normal again; and so on.
    MTL::SamplerAddressMode::SamplerAddressModeMirrorRepeat,
    // MTL: Texture coordinates are clamped between 0.0 and 1.0, inclusive.
    // MSDN: Texture coordinates outside the range [0.0, 1.0] are set to the
    // texture color at 0.0 or 1.0, respectively.
    MTL::SamplerAddressMode::SamplerAddressModeClampToEdge,

    // MTL: Out-of-range texture coordinates return the value specified by the
    // borderColor property.

    // MSDN: Texture coordinates outside the range [0.0, 1.0] are set to the
    // border color specified in D3D11_SAMPLER_DESC or HLSL code.
    MTL::SamplerAddressMode::SamplerAddressModeClampToBorderColor,
    // MTL: Between -1.0 and 1.0, the texture coordinates are mirrored across
    // the axis; outside -1.0 and 1.0, texture coordinates are clamped.

    // MSDN:
    //  Similar to D3D11_TEXTURE_ADDRESS_MIRROR and D3D11_TEXTURE_ADDRESS_CLAMP.
    //  Takes the absolute value of the texture coordinate (thus, mirroring
    //  around 0), and then clamps to the maximum value.
    MTL::SamplerAddressMode::SamplerAddressModeMirrorClampToEdge // 5 - 1
};

constexpr MTL::StencilOperation kStencilOperationMap[] = {
    MTL::StencilOperationZero, // invalid
    MTL::StencilOperationKeep,
    MTL::StencilOperationZero,
    MTL::StencilOperationReplace,
    // D3D11_STENCIL_OP_INCR_SAT: Increment the stencil value by 1, and clamp
    // the result.
    MTL::StencilOperationIncrementClamp,
    MTL::StencilOperationDecrementClamp,
    MTL::StencilOperationInvert,
    // D3D11_STENCIL_OP_INCR:Increment the stencil value by 1, and wrap the
    // result if necessary.

    MTL::StencilOperationIncrementWrap,
    MTL::StencilOperationDecrementWrap,

};

constexpr MTL::BlendOperation kBlendOpMap[] = {
    MTL::BlendOperationAdd, // padding 0
    MTL::BlendOperationAdd,
    MTL::BlendOperationSubtract,
    MTL::BlendOperationReverseSubtract,
    MTL::BlendOperationMin,
    MTL::BlendOperationMax,
};

constexpr MTL::LogicOperation kLogicOpMap[] = {
    MTL::LogicOperationClear,      MTL::LogicOperationSet,
    MTL::LogicOperationCopy,       MTL::LogicOperationCopyInverted,
    MTL::LogicOperationNoOp,       MTL::LogicOperationInvert,
    MTL::LogicOperationAnd,        MTL::LogicOperationNand,
    MTL::LogicOperationOr,         MTL::LogicOperationNor,
    MTL::LogicOperationXor,        MTL::LogicOperationEquiv,
    MTL::LogicOperationAndReverse, MTL::LogicOperationAndInverted,
    MTL::LogicOperationOrReverse,  MTL::LogicOperationOrInverted,
};

constexpr MTL::BlendFactor kBlendFactorMap[] = {
    MTL::BlendFactorZero, // padding 0
    MTL::BlendFactorZero,
    MTL::BlendFactorOne,
    MTL::BlendFactorSourceColor,
    MTL::BlendFactorOneMinusSourceColor,
    MTL::BlendFactorSourceAlpha,
    MTL::BlendFactorOneMinusSourceAlpha,
    MTL::BlendFactorDestinationAlpha,
    MTL::BlendFactorOneMinusDestinationAlpha,
    MTL::BlendFactorDestinationColor,
    MTL::BlendFactorOneMinusDestinationColor,
    MTL::BlendFactorSourceAlphaSaturated,
    MTL::BlendFactorZero,       // invalid,12
    MTL::BlendFactorZero,       // invalid,13
    MTL::BlendFactorBlendAlpha, // BLEND_FACTOR
    MTL::BlendFactorOneMinusBlendAlpha,
    MTL::BlendFactorSource1Color,
    MTL::BlendFactorOneMinusSource1Color,
    MTL::BlendFactorSource1Alpha,
    MTL::BlendFactorOneMinusSource1Alpha,
};

constexpr MTL::ColorWriteMask kColorWriteMaskMap[] = {
    // 0000
    0,
    // 0001
    MTL::ColorWriteMaskRed,
    // 0010
    MTL::ColorWriteMaskGreen,
    // 0011,
    MTL::ColorWriteMaskRed | MTL::ColorWriteMaskGreen,
    // 0100
    MTL::ColorWriteMaskBlue,
    // 0101
    MTL::ColorWriteMaskBlue | MTL::ColorWriteMaskRed,
    // 0110
    MTL::ColorWriteMaskBlue | MTL::ColorWriteMaskGreen,
    // 0111
    MTL::ColorWriteMaskBlue | MTL::ColorWriteMaskRed | MTL::ColorWriteMaskGreen,

    // 1000
    MTL::ColorWriteMaskAlpha | 0,
    // 1001
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskRed,
    // 1010
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskGreen,
    // 1011,
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskRed |
        MTL::ColorWriteMaskGreen,
    // 1100
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskBlue,
    // 0101
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskBlue | MTL::ColorWriteMaskRed,
    // 1110
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskBlue |
        MTL::ColorWriteMaskGreen,
    // 1111
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskBlue |
        MTL::ColorWriteMaskRed | MTL::ColorWriteMaskGreen,
};

class MTLD3D11SamplerState : public ManagedDeviceChild<IMTLD3D11SamplerState> {

public:
  friend class MTLD3D11DeviceContext;
  MTLD3D11SamplerState(IMTLD3D11Device *device, MTL::SamplerState *samplerState,
                       const D3D11_SAMPLER_DESC &desc)
      : ManagedDeviceChild<IMTLD3D11SamplerState>(device), desc_(desc),
        metal_sampler_state_(samplerState),
        argument_handle_(samplerState->gpuResourceID()._impl) {}
  ~MTLD3D11SamplerState() {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11SamplerState) ||
        riid == __uuidof(IMTLD3D11SamplerState)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IMTLD3D11SamplerState), riid)) {
      WARN("D3D11SamplerState: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void STDMETHODCALLTYPE GetDesc(D3D11_SAMPLER_DESC *pDesc) final {
    *pDesc = desc_;
  }

  virtual MTL::SamplerState *GetSamplerState() {
    return metal_sampler_state_.ptr();
  }

  virtual uint64_t GetArgumentHandle() { return argument_handle_; }

private:
  const D3D11_SAMPLER_DESC desc_;
  Obj<MTL::SamplerState> metal_sampler_state_;
  uint64_t argument_handle_;
};

// BlendState

class MTLD3D11BlendState : public ManagedDeviceChild<IMTLD3D11BlendState> {

public:
  friend class MTLD3D11DeviceContext;
  MTLD3D11BlendState(IMTLD3D11Device *device, const D3D11_BLEND_DESC1 &desc)
      : ManagedDeviceChild<IMTLD3D11BlendState>(device), desc_(desc) {
    dual_source_blending_ =
        !desc_.IndependentBlendEnable &&
        (desc_.RenderTarget[0].SrcBlend >= D3D11_BLEND_SRC1_COLOR ||
         desc_.RenderTarget[0].SrcBlendAlpha >= D3D11_BLEND_SRC1_COLOR ||
         desc_.RenderTarget[0].DestBlendAlpha >= D3D11_BLEND_SRC1_COLOR ||
         desc_.RenderTarget[0].DestBlend >= D3D11_BLEND_SRC1_COLOR);
  }
  ~MTLD3D11BlendState() {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11BlendState) ||
        riid == __uuidof(IMTLD3D11BlendState)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IMTLD3D11BlendState), riid)) {
      WARN("D3D11BlendState: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void STDMETHODCALLTYPE GetDesc(D3D11_BLEND_DESC *pDesc) final {
    pDesc->IndependentBlendEnable = desc_.IndependentBlendEnable;
    pDesc->AlphaToCoverageEnable = desc_.AlphaToCoverageEnable;
    for (size_t i = 0; i < 8; i++) {
      auto &renderTarget = desc_.RenderTarget[i];
      pDesc->RenderTarget[i].RenderTargetWriteMask =
          renderTarget.RenderTargetWriteMask;
      pDesc->RenderTarget[i].BlendEnable = renderTarget.BlendEnable;
      pDesc->RenderTarget[i].BlendOp = renderTarget.BlendOp;
      pDesc->RenderTarget[i].BlendOpAlpha = renderTarget.BlendOpAlpha;
      pDesc->RenderTarget[i].SrcBlend = renderTarget.SrcBlend;
      pDesc->RenderTarget[i].DestBlend = renderTarget.SrcBlendAlpha;
      pDesc->RenderTarget[i].SrcBlendAlpha = renderTarget.DestBlend;
      pDesc->RenderTarget[i].DestBlendAlpha = renderTarget.DestBlendAlpha;
    }
  }

  void STDMETHODCALLTYPE GetDesc1(D3D11_BLEND_DESC1 *pDesc) final {
    *pDesc = desc_;
  }

  bool IsDualSourceBlending() { return dual_source_blending_; }

  void SetupMetalPipelineDescriptor(
      MTL::RenderPipelineDescriptor *render_pipeline_descriptor) {
    for (unsigned rt = 0; rt < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; rt++) {
      auto i = desc_.IndependentBlendEnable ? rt : 0;
      auto &renderTarget = desc_.RenderTarget[i];
      if (renderTarget.LogicOpEnable) {
#ifdef DXMT_NO_PRIVATE_API
        ERR("OutputMerger LogicOp is not supported");
        continue;
#else
        render_pipeline_descriptor->setLogicOperationEnabled(true);
        render_pipeline_descriptor->setLogicOperation(
            kLogicOpMap[renderTarget.LogicOp]);
#endif
      }
      auto attachment_desc =
          render_pipeline_descriptor->colorAttachments()->object(rt);
      attachment_desc->setAlphaBlendOperation(
          kBlendOpMap[renderTarget.BlendOpAlpha]);
      attachment_desc->setRgbBlendOperation(kBlendOpMap[renderTarget.BlendOp]);
      attachment_desc->setBlendingEnabled(renderTarget.BlendEnable);
      if (renderTarget.BlendEnable) {
        attachment_desc->setSourceAlphaBlendFactor(
            kBlendFactorMap[renderTarget.SrcBlendAlpha]);
        attachment_desc->setSourceRGBBlendFactor(
            kBlendFactorMap[renderTarget.SrcBlend]);
        attachment_desc->setDestinationAlphaBlendFactor(
            kBlendFactorMap[renderTarget.DestBlendAlpha]);
        attachment_desc->setDestinationRGBBlendFactor(
            kBlendFactorMap[renderTarget.DestBlend]);
      }
      attachment_desc->setWriteMask(
          kColorWriteMaskMap[renderTarget.RenderTargetWriteMask]);
    }
    render_pipeline_descriptor->setAlphaToCoverageEnabled(
        desc_.AlphaToCoverageEnable);
  }

private:
  const D3D11_BLEND_DESC1 desc_;
  bool dual_source_blending_;
};

// RasterizerState

class MTLD3D11RasterizerState
    : public ManagedDeviceChild<IMTLD3D11RasterizerState> {

public:
  friend class MTLD3D11DeviceContext;
  MTLD3D11RasterizerState(IMTLD3D11Device *device,
                          const D3D11_RASTERIZER_DESC2 *desc)
      : ManagedDeviceChild<IMTLD3D11RasterizerState>(device), m_desc(*desc) {}
  ~MTLD3D11RasterizerState() {};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11RasterizerState) ||
        riid == __uuidof(ID3D11RasterizerState1) ||
        riid == __uuidof(IMTLD3D11RasterizerState)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IMTLD3D11RasterizerState), riid)) {
      WARN("D3D11RasterizerState: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void STDMETHODCALLTYPE GetDesc(D3D11_RASTERIZER_DESC *pDesc) final {
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

  void STDMETHODCALLTYPE GetDesc1(D3D11_RASTERIZER_DESC1 *pDesc) final {
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
    pDesc->ForcedSampleCount = m_desc.ForcedSampleCount;
  }

  void STDMETHODCALLTYPE GetDesc2(D3D11_RASTERIZER_DESC2 *pDesc) final {
    *pDesc = m_desc;
  }

  void SetupRasterizerState(MTL::RenderCommandEncoder *encoder) {

    if (m_desc.FillMode == D3D11_FILL_SOLID) {
      encoder->setTriangleFillMode(MTL::TriangleFillMode::TriangleFillModeFill);
    } else {
      encoder->setTriangleFillMode(
          MTL::TriangleFillMode::TriangleFillModeLines);
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
    } else {
      encoder->setFrontFacingWinding(MTL::Winding::WindingClockwise);
    }

    if (m_desc.AntialiasedLineEnable) {
      // nop , we don't support this
    }

    if (m_desc.MultisampleEnable) {
      // used in combination with AntialiasedLineEnable, not supported yet.
    }

    // m_desc.ScissorEnable : handled outside

    // m_desc.ForcedSampleCount : handled outside
  }

  bool IsScissorEnabled() { return m_desc.ScissorEnable; }

  uint32_t UAVOnlySampleCount() { return std::max(1u, m_desc.ForcedSampleCount); }

private:
  const D3D11_RASTERIZER_DESC2 m_desc;
};

// DepthStencilState

class MTLD3D11DepthStencilState
    : public ManagedDeviceChild<IMTLD3D11DepthStencilState> {

public:
  friend class MTLD3D11DeviceContext;
  MTLD3D11DepthStencilState(IMTLD3D11Device *device,
                            MTL::DepthStencilState *state,
                            MTL::DepthStencilState *stencil_disabled,
                            MTL::DepthStencilState *depthsteancil_disabled,
                            const D3D11_DEPTH_STENCIL_DESC &desc)
      : ManagedDeviceChild<IMTLD3D11DepthStencilState>(device), m_desc(desc),
        state_default_(state), state_stencil_disabled_(stencil_disabled),
        state_depthstencil_disabled_(depthsteancil_disabled) {}
  ~MTLD3D11DepthStencilState() {};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11DepthStencilState) ||
        riid == __uuidof(IMTLD3D11DepthStencilState)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IMTLD3D11DepthStencilState), riid)) {
      WARN("D3D11DepthStencilState: Unknown interface query ",
           str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_DESC *pDesc) final {
    *pDesc = m_desc;
  }

  virtual MTL::DepthStencilState *GetDepthStencilState(uint32_t planar_flags) {
    switch (planar_flags) {
    case 0:
      return state_depthstencil_disabled_.ptr();
    case 1: /* depth without stencil */
      return state_stencil_disabled_.ptr();
    case 2: /* stencil without depth */
      ERR("stencil only dsv is not properly handled");
      return state_default_.ptr();
    default:
      return state_default_.ptr();
    }
  }

private:
  const D3D11_DEPTH_STENCIL_DESC m_desc;
  Obj<MTL::DepthStencilState> state_default_;
  Obj<MTL::DepthStencilState> state_stencil_disabled_;
  Obj<MTL::DepthStencilState> state_depthstencil_disabled_;
};

template <>
HRESULT StateObjectCache<D3D11_DEPTH_STENCIL_DESC, IMTLD3D11DepthStencilState>::
    CreateStateObject(const D3D11_DEPTH_STENCIL_DESC *pDesc,
                      IMTLD3D11DepthStencilState **ppDepthStencilState) {
  std::lock_guard<dxmt::mutex> lock(mutex_cache);
  InitReturnPtr(ppDepthStencilState);

  // TODO: validation
  if (!pDesc)
    return E_INVALIDARG;

  if (!ppDepthStencilState)
    return S_FALSE;

  if (cache.contains(*pDesc)) {
    cache.at(*pDesc)->QueryInterface(IID_PPV_ARGS(ppDepthStencilState));
    return S_OK;
  }

  using SD = MTL::StencilDescriptor;

  using DSD = MTL::DepthStencilDescriptor;
  Obj<MTL::DepthStencilDescriptor> dsd = transfer(DSD::alloc()->init());
  if (pDesc->DepthEnable) {
    dsd->setDepthCompareFunction(kCompareFunctionMap[pDesc->DepthFunc]);
    dsd->setDepthWriteEnabled(pDesc->DepthWriteMask ==
                              D3D11_DEPTH_WRITE_MASK_ALL);
  } else {
    dsd->setDepthCompareFunction(MTL::CompareFunctionAlways);
    dsd->setDepthWriteEnabled(false);
  }

  if (pDesc->StencilEnable) {
    {
      auto fsd = transfer(SD::alloc()->init());
      fsd->setDepthStencilPassOperation(
          kStencilOperationMap[pDesc->FrontFace.StencilPassOp]);
      fsd->setStencilFailureOperation(
          kStencilOperationMap[pDesc->FrontFace.StencilFailOp]);
      fsd->setDepthFailureOperation(
          kStencilOperationMap[pDesc->FrontFace.StencilDepthFailOp]);
      fsd->setStencilCompareFunction(
          kCompareFunctionMap[pDesc->FrontFace.StencilFunc]);
      fsd->setWriteMask(pDesc->StencilWriteMask);
      fsd->setReadMask(pDesc->StencilReadMask);
      dsd->setFrontFaceStencil(fsd.ptr());
    }
    {
      auto bsd = transfer(SD::alloc()->init());
      bsd->setDepthStencilPassOperation(
          kStencilOperationMap[pDesc->BackFace.StencilPassOp]);
      bsd->setStencilFailureOperation(
          kStencilOperationMap[pDesc->BackFace.StencilFailOp]);
      bsd->setDepthFailureOperation(
          kStencilOperationMap[pDesc->BackFace.StencilDepthFailOp]);
      bsd->setStencilCompareFunction(
          kCompareFunctionMap[pDesc->BackFace.StencilFunc]);
      bsd->setWriteMask(pDesc->StencilWriteMask);
      bsd->setReadMask(pDesc->StencilReadMask);
      dsd->setBackFaceStencil(bsd.ptr());
    }
  }

  auto state_default =
      transfer(device->GetMTLDevice()->newDepthStencilState(dsd.ptr()));

  if (pDesc->StencilEnable) {
    dsd->setFrontFaceStencil(nullptr);
    dsd->setBackFaceStencil(nullptr);
    auto stencil_effective_disabled =
        transfer(device->GetMTLDevice()->newDepthStencilState(dsd.ptr()));
    if (pDesc->DepthEnable) {
      dsd->setDepthCompareFunction(MTL::CompareFunctionAlways);
      dsd->setDepthWriteEnabled(false);
      auto depthstencil_effective_disabled =
          transfer(device->GetMTLDevice()->newDepthStencilState(dsd.ptr()));
      cache.emplace(*pDesc,
                    std::make_unique<MTLD3D11DepthStencilState>(
                        device, state_default, stencil_effective_disabled,
                        depthstencil_effective_disabled, *pDesc));
    } else {
      // stencil_effective_disabled has no depth either...
      cache.emplace(*pDesc,
                    std::make_unique<MTLD3D11DepthStencilState>(
                        device, state_default, stencil_effective_disabled,
                        stencil_effective_disabled, *pDesc));
    }
  } else {
    if (pDesc->DepthEnable) {
      dsd->setDepthCompareFunction(MTL::CompareFunctionAlways);
      dsd->setDepthWriteEnabled(false);
      auto depth_effective_disabled =
          transfer(device->GetMTLDevice()->newDepthStencilState(dsd.ptr()));
      cache.emplace(*pDesc, std::make_unique<MTLD3D11DepthStencilState>(
                                device, state_default, state_default,
                                depth_effective_disabled, *pDesc));
    } else {
      cache.emplace(*pDesc, std::make_unique<MTLD3D11DepthStencilState>(
                                device, state_default, state_default,
                                state_default, *pDesc));
    }
  }
  cache.at(*pDesc)->QueryInterface(IID_PPV_ARGS(ppDepthStencilState));
  return S_OK;
}

template <>
HRESULT StateObjectCache<D3D11_RASTERIZER_DESC2, IMTLD3D11RasterizerState>::
    CreateStateObject(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc,
                      IMTLD3D11RasterizerState **ppRasterizerState) {
  std::lock_guard<dxmt::mutex> lock(mutex_cache);
  InitReturnPtr(ppRasterizerState);

  // TODO: validate
  if (!pRasterizerDesc)
    return E_INVALIDARG;

  if (!ppRasterizerState)
    return S_FALSE;

  if (cache.contains(*pRasterizerDesc)) {
    cache.at(*pRasterizerDesc)->QueryInterface(IID_PPV_ARGS(ppRasterizerState));
    return S_OK;
  }

  cache.emplace(*pRasterizerDesc, std::make_unique<MTLD3D11RasterizerState>(
                                      device, pRasterizerDesc));
  cache.at(*pRasterizerDesc)->QueryInterface(IID_PPV_ARGS(ppRasterizerState));

  return S_OK;
}

template <>
HRESULT
StateObjectCache<D3D11_SAMPLER_DESC, IMTLD3D11SamplerState>::CreateStateObject(
    const D3D11_SAMPLER_DESC *pSamplerDesc,
    IMTLD3D11SamplerState **ppSamplerState) {
  std::lock_guard<dxmt::mutex> lock(mutex_cache);

  InitReturnPtr(ppSamplerState);

  if (pSamplerDesc == nullptr)
    return E_INVALIDARG;

  D3D11_SAMPLER_DESC desc = *pSamplerDesc;

  // FIXME: validation

  if (!ppSamplerState)
    return S_FALSE;

  if (cache.contains(*pSamplerDesc)) {
    cache.at(*pSamplerDesc)->QueryInterface(IID_PPV_ARGS(ppSamplerState));
    return S_OK;
  }

  auto mtl_sampler_desc = transfer(MTL::SamplerDescriptor::alloc()->init());

  // filter
  if (D3D11_DECODE_MIN_FILTER(desc.Filter)) { // LINEAR = 1
    mtl_sampler_desc->setMinFilter(
        MTL::SamplerMinMagFilter::SamplerMinMagFilterLinear);
  } else {
    mtl_sampler_desc->setMinFilter(
        MTL::SamplerMinMagFilter::SamplerMinMagFilterNearest);
  }
  if (D3D11_DECODE_MAG_FILTER(desc.Filter)) { // LINEAR = 1
    mtl_sampler_desc->setMagFilter(
        MTL::SamplerMinMagFilter::SamplerMinMagFilterLinear);
  } else {
    mtl_sampler_desc->setMagFilter(
        MTL::SamplerMinMagFilter::SamplerMinMagFilterNearest);
  }
  if (D3D11_DECODE_MIP_FILTER(desc.Filter)) { // LINEAR = 1
    mtl_sampler_desc->setMipFilter(
        MTL::SamplerMipFilter::SamplerMipFilterLinear);
  } else {
    mtl_sampler_desc->setMipFilter(
        MTL::SamplerMipFilter::SamplerMipFilterNearest);
  }

  // LOD
  // MipLODBias is not supported
  // FIXME: it can be done in shader, see MSL spec page 186:  bias(float value)
  mtl_sampler_desc->setLodMinClamp(
      desc.MinLOD); // -FLT_MAX vs 0?
                    // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-vssetsamplers
  mtl_sampler_desc->setLodMaxClamp(desc.MaxLOD);

  // Anisotropy
  if (D3D11_DECODE_IS_ANISOTROPIC_FILTER(desc.Filter)) {
    mtl_sampler_desc->setMaxAnisotropy(desc.MaxAnisotropy);
  }

  // address modes
  // U-S  V-T W-R
  if (desc.AddressU > 0 && desc.AddressU < 6) {
    mtl_sampler_desc->setSAddressMode(kAddressModeMap[desc.AddressU - 1]);
  }
  if (desc.AddressV > 0 && desc.AddressV < 6) {
    mtl_sampler_desc->setTAddressMode(kAddressModeMap[desc.AddressV - 1]);
  }
  if (desc.AddressW > 0 && desc.AddressW < 6) {
    mtl_sampler_desc->setRAddressMode(kAddressModeMap[desc.AddressW - 1]);
  }

  if (D3D11_DECODE_IS_COMPARISON_FILTER(desc.Filter)) {
    if (desc.ComparisonFunc < 1 || desc.ComparisonFunc > 8) {
      WARN("CreateSamplerState: invalid ComparisonFunc");
      mtl_sampler_desc->setCompareFunction(MTL::CompareFunctionNever);
    } else {
      mtl_sampler_desc->setCompareFunction(
          kCompareFunctionMap[desc.ComparisonFunc]);
    }
  }

  // border color
  if (desc.BorderColor[0] == 0.0f && desc.BorderColor[1] == 0.0f &&
      desc.BorderColor[2] == 0.0f && desc.BorderColor[3] == 0.0f) {
    mtl_sampler_desc->setBorderColor(
        MTL::SamplerBorderColor::SamplerBorderColorTransparentBlack);
  } else if (desc.BorderColor[0] == 0.0f && desc.BorderColor[1] == 0.0f &&
             desc.BorderColor[2] == 0.0f && desc.BorderColor[3] == 1.0f) {
    mtl_sampler_desc->setBorderColor(
        MTL::SamplerBorderColor::SamplerBorderColorOpaqueBlack);
  } else if (desc.BorderColor[0] == 1.0f && desc.BorderColor[1] == 1.0f &&
             desc.BorderColor[2] == 1.0f && desc.BorderColor[3] == 1.0f) {
    mtl_sampler_desc->setBorderColor(
        MTL::SamplerBorderColor::SamplerBorderColorOpaqueWhite);
  } else {
    WARN("CreateSamplerState: Unsupported border color");
  }
  mtl_sampler_desc->setSupportArgumentBuffers(true);

  auto mtl_sampler =
      transfer(device->GetMTLDevice()->newSamplerState(mtl_sampler_desc.ptr()));

  cache.emplace(*pSamplerDesc, std::make_unique<MTLD3D11SamplerState>(
                                   device, mtl_sampler.ptr(), desc));
  cache.at(*pSamplerDesc)->QueryInterface(IID_PPV_ARGS(ppSamplerState));

  return S_OK;
};

template <>
HRESULT
StateObjectCache<D3D11_BLEND_DESC1, IMTLD3D11BlendState>::CreateStateObject(
    const D3D11_BLEND_DESC1 *pBlendStateDesc,
    IMTLD3D11BlendState **ppBlendState) {
  std::lock_guard<dxmt::mutex> lock(mutex_cache);
  InitReturnPtr(ppBlendState);

  if (!pBlendStateDesc)
    return E_INVALIDARG;

  unsigned num_blend_target = pBlendStateDesc->IndependentBlendEnable ? 8 : 1;
  for (unsigned i = 0; i < num_blend_target; i++) {
    auto &blend_target = pBlendStateDesc->RenderTarget[i];
    if (blend_target.BlendEnable && blend_target.LogicOpEnable) {
      return E_INVALIDARG;
    }
  }

  if (!ppBlendState)
    return S_FALSE;

  D3D11_BLEND_DESC1 desc_normalized = *pBlendStateDesc;
  desc_normalized.AlphaToCoverageEnable =
      bool(desc_normalized.AlphaToCoverageEnable);
  desc_normalized.IndependentBlendEnable =
      bool(desc_normalized.IndependentBlendEnable);

  if (cache.contains(desc_normalized)) {
    cache.at(desc_normalized)->QueryInterface(IID_PPV_ARGS(ppBlendState));
    return S_OK;
  }

  cache.emplace(desc_normalized,
                std::make_unique<MTLD3D11BlendState>(device, desc_normalized));
  cache.at(desc_normalized)->QueryInterface(IID_PPV_ARGS(ppBlendState));

  return S_OK;
}

} // namespace dxmt
