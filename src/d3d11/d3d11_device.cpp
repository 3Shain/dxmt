#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLVertexDescriptor.hpp"
#include "com/com_guid.hpp"
#include "d3d11_pipeline.hpp"
#include "util_hash.hpp"
#include "d3d11_class_linkage.hpp"
#include "d3d11_inspection.hpp"
#include "d3d11_context.hpp"
#include "d3d11_context_state.hpp"
#include "d3d11_device.hpp"
#include "d3d11_input_layout.hpp"
#include "d3d11_private.h"
#include "d3d11_query.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_swapchain.hpp"
#include "d3d11_state_object.hpp"
#include "dxgi_interfaces.h"
#include "dxmt_format.hpp"
#include "ftl.hpp"
#include "mtld11_resource.hpp"
#include "thread.hpp"
#include "threadpool.hpp"
#include "winemacdrv.h"
#include "dxgi_object.hpp"
#include <iterator>
#include <mutex>
#include <unordered_map>

namespace std {
template <> struct hash<MTL_GRAPHICS_PIPELINE_DESC> {
  size_t operator()(const MTL_GRAPHICS_PIPELINE_DESC &v) const noexcept {
    dxmt::HashState state;
    state.add((size_t)v.VertexShader); // FIXME: don't use pointer?
    state.add((size_t)v.PixelShader);  // FIXME: don't use pointer?
    state.add((size_t)v.InputLayout);  // FIXME: don't use pointer?
    state.add(v.BlendState ? v.BlendState->GetHash(): 0);
    state.add((size_t)v.DepthStencilFormat);
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
           (x.PixelShader == y.PixelShader) &&
           (x.InputLayout == y.InputLayout) &&
           (x.DepthStencilFormat == y.DepthStencilFormat);
  }
};
} // namespace std

namespace dxmt {

class MTLD3D11Device final : public IMTLD3D11Device {
public:
  MTLD3D11Device(IMTLDXGIDevice *container, IMTLDXGIAdatper *pAdapter,
                 D3D_FEATURE_LEVEL FeatureLevel, UINT FeatureFlags)
      : m_container(container), adapter_(pAdapter),
        m_FeatureLevel(FeatureLevel), m_FeatureFlags(FeatureFlags),
        m_features(container->GetMTLDevice()) {}

  ~MTLD3D11Device() { context_ = nullptr; }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override {
    return m_container->QueryInterface(riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return m_container->AddRef(); }

  ULONG STDMETHODCALLTYPE Release() override { return m_container->Release(); }

  HRESULT STDMETHODCALLTYPE
  CreateBuffer(const D3D11_BUFFER_DESC *pDesc,
               const D3D11_SUBRESOURCE_DATA *pInitialData,
               ID3D11Buffer **ppBuffer) override {
    InitReturnPtr(ppBuffer);

    try {
      switch (pDesc->Usage) {
      case D3D11_USAGE_DEFAULT:
      case D3D11_USAGE_IMMUTABLE:
        return CreateDeviceBuffer(this, pDesc, pInitialData, ppBuffer);
      case D3D11_USAGE_DYNAMIC:
        return CreateDynamicBuffer(this, pDesc, pInitialData, ppBuffer);
      case D3D11_USAGE_STAGING:
        return CreateStagingBuffer(this, pDesc, pInitialData, ppBuffer);
      }
    } catch (const MTLD3DError &err) {
      ERR(err.message());
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc,
                  const D3D11_SUBRESOURCE_DATA *pInitialData,
                  ID3D11Texture1D **ppTexture1D) override {

    InitReturnPtr(ppTexture1D);

    if (!pDesc)
      return E_INVALIDARG;

    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED)
      return E_INVALIDARG; // not supported yet

    try {
      switch (pDesc->Usage) {
      case D3D11_USAGE_DEFAULT:
      case D3D11_USAGE_IMMUTABLE:
        return CreateDeviceTexture1D(this, pDesc, pInitialData, ppTexture1D);
      case D3D11_USAGE_DYNAMIC:
        ERR("dynamic texture 1d not supported yet");
        return E_NOTIMPL;
      case D3D11_USAGE_STAGING:
        if (pDesc->BindFlags != 0) {
          return E_INVALIDARG;
        }
        return CreateStagingTexture1D(this, pDesc, pInitialData, ppTexture1D);
      }
      return S_OK;
    } catch (const MTLD3DError &err) {
      ERR(err.message());
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
                  const D3D11_SUBRESOURCE_DATA *pInitialData,
                  ID3D11Texture2D **ppTexture2D) override {
    InitReturnPtr(ppTexture2D);

    if (!pDesc)
      return E_INVALIDARG;

    if ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED))
      return E_INVALIDARG; // not supported yet

    try {
      switch (pDesc->Usage) {
      case D3D11_USAGE_DEFAULT:
      case D3D11_USAGE_IMMUTABLE:
        return CreateDeviceTexture2D(this, pDesc, pInitialData, ppTexture2D);
      case D3D11_USAGE_DYNAMIC:
        return CreateDynamicTexture2D(this, pDesc, pInitialData, ppTexture2D);
      case D3D11_USAGE_STAGING:
        if (pDesc->BindFlags != 0) {
          return E_INVALIDARG;
        }
        return CreateStagingTexture2D(this, pDesc, pInitialData, ppTexture2D);
      }
      return S_OK;
    } catch (const MTLD3DError &err) {
      ERR(err.message());
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc,
                  const D3D11_SUBRESOURCE_DATA *pInitialData,
                  ID3D11Texture3D **ppTexture3D) override {
    InitReturnPtr(ppTexture3D);

    if (!pDesc)
      return E_INVALIDARG;

    if ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED))
      return E_INVALIDARG; // not supported yet

    try {
      switch (pDesc->Usage) {
      case D3D11_USAGE_DEFAULT:
      case D3D11_USAGE_IMMUTABLE:
        return CreateDeviceTexture3D(this, pDesc, pInitialData, ppTexture3D);
      case D3D11_USAGE_DYNAMIC:
        ERR("dynamic texture 3d not supported yet");
        return E_NOTIMPL;
      case D3D11_USAGE_STAGING:
        if (pDesc->BindFlags != 0) {
          return E_INVALIDARG;
        }
        return CreateStagingTexture3D(this, pDesc, pInitialData, ppTexture3D);
      }
      return S_OK;
    } catch (const MTLD3DError &err) {
      ERR(err.message());
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
      ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
      ID3D11ShaderResourceView **ppSRView) override {
    InitReturnPtr(ppSRView);

    if (!pResource)
      return E_INVALIDARG;

    if (!ppSRView)
      return S_FALSE;

    if (auto res = Com<IDXMTResource>::queryFrom(pResource); res != nullptr) {
      return res->CreateShaderResourceView(pDesc, ppSRView);
    }
    return E_FAIL;
  }

  HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
      ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
      ID3D11UnorderedAccessView **ppUAView) override {
    InitReturnPtr(ppUAView);

    if (!pResource)
      return E_INVALIDARG;

    if (!ppUAView)
      return S_FALSE;

    if (auto res = Com<IDXMTResource>::queryFrom(pResource); res != nullptr) {
      return res->CreateUnorderedAccessView(pDesc, ppUAView);
    }
    return E_FAIL;
  }

  HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
      ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
      ID3D11RenderTargetView **ppRTView) override {
    InitReturnPtr(ppRTView);

    if (!pResource)
      return E_INVALIDARG;

    if (!ppRTView)
      return S_FALSE;

    if (auto res = Com<IDXMTResource>::queryFrom(pResource); res != nullptr) {
      return res->CreateRenderTargetView(pDesc, ppRTView);
    }
    return E_FAIL;
  }

  HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
      ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
      ID3D11DepthStencilView **ppDepthStencilView) override {
    InitReturnPtr(ppDepthStencilView);

    if (!pResource)
      return E_INVALIDARG;

    if (!ppDepthStencilView)
      return S_FALSE;

    if (auto res = Com<IDXMTResource>::queryFrom(pResource); res != nullptr) {
      return res->CreateDepthStencilView(pDesc, ppDepthStencilView);
    }
    return E_FAIL;
  }

  HRESULT STDMETHODCALLTYPE CreateInputLayout(
      const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements,
      const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength,
      ID3D11InputLayout **ppInputLayout) override {
    InitReturnPtr(ppInputLayout);

    if (!pInputElementDescs)
      return E_INVALIDARG;
    if (!pShaderBytecodeWithInputSignature)
      return E_INVALIDARG;

    // TODO: must get shader reflection info

    if (!ppInputLayout) {
      return S_FALSE;
    }

    return dxmt::CreateInputLayout(this, pShaderBytecodeWithInputSignature,
                                   pInputElementDescs, NumElements,
                                   ppInputLayout);
  }

  HRESULT STDMETHODCALLTYPE
  CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                     ID3D11ClassLinkage *pClassLinkage,
                     ID3D11VertexShader **ppVertexShader) override {
    InitReturnPtr(ppVertexShader);
    if (pClassLinkage != nullptr)
      WARN("Class linkage not supported");

    try {
      return dxmt::CreateVertexShader(this, pShaderBytecode, BytecodeLength,
                                      ppVertexShader);
    } catch (const MTLD3DError &err) {
      ERR(err.message());
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                       ID3D11ClassLinkage *pClassLinkage,
                       ID3D11GeometryShader **ppGeometryShader) override {
    InitReturnPtr(ppGeometryShader);
    if (pClassLinkage != nullptr)
      WARN("Class linkage not supported");

    if (!ppGeometryShader)
      return S_FALSE;
    ERR("CreateGeometryShader: not supported, return a dummy");
    return dxmt::CreateDummyGeometryShader(this, pShaderBytecode,
                                           BytecodeLength, ppGeometryShader);
  }

  HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
      const void *pShaderBytecode, SIZE_T BytecodeLength,
      const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries,
      const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream,
      ID3D11ClassLinkage *pClassLinkage,
      ID3D11GeometryShader **ppGeometryShader) override {
    InitReturnPtr(ppGeometryShader);
    if (pClassLinkage != nullptr)
      WARN("Class linkage not supported");

    if (NumEntries > 0 && RasterizedStream == D3D11_SO_NO_RASTERIZED_STREAM &&
        ((char *)pShaderBytecode)[0] == 'D' &&
        ((char *)pShaderBytecode)[1] == 'X' &&
        ((char *)pShaderBytecode)[2] == 'B' &&
        ((char *)pShaderBytecode)[3] == 'C') {
      // FIXME: ensure the input shader is a vertex shader
      WARN("Emulate stream output");

      return dxmt::CreateEmulatedVertexStreamOutputShader(
          this, pShaderBytecode, BytecodeLength, ppGeometryShader, NumEntries,
          pSODeclaration, NumStrides, pBufferStrides);
    }
    ERR("CreateGeometryShaderWithStreamOutput: not supported, expect problem");
    return dxmt::CreateDummyGeometryShader(this, pShaderBytecode,
                                           BytecodeLength, ppGeometryShader);
  }

  HRESULT STDMETHODCALLTYPE
  CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                    ID3D11ClassLinkage *pClassLinkage,
                    ID3D11PixelShader **ppPixelShader) override {
    InitReturnPtr(ppPixelShader);
    if (pClassLinkage != nullptr)
      WARN("Class linkage not supported");

    try {
      return dxmt::CreatePixelShader(this, pShaderBytecode, BytecodeLength,
                                     ppPixelShader);
    } catch (const MTLD3DError &err) {
      ERR(err.message());
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  CreateHullShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                   ID3D11ClassLinkage *pClassLinkage,
                   ID3D11HullShader **ppHullShader) override {
    InitReturnPtr(ppHullShader);
    if (pClassLinkage != nullptr)
      WARN("Class linkage not supported");

    if (!ppHullShader)
      return S_FALSE;
    ERR("CreateHullShader: not supported, return a dummy");
    return dxmt::CreateDummyHullShader(this, pShaderBytecode, BytecodeLength,
                                       ppHullShader);
  }

  HRESULT STDMETHODCALLTYPE
  CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                     ID3D11ClassLinkage *pClassLinkage,
                     ID3D11DomainShader **ppDomainShader) override {
    InitReturnPtr(ppDomainShader);
    if (pClassLinkage != nullptr)
      WARN("Class linkage not supported");

    if (!ppDomainShader)
      return S_FALSE;
    ERR("CreateDomainShader: not supported, return a dummy");
    return dxmt::CreateDummyDomainShader(this, pShaderBytecode, BytecodeLength,
                                         ppDomainShader);
  }

  HRESULT STDMETHODCALLTYPE
  CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                      ID3D11ClassLinkage *pClassLinkage,
                      ID3D11ComputeShader **ppComputeShader) override {
    InitReturnPtr(ppComputeShader);
    if (pClassLinkage != nullptr)
      WARN("Class linkage not supported");

    try {
      return dxmt::CreateComputeShader(this, pShaderBytecode, BytecodeLength,
                                       ppComputeShader);
    } catch (const MTLD3DError &err) {
      ERR(err.message());
      return E_FAIL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  CreateClassLinkage(ID3D11ClassLinkage **ppLinkage) override {
    *ppLinkage = ref(new MTLD3D11ClassLinkage(this));
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc,
                   ID3D11BlendState **ppBlendState) override {
    ID3D11BlendState1 *pBlendState1;
    D3D11_BLEND_DESC1 desc;
    desc.AlphaToCoverageEnable = pBlendStateDesc->AlphaToCoverageEnable;
    desc.IndependentBlendEnable = pBlendStateDesc->IndependentBlendEnable;

    for (uint32_t i = 0; i < 8; i++) {
      desc.RenderTarget[i].BlendEnable =
          pBlendStateDesc->RenderTarget[i].BlendEnable;
      desc.RenderTarget[i].LogicOpEnable = FALSE;
      desc.RenderTarget[i].LogicOp = D3D11_LOGIC_OP_NOOP;
      desc.RenderTarget[i].SrcBlend = pBlendStateDesc->RenderTarget[i].SrcBlend;
      desc.RenderTarget[i].DestBlend =
          pBlendStateDesc->RenderTarget[i].DestBlend;
      desc.RenderTarget[i].BlendOp = pBlendStateDesc->RenderTarget[i].BlendOp;
      desc.RenderTarget[i].SrcBlendAlpha =
          pBlendStateDesc->RenderTarget[i].SrcBlendAlpha;
      desc.RenderTarget[i].DestBlendAlpha =
          pBlendStateDesc->RenderTarget[i].DestBlendAlpha;
      desc.RenderTarget[i].BlendOpAlpha =
          pBlendStateDesc->RenderTarget[i].BlendOpAlpha;
      desc.RenderTarget[i].RenderTargetWriteMask =
          pBlendStateDesc->RenderTarget[i].RenderTargetWriteMask;
    }
    auto hr = CreateBlendState1(&desc, &pBlendState1);
    *ppBlendState = pBlendState1;
    return hr;
  }

  HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
      const D3D11_DEPTH_STENCIL_DESC *pDesc,
      ID3D11DepthStencilState **ppDepthStencilState) override {
    return dxmt::CreateDepthStencilState(this, pDesc, ppDepthStencilState);
  }

  HRESULT STDMETHODCALLTYPE
  CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc,
                        ID3D11RasterizerState **ppRasterizerState) override {
    ID3D11RasterizerState1 *pRasterizerState1;
    D3D11_RASTERIZER_DESC1 desc;
    desc.FillMode = pRasterizerDesc->FillMode;
    desc.CullMode = pRasterizerDesc->CullMode;
    desc.FrontCounterClockwise = pRasterizerDesc->FrontCounterClockwise;
    desc.DepthBias = pRasterizerDesc->DepthBias;
    desc.DepthBiasClamp = pRasterizerDesc->DepthBiasClamp;
    desc.SlopeScaledDepthBias = pRasterizerDesc->SlopeScaledDepthBias;
    desc.DepthClipEnable = pRasterizerDesc->DepthClipEnable;
    desc.ScissorEnable = pRasterizerDesc->ScissorEnable;
    desc.MultisampleEnable = pRasterizerDesc->MultisampleEnable;
    desc.AntialiasedLineEnable = pRasterizerDesc->AntialiasedLineEnable;
    desc.ForcedSampleCount = 0;
    auto hr = CreateRasterizerState1(&desc, &pRasterizerState1);
    *ppRasterizerState = pRasterizerState1;
    return hr;
  }

  HRESULT STDMETHODCALLTYPE
  CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc,
                     ID3D11SamplerState **ppSamplerState) override {
    return dxmt::CreateSamplerState(this, pSamplerDesc, ppSamplerState);
  }

  HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC *pQueryDesc,
                                        ID3D11Query **ppQuery) override {
    InitReturnPtr(ppQuery);

    if (!pQueryDesc)
      return E_INVALIDARG;

    switch (pQueryDesc->Query) {
    case D3D11_QUERY_EVENT:
      return CreateEventQuery(this, pQueryDesc, ppQuery);
      return S_OK;
    case D3D11_QUERY_OCCLUSION:
      return CreateOcculusionQuery(this, pQueryDesc, ppQuery);
    case D3D11_QUERY_TIMESTAMP: {
      *ppQuery = ref(new MTLD3D11DummyQuery<UINT64>(this, pQueryDesc));
      return S_OK;
    }
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      *ppQuery = ref(new MTLD3D11DummyQuery<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT>(this, pQueryDesc));
      return S_OK;
    }
    default:
      // ERR("CreateQuery: query type not implemented: ", pQueryDesc->Query);
      return E_NOTIMPL;
    }
  }

  HRESULT STDMETHODCALLTYPE
  CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc,
                  ID3D11Predicate **ppPredicate) override {
    return CreateQuery(pPredicateDesc,
                       reinterpret_cast<ID3D11Query **>(ppPredicate));
  }

  HRESULT STDMETHODCALLTYPE
  CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc,
                ID3D11Counter **ppCounter) override {
    InitReturnPtr(ppCounter);
    WARN("Not supported");
    return DXGI_ERROR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE CreateDeferredContext(
      UINT ContextFlags,
      ID3D11DeviceContext **ppDeferredContext) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface,
                         void **ppResource) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport) override {

    if (pFormatSupport) {
      *pFormatSupport = 0;
    }

    MTL_FORMAT_DESC metal_format;

    if (FAILED(adapter_->QueryFormatDesc(Format, &metal_format))) {
      return S_OK;
    }

    UINT outFormatSupport = 0;

    // All graphics and compute kernels can read or sample a texture with any
    // pixel format.
    outFormatSupport |= D3D11_FORMAT_SUPPORT_SHADER_LOAD |
                        D3D11_FORMAT_SUPPORT_SHADER_SAMPLE |
                        D3D11_FORMAT_SUPPORT_SHADER_GATHER |
                        D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD |
                        D3D11_FORMAT_SUPPORT_CPU_LOCKABLE;

    /* UNCHECKED */
    outFormatSupport |=
        D3D11_FORMAT_SUPPORT_BUFFER | D3D11_FORMAT_SUPPORT_TEXTURE1D |
        D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_TEXTURE3D |
        D3D11_FORMAT_SUPPORT_TEXTURECUBE | D3D11_FORMAT_SUPPORT_MIP |
        D3D11_FORMAT_SUPPORT_MIP_AUTOGEN | // ?
        D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT;

    if (metal_format.SupportBackBuffer) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_DISPLAY;
    }

    if (metal_format.VertexFormat != MTL::VertexFormatInvalid) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER;
    }

    if (metal_format.PixelFormat == MTL::PixelFormatR32Uint ||
        metal_format.PixelFormat == MTL::PixelFormatR16Uint) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER;
    }

    if (any_bit_set(metal_format.Capability & FormatCapability::Color)) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_RENDER_TARGET;
    }

    if (any_bit_set(metal_format.Capability & FormatCapability::Blend)) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_BLENDABLE;
    }

    if (any_bit_set(metal_format.Capability & FormatCapability::DepthStencil)) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_DEPTH_STENCIL |
                          D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON |
                          D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON;
    }

    if (any_bit_set(metal_format.Capability & FormatCapability::Resolve)) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE;
    }

    if (any_bit_set(metal_format.Capability & FormatCapability::MSAA)) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET;
    }

    if (any_bit_set(metal_format.Capability & FormatCapability::Write)) {
      outFormatSupport |= D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW;
    }

    if (pFormatSupport) {
      *pFormatSupport = outFormatSupport;
    }

    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
      DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels) override {
    return CheckMultisampleQualityLevels1(Format, SampleCount, 0,
                                          pNumQualityLevels);
  }

  void STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo)
      override { // We basically don't support counters
    pCounterInfo->LastDeviceDependentCounter = D3D11_COUNTER(0);
    pCounterInfo->NumSimultaneousCounters = 0;
    pCounterInfo->NumDetectableParallelUnits = 0;
  }

  HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC *pDesc,
                                         D3D11_COUNTER_TYPE *pType,
                                         UINT *pActiveCounters, LPSTR szName,
                                         UINT *pNameLength, LPSTR szUnits,
                                         UINT *pUnitsLength,
                                         LPSTR szDescription,
                                         UINT *pDescriptionLength) override {
    WARN("Not supported");
    return DXGI_ERROR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE
  CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData,
                      UINT FeatureSupportDataSize) override {
    switch (Feature) {
    // Format support queries are special in that they use in-out
    // structs
    case D3D11_FEATURE_FORMAT_SUPPORT: {
      auto info =
          static_cast<D3D11_FEATURE_DATA_FORMAT_SUPPORT *>(pFeatureSupportData);

      if (FeatureSupportDataSize != sizeof(*info))
        return E_INVALIDARG;

      return CheckFormatSupport(info->InFormat, &info->OutFormatSupport);
    }
    case D3D11_FEATURE_FORMAT_SUPPORT2: {
      auto info = static_cast<D3D11_FEATURE_DATA_FORMAT_SUPPORT2 *>(
          pFeatureSupportData);

      if (FeatureSupportDataSize != sizeof(*info))
        return E_INVALIDARG;
      info->OutFormatSupport2 = 0;

      MTL_FORMAT_DESC desc;
      if (FAILED(adapter_->QueryFormatDesc(info->InFormat, &desc))) {
        return S_OK;
      }

      if (any_bit_set(desc.Capability & FormatCapability::Atomic)) {
        info->OutFormatSupport2 |=
            D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD |
            D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS |
            D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE |
            D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE |
            D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX |
            D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX;

#ifndef DXMT_NO_PRIVATE_API
        /* UNCHECKED */
        info->OutFormatSupport2 |= D3D11_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP;
#endif
      }

      if (any_bit_set(desc.Capability & FormatCapability::Sparse)) {
        info->OutFormatSupport2 |= D3D11_FORMAT_SUPPORT2_TILED;
      }

      return S_OK;
    }
    default:
      // For everything else, we can use the device feature struct
      // that we already initialized during device creation.
      return m_features.GetFeatureData(Feature, FeatureSupportDataSize,
                                       pFeatureSupportData);
    }
  }

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize,
                                           void *pData) override {
    return m_container->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize,
                                           const void *pData) override {
    return m_container->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override {
    return m_container->SetPrivateDataInterface(guid, pData);
  }

  D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() override {
    return m_FeatureLevel;
  }

  UINT STDMETHODCALLTYPE GetCreationFlags() override { return m_FeatureFlags; }

  HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override {
    // unless we are deal with eGPU, this method should awalys return S_OK?
    return S_OK;
  }

  void STDMETHODCALLTYPE
  GetImmediateContext(ID3D11DeviceContext **ppImmediateContext) override {
    GetImmediateContext2((ID3D11DeviceContext2 **)ppImmediateContext);
  }

  HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) override {
    ERR("Not implemented");
    return E_NOTIMPL;
  }

  UINT STDMETHODCALLTYPE GetExceptionMode() override {
    ERR("Not implemented");
    return 0;
  }

  void STDMETHODCALLTYPE
  GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext) override {
    GetImmediateContext2((ID3D11DeviceContext2 **)ppImmediateContext);
  }

  HRESULT STDMETHODCALLTYPE CreateDeferredContext1(
      UINT ContextFlags,
      ID3D11DeviceContext1 **ppDeferredContext) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc,
                        ID3D11BlendState1 **ppBlendState) override {
    return dxmt::CreateBlendState(this, pBlendStateDesc, ppBlendState);
  }

  HRESULT STDMETHODCALLTYPE
  CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
                         ID3D11RasterizerState1 **ppRasterizerState) override {
    return dxmt::CreateRasterizerState(this, pRasterizerDesc,
                                       ppRasterizerState);
  }

  HRESULT STDMETHODCALLTYPE CreateDeviceContextState(
      UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, REFIID EmulatedInterface,
      D3D_FEATURE_LEVEL *pChosenFeatureLevel,
      ID3DDeviceContextState **ppContextState) override {
    InitReturnPtr(ppContextState);

    // TODO: validation

    if (ppContextState == nullptr) {
      return S_FALSE;
    }
    *ppContextState = ref(new MTLD3D11DeviceContextState(this));
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OpenSharedResource1(HANDLE hResource,
                                                REFIID returnedInterface,
                                                void **ppResource) override{
      IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess,
                               REFIID returnedInterface,
                               void **ppResource) override {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  GetImmediateContext2(ID3D11DeviceContext2 **ppImmediateContext) override {
    if (!context_) {
      /*
      lazy construction by design
      to solve an awkward com_cast on refcount=0 object
      (generally shouldn't pass this to others in constructor)
      */
      context_ = CreateD3D11DeviceContext(this);
    }
    *ppImmediateContext = context_.ref();
  }

  HRESULT STDMETHODCALLTYPE
  CreateDeferredContext2(UINT flags, ID3D11DeviceContext2 **context) override {
    IMPLEMENT_ME;
  }

  void STDMETHODCALLTYPE GetResourceTiling(
      ID3D11Resource *resource, UINT *tile_count,
      D3D11_PACKED_MIP_DESC *mip_desc, D3D11_TILE_SHAPE *tile_shape,
      UINT *subresource_tiling_count, UINT first_subresource_tiling,
      D3D11_SUBRESOURCE_TILING *subresource_tiling) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CheckMultisampleQualityLevels1(DXGI_FORMAT Format, UINT SampleCount,
                                     UINT Flags,
                                     UINT *pNumQualityLevels) override {
    if (Flags) {
      IMPLEMENT_ME;
    }
    MTL_FORMAT_DESC desc;
    adapter_->QueryFormatDesc(Format, &desc);
    if (desc.PixelFormat == MTL::PixelFormatInvalid) {
      *pNumQualityLevels = 0;
      return E_INVALIDARG;
    }

    // MSDN:
    // FEATURE_LEVEL_11_0 devices are required to support 4x MSAA for all render
    // target formats, and 8x MSAA for all render target formats except
    // R32G32B32A32 formats.

    // seems some pixel format doesn't support MSAA
    // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

    if (GetMTLDevice()->supportsTextureSampleCount(SampleCount)) {
      *pNumQualityLevels = 1; // always 1: in metal there is no concept of
                              // Quality Level (so is it in vulkan iirc)
      return S_OK;
    }
    return S_FALSE;
  }

  HRESULT STDMETHODCALLTYPE
  CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *desc,
                   const D3D11_SUBRESOURCE_DATA *initial_data,
                   ID3D11Texture2D1 **texture) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *desc,
                       const D3D11_SUBRESOURCE_DATA *initial_data,
                       ID3D11Texture3D1 **texture) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *desc,
                             ID3D11RasterizerState2 **state) override{
          IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CreateShaderResourceView1(ID3D11Resource *resource,
                                const D3D11_SHADER_RESOURCE_VIEW_DESC1 *desc,
                                ID3D11ShaderResourceView1 **view) override{
          IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CreateUnorderedAccessView1(ID3D11Resource *resource,
                                 const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *desc,
                                 ID3D11UnorderedAccessView1 **view) override{
          IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CreateRenderTargetView1(ID3D11Resource *resource,
                              const D3D11_RENDER_TARGET_VIEW_DESC1 *desc,
                              ID3D11RenderTargetView1 **view) override{
          IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE CreateQuery1(const D3D11_QUERY_DESC1 *desc,
                                         ID3D11Query1 **query) override {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  GetImmediateContext3(ID3D11DeviceContext3 **context) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      CreateDeferredContext3(UINT flags,
                             ID3D11DeviceContext3 **context) override {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE WriteToSubresource(ID3D11Resource *dst_resource,
                                            UINT dst_subresource,
                                            const D3D11_BOX *dst_box,
                                            const void *src_data,
                                            UINT src_row_pitch,
                                            UINT src_depth_pitch) override {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  ReadFromSubresource(void *dst_data, UINT dst_row_pitch, UINT dst_depth_pitch,
                      ID3D11Resource *src_resource, UINT src_subresource,
                      const D3D11_BOX *src_box) override{IMPLEMENT_ME}

  MTL::Device *STDMETHODCALLTYPE GetMTLDevice() override {
    return m_container->GetMTLDevice();
  }

  void GetAdapter(IMTLDXGIAdatper **ppAdapter) override {
    *ppAdapter = ref(adapter_);
  }

  void SubmitThreadgroupWork(IMTLThreadpoolWork *pWork,
                             THREADGROUP_WORK_STATE *pState) override {
    pWork->AddRef();
    pool_.enqueue(pWork, pState);
  }

  void WaitThreadgroupWork(THREADGROUP_WORK_STATE *pState) override {
    pool_.wait(pState);
  }

  HRESULT
  CreateGraphicsPipeline(MTL_GRAPHICS_PIPELINE_DESC *pDesc,
                         IMTLCompiledGraphicsPipeline **ppPipeline) override {
    std::lock_guard<dxmt::mutex> lock(mutex_);

    auto iter = pipelines_.find(*pDesc);
    if (iter != pipelines_.end()) {
      *ppPipeline = iter->second.ref();
      return S_OK;
    }
    auto temp = dxmt::CreateGraphicsPipeline(
        this, pDesc->VertexShader, pDesc->PixelShader, pDesc->InputLayout,
        pDesc->BlendState, pDesc->NumColorAttachments,
        pDesc->ColorAttachmentFormats, pDesc->DepthStencilFormat);
    D3D11_ASSERT(pipelines_.insert({*pDesc, temp}).second); // copy
    *ppPipeline = std::move(temp);                          // move
    return S_OK;
  };

  HRESULT
  CreateComputePipeline(IMTLCompiledShader *pShader,
                        IMTLCompiledComputePipeline **ppPipeline) override {
    std::lock_guard<dxmt::mutex> lock(mutex_cs_);

    auto iter = pipelines_cs_.find(pShader);
    if (iter != pipelines_cs_.end()) {
      *ppPipeline = iter->second.ref();
      return S_OK;
    }
    auto temp = dxmt::CreateComputePipeline(this, pShader);
    D3D11_ASSERT(pipelines_cs_.insert({pShader, temp}).second); // copy
    *ppPipeline = std::move(temp);                              // move
    return S_OK;
  };

private:
  IMTLDXGIDevice *m_container;
  IMTLDXGIAdatper *adapter_;
  D3D_FEATURE_LEVEL m_FeatureLevel;
  UINT m_FeatureFlags;
  MTLD3D11Inspection m_features;
  Com<IMTLD3D11DeviceContext> context_;
  threadpool<threadpool_trait> pool_;

  std::unordered_map<MTL_GRAPHICS_PIPELINE_DESC,
                     Com<IMTLCompiledGraphicsPipeline>>
      pipelines_;
  dxmt::mutex mutex_;

  std::unordered_map<IMTLCompiledShader *, Com<IMTLCompiledComputePipeline>>
      pipelines_cs_;
  dxmt::mutex mutex_cs_;
};

/**
 * \brief D3D11 device container
 *
 * Stores all the objects that contribute to the D3D11
 * device implementation, including the DXGI device.
 */
class MTLD3D11DXGIDevice final : public MTLDXGIObject<IMTLDXGIDevice> {
public:
  friend class MTLDXGIMetalLayerFactory;

  MTLD3D11DXGIDevice(IMTLDXGIAdatper *adapter, D3D_FEATURE_LEVEL feature_level,
                     UINT feature_flags)
      : adapter_(adapter),
        d3d11_device_(this, adapter, feature_level, feature_flags) {}

  ~MTLD3D11DXGIDevice() override {}
  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDevice) || riid == __uuidof(IDXGIDevice1) ||
        riid == __uuidof(IDXGIDevice2) || riid == __uuidof(IDXGIDevice3) ||
        riid == __uuidof(IMTLDXGIDevice)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3D11Device) || riid == __uuidof(ID3D11Device1) ||
        riid == __uuidof(ID3D11Device2) || riid == __uuidof(ID3D11Device3) ||
        riid == __uuidof(IMTLD3D11Device)) {
      *ppvObject = ref(&d3d11_device_);
      return S_OK;
    }

    if (riid == __uuidof(ID3D11Debug))
      return E_NOINTERFACE;

    if (logQueryInterfaceError(__uuidof(IMTLDXGIDevice), riid)) {
      WARN("D3D11Device: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override {
    return adapter_->QueryInterface(riid, ppParent);
  }

  HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter **pAdapter) override {
    if (pAdapter == nullptr)
      return DXGI_ERROR_INVALID_CALL;

    *pAdapter = adapter_.ref();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  CreateSurface(const DXGI_SURFACE_DESC *desc, UINT surface_count,
                DXGI_USAGE usage, const DXGI_SHARED_RESOURCE *shared_resource,
                IDXGISurface **surface) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      QueryResourceResidency(IUnknown *const *resources,
                             DXGI_RESIDENCY *residency,
                             UINT resource_count) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority) override {
    if (Priority < -7 || Priority > 7)
      return E_INVALIDARG;

    WARN("SetGPUThreadPriority: Ignoring");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *pPriority) override {
    *pPriority = 0;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override{
      IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      GetMaximumFrameLatency(UINT *pMaxLatency) override{IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      OfferResources(UINT NumResources, IDXGIResource *const *ppResources,
                     DXGI_OFFER_RESOURCE_PRIORITY Priority) override {
    // stub
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE ReclaimResources(UINT NumResources,
                                             IDXGIResource *const *ppResources,
                                             WINBOOL *pDiscarded) override {
    // stub
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE EnqueueSetEvent(HANDLE hEvent) override {
    return E_FAIL;
  }

  void STDMETHODCALLTYPE Trim() override { WARN("DXGIDevice3::Trim: no-op"); };

  MTL::Device *STDMETHODCALLTYPE GetMTLDevice() override {
    return adapter_->GetMTLDevice();
  }

  HRESULT STDMETHODCALLTYPE CreateSwapChain(
      IDXGIFactory1 *pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
      IDXGISwapChain1 **ppSwapChain) override {

    return dxmt::CreateSwapChain(pFactory, this, hWnd, pDesc, pFullscreenDesc,
                                 ppSwapChain);
  }

  HRESULT STDMETHODCALLTYPE GetMetalLayerFromHwnd(
      HWND hWnd, CA::MetalLayer **ppMetalLayer, void **ppNativeView) override {
    if (ppMetalLayer == nullptr || ppNativeView == nullptr)
      return E_POINTER;

    *ppMetalLayer = nullptr;
    *ppNativeView = nullptr;

    auto data = get_win_data(hWnd);

    auto metalView = macdrv_view_create_metal_view(
        data->client_cocoa_view, (macdrv_metal_device)GetMTLDevice());

    if (metalView == nullptr)
      return E_FAIL;

    *ppNativeView = metalView;

    auto layer = (CA::MetalLayer *)macdrv_view_get_metal_layer(metalView);

    TRACE("CAMetaLayer got with ref count ", layer->retainCount());

    if (layer == nullptr)
      return E_FAIL;

    *ppMetalLayer = layer;

    release_win_data(data);

    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE ReleaseMetalLayer(HWND hWnd,
                                              void *pNativeView) override {
    macdrv_view_release_metal_view((macdrv_metal_view)pNativeView);

    return S_OK;
  }

private:
  Com<IMTLDXGIAdatper> adapter_;
  MTLD3D11Device d3d11_device_;
};

Com<IMTLDXGIDevice> CreateD3D11Device(IMTLDXGIAdatper *adapter,
                                      D3D_FEATURE_LEVEL feature_level,
                                      UINT feature_flags) {
  return new MTLD3D11DXGIDevice(adapter, feature_level, feature_flags);
};
} // namespace dxmt