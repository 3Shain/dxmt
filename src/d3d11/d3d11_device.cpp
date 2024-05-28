#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLVertexDescriptor.hpp"
#include "com/com_guid.hpp"
#include "com/com_aggregatable.hpp"
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
    state.add(v.BlendState->GetHash());
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

class MTLD3D11DXGIDevice;
class MTLDXGIMetalLayerFactory
    : public ComAggregatedObject<MTLD3D11DXGIDevice, IDXGIMetalLayerFactory> {
public:
  friend class MTLD3D11DeviceContext;
  MTLDXGIMetalLayerFactory(MTLD3D11DXGIDevice *container)
      : ComAggregatedObject(container) {};
  HRESULT GetMetalLayerFromHwnd(HWND hWnd, CA::MetalLayer **ppMetalLayer,
                                void **ppNativeView) final;
  HRESULT ReleaseMetalLayer(HWND hWnd, void *pNativeView) final;
};

class MTLD3D11DeviceContext;

class MTLD3D11Device : public IMTLD3D11Device {
public:
  friend class MTLD3D11SwapChain;
  MTLD3D11Device(MTLD3D11DXGIDevice *container, D3D_FEATURE_LEVEL FeatureLevel,
                 UINT FeatureFlags);
  ~MTLD3D11Device();
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
  ULONG STDMETHODCALLTYPE AddRef() final;
  ULONG STDMETHODCALLTYPE Release() final;

  HRESULT STDMETHODCALLTYPE
  CreateBuffer(const D3D11_BUFFER_DESC *pDesc,
               const D3D11_SUBRESOURCE_DATA *pInitialData,
               ID3D11Buffer **ppBuffer) final;

  HRESULT STDMETHODCALLTYPE
  CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc,
                  const D3D11_SUBRESOURCE_DATA *pInitialData,
                  ID3D11Texture1D **ppTexture1D) final;

  HRESULT STDMETHODCALLTYPE
  CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
                  const D3D11_SUBRESOURCE_DATA *pInitialData,
                  ID3D11Texture2D **ppTexture2D) final;

  HRESULT STDMETHODCALLTYPE
  CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc,
                  const D3D11_SUBRESOURCE_DATA *pInitialData,
                  ID3D11Texture3D **ppTexture3D) final;

  HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
      ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
      ID3D11ShaderResourceView **ppSRView) final;

  HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
      ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
      ID3D11UnorderedAccessView **ppUAView) final;

  HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
      ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
      ID3D11RenderTargetView **ppRTView) final;

  HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
      ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
      ID3D11DepthStencilView **ppDepthStencilView) final;

  HRESULT STDMETHODCALLTYPE CreateInputLayout(
      const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements,
      const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength,
      ID3D11InputLayout **ppInputLayout) final;

  HRESULT STDMETHODCALLTYPE
  CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                     ID3D11ClassLinkage *pClassLinkage,
                     ID3D11VertexShader **ppVertexShader) final;

  HRESULT STDMETHODCALLTYPE
  CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                       ID3D11ClassLinkage *pClassLinkage,
                       ID3D11GeometryShader **ppGeometryShader) final;

  HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
      const void *pShaderBytecode, SIZE_T BytecodeLength,
      const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries,
      const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream,
      ID3D11ClassLinkage *pClassLinkage,
      ID3D11GeometryShader **ppGeometryShader) final;

  HRESULT STDMETHODCALLTYPE
  CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                    ID3D11ClassLinkage *pClassLinkage,
                    ID3D11PixelShader **ppPixelShader) final;

  HRESULT STDMETHODCALLTYPE CreateHullShader(
      const void *pShaderBytecode, SIZE_T BytecodeLength,
      ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader) final;

  HRESULT STDMETHODCALLTYPE
  CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                     ID3D11ClassLinkage *pClassLinkage,
                     ID3D11DomainShader **ppDomainShader) final;

  HRESULT STDMETHODCALLTYPE
  CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                      ID3D11ClassLinkage *pClassLinkage,
                      ID3D11ComputeShader **ppComputeShader) final;

  HRESULT STDMETHODCALLTYPE
  CreateClassLinkage(ID3D11ClassLinkage **ppLinkage) final;

  HRESULT STDMETHODCALLTYPE
  CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc,
                   ID3D11BlendState **ppBlendState) final;

  HRESULT STDMETHODCALLTYPE
  CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
                          ID3D11DepthStencilState **ppDepthStencilState) final;

  HRESULT STDMETHODCALLTYPE
  CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc,
                        ID3D11RasterizerState **ppRasterizerState) final;

  HRESULT STDMETHODCALLTYPE
  CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc,
                     ID3D11SamplerState **ppSamplerState) final;

  HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC *pQueryDesc,
                                        ID3D11Query **ppQuery) final;

  HRESULT STDMETHODCALLTYPE
  CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc,
                  ID3D11Predicate **ppPredicate) final;

  HRESULT STDMETHODCALLTYPE CreateCounter(
      const D3D11_COUNTER_DESC *pCounterDesc, ID3D11Counter **ppCounter) final;

  HRESULT STDMETHODCALLTYPE CreateDeferredContext(
      UINT ContextFlags, ID3D11DeviceContext **ppDeferredContext) final;

  HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource,
                                               REFIID ReturnedInterface,
                                               void **ppResource) final;

  HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format,
                                               UINT *pFormatSupport) final;

  HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
      DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels) final;

  void STDMETHODCALLTYPE
  CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo) final;

  HRESULT STDMETHODCALLTYPE CheckCounter(
      const D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType,
      UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits,
      UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength) final;

  HRESULT STDMETHODCALLTYPE
  CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData,
                      UINT FeatureSupportDataSize) final;

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize,
                                           void *pData) final;

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize,
                                           const void *pData) final;

  HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) final;

  D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() final;

  UINT STDMETHODCALLTYPE GetCreationFlags() final;

  HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() final;

  void STDMETHODCALLTYPE
  GetImmediateContext(ID3D11DeviceContext **ppImmediateContext) final;

  HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) final;

  UINT STDMETHODCALLTYPE GetExceptionMode() final;
  void STDMETHODCALLTYPE
  GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext) final;

  HRESULT STDMETHODCALLTYPE CreateDeferredContext1(
      UINT ContextFlags, ID3D11DeviceContext1 **ppDeferredContext) final;

  HRESULT STDMETHODCALLTYPE
  CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc,
                    ID3D11BlendState1 **ppBlendState) final;

  HRESULT STDMETHODCALLTYPE
  CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
                         ID3D11RasterizerState1 **ppRasterizerState) final;

  HRESULT STDMETHODCALLTYPE CreateDeviceContextState(
      UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, REFIID EmulatedInterface,
      D3D_FEATURE_LEVEL *pChosenFeatureLevel,
      ID3DDeviceContextState **ppContextState) final;

  HRESULT STDMETHODCALLTYPE OpenSharedResource1(HANDLE hResource,
                                                REFIID returnedInterface,
                                                void **ppResource) final;

  HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(LPCWSTR lpName,
                                                     DWORD dwDesiredAccess,
                                                     REFIID returnedInterface,
                                                     void **ppResource) final;

  MTL::Device *STDMETHODCALLTYPE GetMTLDevice();

  void GetAdapter(IMTLDXGIAdatper **ppAdapter);

  void SubmitThreadgroupWork(IMTLThreadpoolWork *pWork,
                             THREADGROUP_WORK_STATE *pState) final;
  void WaitThreadgroupWork(THREADGROUP_WORK_STATE *pState) final;

  HRESULT CreateGraphicsPipeline(MTL_GRAPHICS_PIPELINE_DESC *pDesc,
                                 IMTLCompiledGraphicsPipeline **ppPipeline) {
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
    assert(pipelines_.insert({*pDesc, temp}).second); // copy
    *ppPipeline = std::move(temp);                    // move
    return S_OK;
  };

  HRESULT CreateComputePipeline(IMTLCompiledShader *pShader,
                                IMTLCompiledComputePipeline **ppPipeline) {
    std::lock_guard<dxmt::mutex> lock(mutex_cs_);

    auto iter = pipelines_cs_.find(pShader);
    if (iter != pipelines_cs_.end()) {
      *ppPipeline = iter->second.ref();
      return S_OK;
    }
    auto temp = dxmt::CreateComputePipeline(this, pShader);
    assert(pipelines_cs_.insert({pShader, temp}).second); // copy
    *ppPipeline = std::move(temp);                        // move
    return S_OK;
  };

private:
  MTLD3D11DXGIDevice *m_container;
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
class MTLD3D11DXGIDevice : public MTLDXGIObject<IMTLDXGIDevice> {
public:
  friend class MTLD3D11Device;
  friend class MTLDXGIMetalLayerFactory;
  MTLD3D11DXGIDevice(IMTLDXGIAdatper *pAdapter, D3D_FEATURE_LEVEL FeatureLevel,
                     UINT FeatureFlags);
  ~MTLD3D11DXGIDevice();
  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) final;

  HRESULT
  STDMETHODCALLTYPE
  GetParent(REFIID riid, void **parent) final;

  HRESULT
  STDMETHODCALLTYPE
  GetAdapter(IDXGIAdapter **adapter) final;

  HRESULT
  STDMETHODCALLTYPE
  CreateSurface(const DXGI_SURFACE_DESC *desc, UINT surface_count,
                DXGI_USAGE usage, const DXGI_SHARED_RESOURCE *shared_resource,
                IDXGISurface **surface) final;

  HRESULT
  STDMETHODCALLTYPE
  QueryResourceResidency(IUnknown *const *resources, DXGI_RESIDENCY *residency,
                         UINT resource_count) final;

  HRESULT
  STDMETHODCALLTYPE
  SetGPUThreadPriority(INT priority) final;

  HRESULT
  STDMETHODCALLTYPE
  GetGPUThreadPriority(INT *priority) final;

  HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) final;

  HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) final;

  HRESULT STDMETHODCALLTYPE
  OfferResources(UINT NumResources, IDXGIResource *const *ppResources,
                 DXGI_OFFER_RESOURCE_PRIORITY Priority) final;

  HRESULT STDMETHODCALLTYPE ReclaimResources(UINT NumResources,
                                             IDXGIResource *const *ppResources,
                                             WINBOOL *pDiscarded) final;

  HRESULT STDMETHODCALLTYPE EnqueueSetEvent(HANDLE hEvent) final;

  MTL::Device *STDMETHODCALLTYPE GetMTLDevice() final;

  HRESULT STDMETHODCALLTYPE CreateSwapChain(
      IDXGIFactory1 *pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
      IDXGISwapChain1 **ppSwapChain) final;

private:
  Com<IMTLDXGIAdatper> adapter_;
  MTLDXGIMetalLayerFactory metal_layer_factory;
  MTLD3D11Device d3d11_device_;
};

HRESULT STDMETHODCALLTYPE MTLDXGIMetalLayerFactory::GetMetalLayerFromHwnd(
    HWND hWnd, CA::MetalLayer **ppMetalLayer, void **ppNativeView) {
  if (ppMetalLayer == nullptr || ppNativeView == nullptr)
    return E_POINTER;

  *ppMetalLayer = nullptr;
  *ppNativeView = nullptr;

  auto data = get_win_data(hWnd);

  auto metalView = macdrv_view_create_metal_view(
      data->client_cocoa_view, (macdrv_metal_device)context->GetMTLDevice());

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

HRESULT STDMETHODCALLTYPE
MTLDXGIMetalLayerFactory::ReleaseMetalLayer(HWND hWnd, void *pNativeView) {
  macdrv_view_release_metal_view((macdrv_metal_view)pNativeView);

  return S_OK;
}

MTLD3D11Device::MTLD3D11Device(MTLD3D11DXGIDevice *container,
                               D3D_FEATURE_LEVEL FeatureLevel,
                               UINT FeatureFlags)
    : m_container(container), m_FeatureLevel(FeatureLevel),
      m_FeatureFlags(FeatureFlags), m_features(container->GetMTLDevice()) {}

MTLD3D11Device::~MTLD3D11Device() { context_ = nullptr; }

HRESULT STDMETHODCALLTYPE MTLD3D11Device::QueryInterface(REFIID riid,
                                                         void **ppvObject) {
  return m_container->QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE MTLD3D11Device::AddRef() {
  return m_container->AddRef();
}

ULONG STDMETHODCALLTYPE MTLD3D11Device::Release() {
  return m_container->Release();
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateBuffer(
    const D3D11_BUFFER_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData,
    ID3D11Buffer **ppBuffer) {
  InitReturnPtr(ppBuffer);

  try {
    switch (pDesc->Usage) {
    case D3D11_USAGE_DEFAULT:
    case D3D11_USAGE_IMMUTABLE:
      *ppBuffer = CreateDeviceBuffer(this, pDesc, pInitialData);
      break;
    case D3D11_USAGE_DYNAMIC:
      *ppBuffer = CreateDynamicBuffer(this, pDesc, pInitialData);
      break;
    case D3D11_USAGE_STAGING:
      IMPLEMENT_ME
      break;
    }
    return S_OK;
  } catch (const MTLD3DError &err) {
    ERR(err.message());
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateTexture1D(
    const D3D11_TEXTURE1D_DESC *pDesc,
    const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture1D **ppTexture1D) {

  InitReturnPtr(ppTexture1D);

  if (!pDesc)
    return E_INVALIDARG;

  if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED)
    return E_INVALIDARG; // not supported yet

  IMPLEMENT_ME;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateTexture2D(
    const D3D11_TEXTURE2D_DESC *pDesc,
    const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture2D **ppTexture2D) {
  InitReturnPtr(ppTexture2D);

  if (!pDesc)
    return E_INVALIDARG;

  if ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED))
    return E_INVALIDARG; // not supported yet

  try {
    switch (pDesc->Usage) {
    case D3D11_USAGE_DEFAULT:
    case D3D11_USAGE_IMMUTABLE:
      *ppTexture2D = CreateDeviceTexture2D(this, pDesc, pInitialData);
      break;
    case D3D11_USAGE_DYNAMIC:
      IMPLEMENT_ME;
      break;
    case D3D11_USAGE_STAGING:
      if (pDesc->BindFlags != 0) {
        return E_INVALIDARG;
      }
      *ppTexture2D = CreateStagingTexture2D(this, pDesc, pInitialData);
      break;
    }
    return S_OK;
  } catch (const MTLD3DError &err) {
    ERR(err.message());
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateTexture3D(
    const D3D11_TEXTURE3D_DESC *pDesc,
    const D3D11_SUBRESOURCE_DATA *pInitialData, ID3D11Texture3D **ppTexture3D) {
  InitReturnPtr(ppTexture3D);

  if (!pDesc)
    return E_INVALIDARG;

  if ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED))
    return E_INVALIDARG; // not supported yet

  IMPLEMENT_ME;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateShaderResourceView(
    ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
    ID3D11ShaderResourceView **ppSRView) {
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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateUnorderedAccessView(
    ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
    ID3D11UnorderedAccessView **ppUAView) {
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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateRenderTargetView(
    ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
    ID3D11RenderTargetView **ppRTView) {
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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateDepthStencilView(
    ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
    ID3D11DepthStencilView **ppDepthStencilView) {
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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements,
    const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength,
    ID3D11InputLayout **ppInputLayout) {
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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateVertexShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader) {
  InitReturnPtr(ppVertexShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppVertexShader) {
    // FIXME: we didn't really check if the shader bytecode is valid
    return S_FALSE;
  }

  try {
    *ppVertexShader =
        dxmt::CreateVertexShader(this, pShaderBytecode, BytecodeLength);
    return S_OK;
  } catch (const MTLD3DError &err) {
    ERR(err.message());
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateGeometryShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage,
    ID3D11GeometryShader **ppGeometryShader) {
  InitReturnPtr(ppGeometryShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppGeometryShader)
    return S_FALSE;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateGeometryShaderWithStreamOutput(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries,
    const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream,
    ID3D11ClassLinkage *pClassLinkage,
    ID3D11GeometryShader **ppGeometryShader) {
  InitReturnPtr(ppGeometryShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreatePixelShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader) {
  InitReturnPtr(ppPixelShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppPixelShader) {
    // FIXME: we didn't really check if the shader bytecode is valid
    return S_FALSE;
  }
  try {
    *ppPixelShader =
        dxmt::CreatePixelShader(this, pShaderBytecode, BytecodeLength);
    return S_OK;
  } catch (const MTLD3DError &err) {
    ERR(err.message());
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateHullShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader) {
  InitReturnPtr(ppHullShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppHullShader)
    return S_FALSE;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateDomainShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader) {
  InitReturnPtr(ppDomainShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppDomainShader)
    return S_FALSE;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateComputeShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage, ID3D11ComputeShader **ppComputeShader) {
  InitReturnPtr(ppComputeShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppComputeShader) {
    // FIXME: we didn't really check if the shader bytecode is valid
    return S_FALSE;
  }

  try {
    *ppComputeShader =
        dxmt::CreateComputeShader(this, pShaderBytecode, BytecodeLength);
    return S_OK;
  } catch (const MTLD3DError &err) {
    ERR(err.message());
    return E_FAIL;
  }
}

HRESULT STDMETHODCALLTYPE
MTLD3D11Device::CreateClassLinkage(ID3D11ClassLinkage **ppLinkage) {
  *ppLinkage = ref(new MTLD3D11ClassLinkage(this));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateBlendState(
    const D3D11_BLEND_DESC *pBlendStateDesc, ID3D11BlendState **ppBlendState) {
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
    desc.RenderTarget[i].DestBlend = pBlendStateDesc->RenderTarget[i].DestBlend;
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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC *pDesc,
    ID3D11DepthStencilState **ppDepthStencilState) {
  return dxmt::CreateDepthStencilState(this, pDesc, ppDepthStencilState);
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateRasterizerState(
    const D3D11_RASTERIZER_DESC *pRasterizerDesc,
    ID3D11RasterizerState **ppRasterizerState) {
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
MTLD3D11Device::CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc,
                                   ID3D11SamplerState **ppSamplerState) {
  return dxmt::CreateSamplerState(this, pSamplerDesc, ppSamplerState);
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateQuery(
    const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery) {
  InitReturnPtr(ppQuery);

  if (!pQueryDesc)
    return E_INVALIDARG;

  switch (pQueryDesc->Query) {
  case D3D11_QUERY_EVENT:
    // TODO: implement it at command chunk boundary
    return S_OK;
  case D3D11_QUERY_TIMESTAMP:

    if (!ppQuery)
      return S_FALSE;

    *ppQuery = ref(new MTLD3D11TimeStampQuery(this, *pQueryDesc));
    return S_OK;
  case D3D11_QUERY_TIMESTAMP_DISJOINT:

    if (!ppQuery)
      return S_FALSE;

    *ppQuery = ref(new MTLD3D11TimeStampDisjointQuery(this, *pQueryDesc));
    return S_OK;
  case D3D11_QUERY_OCCLUSION:
    // TODO: implement occulusion query
    return E_NOTIMPL;
  default:
    return E_NOTIMPL;
  }
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreatePredicate(
    const D3D11_QUERY_DESC *pPredicateDesc, ID3D11Predicate **ppPredicate) {
  return CreateQuery(pPredicateDesc,
                     reinterpret_cast<ID3D11Query **>(ppPredicate));
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateCounter(
    const D3D11_COUNTER_DESC *pCounterDesc, ID3D11Counter **ppCounter) {
  InitReturnPtr(ppCounter);
  WARN("Not supported");
  return DXGI_ERROR_UNSUPPORTED;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateDeferredContext(
    UINT ContextFlags, ID3D11DeviceContext **ppDeferredContext){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::OpenSharedResource(
    HANDLE hResource, REFIID ReturnedInterface, void **ppResource){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CheckFormatSupport(
    DXGI_FORMAT Format, UINT *pFormatSupport) {

  if (pFormatSupport) {
    *pFormatSupport = 0;
  }

  MTL_FORMAT_DESC metal_format;

  if (FAILED(m_container->adapter_->QueryFormatDesc(Format, &metal_format))) {
    return S_OK;
  }

  UINT outFormatSupport = 0;

  // All graphics and compute kernels can read or sample a texture with any
  // pixel format.
  outFormatSupport |=
      D3D11_FORMAT_SUPPORT_SHADER_LOAD | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE |
      D3D11_FORMAT_SUPPORT_SHADER_GATHER |
      D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD | D3D11_FORMAT_SUPPORT_CPU_LOCKABLE;

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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CheckMultisampleQualityLevels(
    DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels) {
  MTL_FORMAT_DESC desc;
  m_container->adapter_->QueryFormatDesc(Format, &desc);
  if (desc.PixelFormat == MTL::PixelFormatInvalid) {
    return E_FAIL;
  }

  // MSDN:
  // FEATURE_LEVEL_11_0 devices are required to support 4x MSAA for all render
  // target formats, and 8x MSAA for all render target formats except
  // R32G32B32A32 formats.

  // seems some pixel format doesn't support MSAA
  // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

  if (GetMTLDevice()->supportsTextureSampleCount(SampleCount)) {
    *pNumQualityLevels = 1; // always 1: in metal there is no concept of Quality
                            // Level (so is it in vulkan iirc)
    return S_OK;
  }
  return S_FALSE;
}

void STDMETHODCALLTYPE MTLD3D11Device::CheckCounterInfo(
    D3D11_COUNTER_INFO *pCounterInfo) { // We basically don't support counters
  pCounterInfo->LastDeviceDependentCounter = D3D11_COUNTER(0);
  pCounterInfo->NumSimultaneousCounters = 0;
  pCounterInfo->NumDetectableParallelUnits = 0;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CheckCounter(
    const D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType,
    UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits,
    UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength) {
  WARN("Not supported");
  return DXGI_ERROR_UNSUPPORTED;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CheckFeatureSupport(
    D3D11_FEATURE Feature, void *pFeatureSupportData,
    UINT FeatureSupportDataSize) {
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
    auto info =
        static_cast<D3D11_FEATURE_DATA_FORMAT_SUPPORT2 *>(pFeatureSupportData);

    if (FeatureSupportDataSize != sizeof(*info))
      return E_INVALIDARG;
    info->OutFormatSupport2 = 0;

    MTL_FORMAT_DESC desc;
    if (FAILED(m_container->adapter_->QueryFormatDesc(info->InFormat, &desc))) {
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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::GetPrivateData(REFGUID guid,
                                                         UINT *pDataSize,
                                                         void *pData) {
  return m_container->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::SetPrivateData(REFGUID guid,
                                                         UINT DataSize,
                                                         const void *pData) {
  return m_container->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
MTLD3D11Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) {
  return m_container->SetPrivateDataInterface(guid, pData);
}

D3D_FEATURE_LEVEL STDMETHODCALLTYPE MTLD3D11Device::GetFeatureLevel() {
  return m_FeatureLevel;
}

UINT STDMETHODCALLTYPE MTLD3D11Device::GetCreationFlags() {
  return m_FeatureFlags;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::GetDeviceRemovedReason() {
  // unless we are deal with eGPU, this method should awalys return S_OK?
  return S_OK;
}

void STDMETHODCALLTYPE
MTLD3D11Device::GetImmediateContext(ID3D11DeviceContext **ppImmediateContext) {
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

HRESULT STDMETHODCALLTYPE MTLD3D11Device::SetExceptionMode(UINT RaiseFlags) {
  ERR("Not implemented");
  return E_NOTIMPL;
}

UINT STDMETHODCALLTYPE MTLD3D11Device::GetExceptionMode() {
  ERR("Not implemented");
  return 0;
}

void STDMETHODCALLTYPE MTLD3D11Device::GetImmediateContext1(
    ID3D11DeviceContext1 **ppImmediateContext) {
  *ppImmediateContext = context_.ref();
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateDeferredContext1(
    UINT ContextFlags, ID3D11DeviceContext1 **ppDeferredContext){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE
    MTLD3D11Device::CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc,
                                      ID3D11BlendState1 **ppBlendState) {
  return dxmt::CreateBlendState(this, pBlendStateDesc, ppBlendState);
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateRasterizerState1(
    const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
    ID3D11RasterizerState1 **ppRasterizerState) {
  return dxmt::CreateRasterizerState(this, pRasterizerDesc, ppRasterizerState);
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateDeviceContextState(
    UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
    UINT SDKVersion, REFIID EmulatedInterface,
    D3D_FEATURE_LEVEL *pChosenFeatureLevel,
    ID3DDeviceContextState **ppContextState) {
  InitReturnPtr(ppContextState);

  // TODO: validation

  if (ppContextState == nullptr) {
    return S_FALSE;
  }
  *ppContextState = ref(new MTLD3D11DeviceContextState(this));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::OpenSharedResource1(
    HANDLE hResource, REFIID returnedInterface, void **ppResource){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::OpenSharedResourceByName(
    LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface,
    void **ppResource){IMPLEMENT_ME}

MTL::Device *STDMETHODCALLTYPE MTLD3D11Device::GetMTLDevice() {
  return m_container->GetMTLDevice();
}

void MTLD3D11Device::GetAdapter(IMTLDXGIAdatper **ppAdapter) {
  *ppAdapter = m_container->adapter_.ref();
}

void MTLD3D11Device::SubmitThreadgroupWork(IMTLThreadpoolWork *pWork,
                                           THREADGROUP_WORK_STATE *pState) {
  pool_.enqueue(pWork, pState);
}

VOID MTLD3D11Device::WaitThreadgroupWork(THREADGROUP_WORK_STATE *pState) {
  pool_.wait(pState);
}

MTLD3D11DXGIDevice::MTLD3D11DXGIDevice(IMTLDXGIAdatper *adapter,
                                       D3D_FEATURE_LEVEL feature_level,
                                       UINT feature_flags)
    : adapter_(adapter), metal_layer_factory(this),
      d3d11_device_(this, feature_level, feature_flags) {}

MTLD3D11DXGIDevice::~MTLD3D11DXGIDevice() {}
HRESULT
STDMETHODCALLTYPE
MTLD3D11DXGIDevice::QueryInterface(REFIID riid, void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
      riid == __uuidof(IDXGIDevice) || riid == __uuidof(IDXGIDevice1) ||
      riid == __uuidof(IDXGIDevice2) || riid == __uuidof(IMTLDXGIDevice)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (riid == __uuidof(ID3D11Device) || riid == __uuidof(ID3D11Device1) ||
      riid == __uuidof(IMTLD3D11Device)) {
    *ppvObject = ref(&d3d11_device_);
    return S_OK;
  }

  if (riid == __uuidof(IDXGIMetalLayerFactory)) {
    *ppvObject = ref(&metal_layer_factory);
    return S_OK;
  }

  if (riid == __uuidof(ID3D11Debug))
    return E_NOINTERFACE;

  if (logQueryInterfaceError(__uuidof(IMTLDXGIDevice), riid)) {
    WARN("D3D11Device: Unknown interface query ", str::format(riid));
  }

  return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE MTLD3D11DXGIDevice::GetParent(REFIID riid,
                                                        void **ppParent) {
  return adapter_->QueryInterface(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE
MTLD3D11DXGIDevice::GetAdapter(IDXGIAdapter **pAdapter) {
  if (pAdapter == nullptr)
    return DXGI_ERROR_INVALID_CALL;

  *pAdapter = adapter_.ref();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11DXGIDevice::CreateSurface(
    const DXGI_SURFACE_DESC *desc, UINT surface_count, DXGI_USAGE usage,
    const DXGI_SHARED_RESOURCE *shared_resource,
    IDXGISurface **surface){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE MTLD3D11DXGIDevice::QueryResourceResidency(
    IUnknown *const *resources, DXGI_RESIDENCY *residency,
    UINT resource_count){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE
    MTLD3D11DXGIDevice::SetGPUThreadPriority(INT Priority) {
  if (Priority < -7 || Priority > 7)
    return E_INVALIDARG;

  WARN("SetGPUThreadPriority: Ignoring");
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D11DXGIDevice::GetGPUThreadPriority(INT *pPriority) {
  *pPriority = 0;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D11DXGIDevice::SetMaximumFrameLatency(UINT MaxLatency){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE
    MTLD3D11DXGIDevice::GetMaximumFrameLatency(UINT *pMaxLatency){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE MTLD3D11DXGIDevice::OfferResources(
    UINT NumResources, IDXGIResource *const *ppResources,
    DXGI_OFFER_RESOURCE_PRIORITY Priority) {
  // stub
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11DXGIDevice::ReclaimResources(
    UINT NumResources, IDXGIResource *const *ppResources, WINBOOL *pDiscarded) {
  // stub
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11DXGIDevice::EnqueueSetEvent(HANDLE hEvent) {
  return E_FAIL;
}

MTL::Device *STDMETHODCALLTYPE MTLD3D11DXGIDevice::GetMTLDevice() {
  return adapter_->GetMTLDevice();
}

HRESULT STDMETHODCALLTYPE MTLD3D11DXGIDevice::CreateSwapChain(
    IDXGIFactory1 *pFactory, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGISwapChain1 **ppSwapChain) {

  return dxmt::CreateSwapChain(pFactory, this, hWnd, &this->metal_layer_factory,
                               pDesc, pFullscreenDesc, ppSwapChain);
}

Com<IMTLDXGIDevice> CreateD3D11Device(IMTLDXGIAdatper *adapter,
                                      D3D_FEATURE_LEVEL feature_level,
                                      UINT feature_flags) {
  return new MTLD3D11DXGIDevice(adapter, feature_level, feature_flags);
};
} // namespace dxmt