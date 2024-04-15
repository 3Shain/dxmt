#include "com/com_guid.hpp"
#include "d3d11_class_linkage.hpp"
#include "d3d11_inspection.hpp"
#include "d3d11_context.hpp"
#include "d3d11_context_state.hpp"
#include "d3d11_device.hpp"
#include "d3d11_input_layout.hpp"
#include "d3d11_query.hpp"
#include "d3d11_swapchain.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_state_object.hpp"
#include "mtld11_resource.hpp"
#include "winemacdrv.h"
#include "com/com_aggregatable.hpp"
#include "dxgi_object.h"
#include <winerror.h>

namespace dxmt {

class MTLD3D11DXGIDevice;
class MTLDXGIMetalLayerFactory
    : public ComAggregatedObject<MTLD3D11DXGIDevice, IDXGIMetalLayerFactory> {
public:
  friend class MTLD3D11DeviceContext;
  MTLDXGIMetalLayerFactory(MTLD3D11DXGIDevice *container)
      : ComAggregatedObject(container){};
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

private:
  Com<MTLD3D11DXGIDevice> m_container;
  D3D_FEATURE_LEVEL m_FeatureLevel;
  UINT m_FeatureFlags;
  MTLD3D11Inspection m_features;
  Com<IMTLD3D11DeviceContext> context_;
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
      data->client_cocoa_view, (macdrv_metal_device)outer_->GetMTLDevice());

  if (metalView == nullptr)
    return E_FAIL;

  *ppNativeView = metalView;

  auto layer = (CA::MetalLayer *)macdrv_view_get_metal_layer(metalView);

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
      m_FeatureFlags(FeatureFlags), m_features(container->GetMTLDevice()) {
  context_ = NewD3D11DeviceContext(this);
}

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
    case D3D11_USAGE_DYNAMIC:
    case D3D11_USAGE_STAGING:
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
    case D3D11_USAGE_DYNAMIC:

      IMPLEMENT_ME;
      break;
    case D3D11_USAGE_STAGING:
      if (pDesc->BindFlags != 0) {
        return E_INVALIDARG;
      }
      IMPLEMENT_ME;
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

  auto hash = Sha1Hash::compute(pShaderBytecode, BytecodeLength);

  if (!ppVertexShader)
    return S_FALSE;

  *ppVertexShader = ref(
      new MTLD3D11VertexShader(this, hash, pShaderBytecode, BytecodeLength));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateGeometryShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage,
    ID3D11GeometryShader **ppGeometryShader) {
  InitReturnPtr(ppGeometryShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  // auto hash = Sha1Hash::compute(pShaderBytecode, BytecodeLength);

  if (!ppGeometryShader)
    return S_FALSE;

  // *ppGeometryShader = ref(new MTLD3D11GeometryShader(this, , , hash));
  // return S_OK;
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

  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreatePixelShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader) {
  InitReturnPtr(ppPixelShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppPixelShader)
    return S_FALSE;

  auto hash = Sha1Hash::compute(pShaderBytecode, BytecodeLength);

  *ppPixelShader =
      ref(new MTLD3D11PixelShader(this, hash, pShaderBytecode, BytecodeLength));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateHullShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader) {
  InitReturnPtr(ppHullShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppHullShader)
    return S_FALSE;
  // auto hash = Sha1Hash::compute(pShaderBytecode, BytecodeLength);
  // *ppHullShader = ref(new MTLD3D11HullShader(this, , , hash));
  // return S_OK;
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

  // auto hash = Sha1Hash::compute(pShaderBytecode, BytecodeLength);
  // *ppDomainShader = ref(new MTLD3D11DomainShader(this, , , hash));
  // return S_OK;
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateComputeShader(
    const void *pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage *pClassLinkage, ID3D11ComputeShader **ppComputeShader) {
  InitReturnPtr(ppComputeShader);
  if (pClassLinkage != nullptr)
    WARN("Class linkage not supported");

  if (!ppComputeShader)
    return S_FALSE;
  // auto hash = Sha1Hash::compute(pShaderBytecode, BytecodeLength);
  // *ppComputeShader = ref(new MTLD3D11ComputeShader(this, , , hash));
  // TODO
  return S_OK;
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
  InitReturnPtr(ppDepthStencilState);

  // TODO: validation

  if (!ppDepthStencilState)
    return S_FALSE;
  using SD = MTL::StencilDescriptor;

  using DSD = MTL::DepthStencilDescriptor;
  Obj<MTL::DepthStencilDescriptor> dsd = transfer(DSD::alloc()->init());
  if (pDesc->DepthEnable) {
    dsd->setDepthCompareFunction(compareFunctionMap[pDesc->DepthFunc]);
    dsd->setDepthWriteEnabled(pDesc->DepthWriteMask ==
                              D3D11_DEPTH_WRITE_MASK_ALL);
  } else {
    dsd->setDepthCompareFunction(MTL::CompareFunctionAlways);
    dsd->setDepthWriteEnabled(0);
  }

  if (pDesc->StencilEnable) {
    {
      auto fsd = transfer(SD::alloc()->init());
      fsd->setDepthStencilPassOperation(
          stencilOperationMap[pDesc->FrontFace.StencilPassOp]);
      fsd->setStencilFailureOperation(
          stencilOperationMap[pDesc->FrontFace.StencilFailOp]);
      fsd->setDepthFailureOperation(
          stencilOperationMap[pDesc->FrontFace.StencilDepthFailOp]);
      fsd->setStencilCompareFunction(
          compareFunctionMap[pDesc->FrontFace.StencilFunc]);
      fsd->setWriteMask(pDesc->StencilWriteMask);
      fsd->setReadMask(pDesc->StencilReadMask);
      dsd->setFrontFaceStencil(fsd.ptr());
    }
    {
      auto bsd = transfer(SD::alloc()->init());
      bsd->setDepthStencilPassOperation(
          stencilOperationMap[pDesc->BackFace.StencilPassOp]);
      bsd->setStencilFailureOperation(
          stencilOperationMap[pDesc->BackFace.StencilFailOp]);
      bsd->setDepthFailureOperation(
          stencilOperationMap[pDesc->BackFace.StencilDepthFailOp]);
      bsd->setStencilCompareFunction(
          compareFunctionMap[pDesc->BackFace.StencilFunc]);
      bsd->setWriteMask(pDesc->StencilWriteMask);
      bsd->setReadMask(pDesc->StencilReadMask);
      dsd->setBackFaceStencil(bsd.ptr());
    }
  }

  auto state = transfer(GetMTLDevice()->newDepthStencilState(dsd.ptr()));

  *ppDepthStencilState =
      ref(new MTLD3D11DepthStencilState(this, state.ptr(), *pDesc));
  return S_OK;
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

  InitReturnPtr(ppSamplerState);

  if (pSamplerDesc == nullptr)
    return E_INVALIDARG;

  D3D11_SAMPLER_DESC desc = *pSamplerDesc;

  // FIXME: validation

  if (!ppSamplerState)
    return S_FALSE;

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
    mtl_sampler_desc->setSAddressMode(addressModeMap[desc.AddressU - 1]);
  }
  if (desc.AddressV > 0 && desc.AddressV < 6) {
    mtl_sampler_desc->setTAddressMode(addressModeMap[desc.AddressV - 1]);
  }
  if (desc.AddressW > 0 && desc.AddressW < 6) {
    mtl_sampler_desc->setRAddressMode(addressModeMap[desc.AddressW - 1]);
  }

  // comparison function
  if (desc.ComparisonFunc < 1 || desc.ComparisonFunc > 8) {
    ERR("Invalid ComparisonFunc");
  } else {
    mtl_sampler_desc->setCompareFunction(
        compareFunctionMap[desc.ComparisonFunc]);
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
    WARN("Unsupported border color");
  }
  mtl_sampler_desc->setSupportArgumentBuffers(true);

  auto mtl_sampler =
      transfer(GetMTLDevice()->newSamplerState(mtl_sampler_desc.ptr()));

  *ppSamplerState =
      ref(new MTLD3D11SamplerState(this, mtl_sampler.ptr(), desc));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateQuery(
    const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery) {
  InitReturnPtr(ppQuery);

  if (!pQueryDesc)
    return E_INVALIDARG;

  switch (pQueryDesc->Query) {
  case D3D11_QUERY_EVENT:
    // TODO
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
  return m_features.CheckSupportedFormat(Format, pFormatSupport);
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CheckMultisampleQualityLevels(
    DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels) {

  const auto pfmt = g_metal_format_map[Format];
  if (pfmt.pixel_format == MTL::PixelFormatInvalid) {
    return E_FAIL;
  }

  // MSDN:
  // FEATURE_LEVEL_11_0 devices are required to support 4x MSAA for all render
  // target formats, and 8x MSAA for all render target formats except
  // R32G32B32A32 formats.
  // Hmmm I don't know

  // FIXME: seems some pixel format doesn't support MSAA
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

    return m_features.CheckSupportedFormat(info->InFormat,
                                           &info->OutFormatSupport);
  }
    return S_OK;

  case D3D11_FEATURE_FORMAT_SUPPORT2: {
    auto info =
        static_cast<D3D11_FEATURE_DATA_FORMAT_SUPPORT2 *>(pFeatureSupportData);

    if (FeatureSupportDataSize != sizeof(*info))
      return E_INVALIDARG;

    return m_features.CheckSupportedFormat2(info->InFormat,
                                            &info->OutFormatSupport2);
  }
    return S_OK;

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

  // FIXME: unless we are deal with eGPU, this method should awalys return S_OK?
  return S_OK;
}

void STDMETHODCALLTYPE
MTLD3D11Device::GetImmediateContext(ID3D11DeviceContext **ppImmediateContext) {
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
  InitReturnPtr(ppBlendState);

  // TODO: validate
  // if(pBlendStateDesc->AlphaToCoverageEnable)

  if (ppBlendState) {
    *ppBlendState = ref(new MTLD3D11BlendState(this, *pBlendStateDesc));
    return S_OK;
  } else {
    return S_FALSE;
  }
}

HRESULT STDMETHODCALLTYPE MTLD3D11Device::CreateRasterizerState1(
    const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
    ID3D11RasterizerState1 **ppRasterizerState) {
  InitReturnPtr(ppRasterizerState);

  // TODO: validate

  if (ppRasterizerState) {
    *ppRasterizerState =
        ref(new MTLD3D11RasterizerState(this, *pRasterizerDesc));
    return S_OK;
  } else {
    return S_FALSE;
  }
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
    WARN("Unknown interface query: ", str::format(riid));
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

  WARN("Ignoring");
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
  // TODO Stub
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D11DXGIDevice::ReclaimResources(
    UINT NumResources, IDXGIResource *const *ppResources, WINBOOL *pDiscarded) {
  // TODO Stub
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

IMTLDXGIDevice *NewMTLD3D11DXGIDevice(IMTLDXGIAdatper *adapter,
                                      D3D_FEATURE_LEVEL feature_level,
                                      UINT feature_flags) {
  return new MTLD3D11DXGIDevice(adapter, feature_level, feature_flags);
};
} // namespace dxmt