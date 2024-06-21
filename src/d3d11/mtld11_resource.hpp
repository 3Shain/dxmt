#pragma once
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_1.h"
#include "d3d11_device_child.hpp"
#include "com/com_pointer.hpp"
#include "com/com_guid.hpp"
#include "d3d11_view.hpp"
#include "dxgi_resource.hpp"
#include "dxmt_binding.hpp"
#include "dxmt_resource_binding.hpp"
#include "log/log.hpp"
#include "mtld11_interfaces.hpp"
#include <memory>
#include <type_traits>

typedef struct MappedResource {
  void *pData;
  uint32_t RowPitch;
  uint32_t DepthPitch;
} MappedResource;

DEFINE_COM_INTERFACE("d8a49d20-9a1f-4bb8-9ee6-442e064dce23", IDXMTResource)
    : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
      const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
      ID3D11ShaderResourceView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
      const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
      ID3D11UnorderedAccessView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
      const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
      ID3D11RenderTargetView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
      const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
      ID3D11DepthStencilView **ppView) = 0;
};

enum MTL_CTX_BIND_TYPE {
  MTL_BIND_BUFFER_UNBOUNDED = 0,
  MTL_BIND_TEXTURE = 1,
  MTL_BIND_BUFFER_BOUNDED = 2,
  // MTL_BIND_TEXTURE_MINLOD = 3,
  MTL_BIND_UAV_WITH_COUNTER = 4
  // MTL_BIND_SWAPCHAIN_BACKBUFFER = 5,
};

struct MTL_BIND_RESOURCE {
  MTL_CTX_BIND_TYPE Type;
  UINT Padding_unused;
  union {
    MTL::Buffer *Buffer;
    MTL::Texture *Texture;
  };
  uint32_t BoundOffset;
  uint32_t BoundSize;
};

DEFINE_COM_INTERFACE("1c7e7c98-6dd4-42f0-867b-67960806886e", IMTLBindable)
    : public IUnknown {
  /**
  Note: the resource object is NOT retained by design
  */
  // virtual void GetBoundResource(MTL_BIND_RESOURCE * ppResource) = 0;

  // I swear I will use my own rtti system instead of COM
  // it's just weird to involve c++ class in a COM interface
  // the argument `bindAtSeqId` is not checked
  virtual dxmt::BindingRef GetBinding(uint64_t bindAtSeqId) = 0;
  virtual dxmt::ArgumentData GetArgumentData() = 0;
  // it's only used as a potential to optimize resource updating
  // generally assume it return `true` meaning it's in use
  virtual bool GetContentionState(uint64_t finishedSeqId) = 0;
  virtual void GetLogicalResourceOrView(REFIID riid,
                                        void **ppLogicalResource) = 0;
};

DEFINE_COM_INTERFACE("daf21510-d136-44dd-bb16-068a94690775",
                     IMTLD3D11BackBuffer)
    : public IUnknown {
  virtual void Swap() = 0;
  virtual CA::MetalDrawable *CurrentDrawable() = 0;
  virtual void Destroy() = 0;
};

DEFINE_COM_INTERFACE("65feb8c5-01de-49df-bf58-d115007a117d", IMTLDynamicBuffer)
    : public IUnknown {
  virtual void *GetMappedMemory(UINT * pBytesPerRow) = 0;
  virtual void RotateBuffer(IMTLDynamicBufferExchange * pool) = 0;
};

using BufferSwapCallback = std::function<void(MTL::Buffer *resource)>;

DEFINE_COM_INTERFACE("0988488c-75fb-44f3-859a-b6fb2d022239",
                     IMTLDynamicBindable)
    : public IUnknown {
  /**
   */
  virtual void GetBindable(IMTLBindable * *ppResource,
                           BufferSwapCallback && onBufferSwap) = 0;
};

DEFINE_COM_INTERFACE("252c1a0e-1c61-42e7-9b57-23dfe3d73d49", IMTLD3D11Staging)
    : public IUnknown {

  virtual bool UseCopyDestination(
      uint32_t Subresource, uint64_t seq_id, MTL_BIND_RESOURCE * pBuffer,
      uint32_t * pBytesPerRow, uint32_t * pBytesPerImage) = 0;
  virtual bool UseCopySource(
      uint32_t Subresource, uint64_t seq_id, MTL_BIND_RESOURCE * pBuffer,
      uint32_t * pBytesPerRow, uint32_t * pBytesPerImage) = 0;
  /**
  cpu_coherent_seq_id: any operation at/before seq_id is coherent to cpu
  -
  return:
  = 0 - map success
  > 0 - resource in use
  < 0 - error (?)
  */
  virtual int64_t TryMap(uint32_t Subresource, uint64_t cpu_coherent_seq_id,
                         // uint64_t commited_seq_id,
                         D3D11_MAP flag,
                         D3D11_MAPPED_SUBRESOURCE * pMappedResource) = 0;
  virtual void Unmap(uint32_t Subresource) = 0;
};

namespace dxmt {

struct tag_buffer {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_BUFFER;
  using COM = ID3D11Buffer;
  using DESC = D3D11_BUFFER_DESC;
  using DESC_S = D3D11_BUFFER_DESC;
};

struct tag_texture_1d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE1D;
  using COM = ID3D11Texture1D;
  using DESC = D3D11_TEXTURE1D_DESC;
  using DESC_S = D3D11_TEXTURE1D_DESC;
};

struct tag_texture_2d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  using COM = ID3D11Texture2D;
  using DESC = D3D11_TEXTURE2D_DESC;
  using DESC_S = D3D11_TEXTURE2D_DESC;
};

struct tag_texture_3d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE3D;
  using COM = ID3D11Texture3D;
  using DESC = D3D11_TEXTURE3D_DESC;
  using DESC_S = D3D11_TEXTURE3D_DESC;
};

template <typename tag, typename... Base>
class TResourceBase
    : public MTLD3D11DeviceChild<typename tag::COM, IDXMTResource, Base...> {
public:
  TResourceBase(const tag::DESC_S *pDesc, IMTLD3D11Device *device)
      : MTLD3D11DeviceChild<typename tag::COM, IDXMTResource, Base...>(device),
        dxgi_resource(new MTLDXGIResource<TResourceBase<tag, Base...>>(this)) {
    if (pDesc) {
      desc = *pDesc;
    }
  }

  template <std::size_t n> HRESULT ResolveBase(REFIID riid, void **ppvObject) {
    return E_NOINTERFACE;
  };

  template <std::size_t n, typename V, typename... Args>
  HRESULT ResolveBase(REFIID riid, void **ppvObject) {
    if (riid == __uuidof(V)) {
      *ppvObject = ref_and_cast<V>(this);
      return S_OK;
    }
    return ResolveBase<n + 1, Args...>(riid, ppvObject);
  };

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    auto hr = ResolveBase<0, Base...>(riid, ppvObject);
    if (SUCCEEDED(hr)) {
      return S_OK;
    }

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11Resource) ||
        riid == __uuidof(typename tag::COM)) {
      *ppvObject = ref_and_cast<typename tag::COM>(this);
      return S_OK;
    }

    if (riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGIResource) || riid == __uuidof(IDXGIResource1)) {
      *ppvObject = ref(dxgi_resource.get());
      return S_OK;
    }

    if (riid == __uuidof(IDXMTResource)) {
      *ppvObject = ref_and_cast<IDXMTResource>(this);
      return S_OK;
    }

    if (riid == __uuidof(IMTLDynamicBuffer) || riid == __uuidof(IMTLBindable) ||
        riid == __uuidof(IMTLDynamicBindable) ||
        riid == __uuidof(IMTLD3D11Staging)) {
      // silent these interfaces
      return E_NOINTERFACE;
    }

    if (logQueryInterfaceError(__uuidof(typename tag::COM), riid)) {
      WARN("D3D11Resource: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void GetDesc(tag::DESC *pDesc) final { *pDesc = desc; }

  void GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) final {
    *pResourceDimension = tag::dimension;
  }

  void SetEvictionPriority(UINT EvictionPriority) final {}

  UINT GetEvictionPriority() final { return DXGI_RESOURCE_PRIORITY_NORMAL; }

  virtual HRESULT GetDeviceInterface(REFIID riid, void **ppDevice) {
    Com<ID3D11Device> device;
    this->GetDevice(&device);
    return device->QueryInterface(riid, ppDevice);
  };

  virtual HRESULT GetDXGIUsage(DXGI_USAGE *pUsage) {
    if (!pUsage) {
      return E_INVALIDARG;
    }
    *pUsage = 0;
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                           ID3D11ShaderResourceView **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                            ID3D11UnorderedAccessView **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                         ID3D11RenderTargetView **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                         ID3D11DepthStencilView **ppView) {
    return E_INVALIDARG;
  };

protected:
  tag::DESC_S desc;
  std::unique_ptr<IDXGIResource1> dxgi_resource;
};

template <typename RESOURCE_IMPL_ = ID3D11Resource,
          typename COM_IMPL_ = IMTLD3D11RenderTargetView>
struct tag_render_target_view {
  using COM = ID3D11RenderTargetView;
  using COM_IMPL = COM_IMPL_;
  using RESOURCE = ID3D11Resource;
  using RESOURCE_IMPL = RESOURCE_IMPL_;
  using DESC = D3D11_RENDER_TARGET_VIEW_DESC;
  using DESC_S = D3D11_RENDER_TARGET_VIEW_DESC;
};

template <typename RESOURCE_IMPL_ = ID3D11Resource,
          typename COM_IMPL_ = IMTLD3D11DepthStencilView>
struct tag_depth_stencil_view {
  using COM = ID3D11DepthStencilView;
  using COM_IMPL = COM_IMPL_;
  using RESOURCE = ID3D11Resource;
  using RESOURCE_IMPL = RESOURCE_IMPL_;
  using DESC = D3D11_DEPTH_STENCIL_VIEW_DESC;
  using DESC_S = D3D11_DEPTH_STENCIL_VIEW_DESC;
};

template <typename RESOURCE_IMPL_ = ID3D11Resource,
          typename COM_IMPL_ = IMTLD3D11ShaderResourceView>
struct tag_shader_resource_view {
  using COM = ID3D11ShaderResourceView;
  using COM_IMPL = COM_IMPL_;
  using RESOURCE = ID3D11Resource;
  using RESOURCE_IMPL = RESOURCE_IMPL_;
  using DESC = D3D11_SHADER_RESOURCE_VIEW_DESC;
  using DESC_S = D3D11_SHADER_RESOURCE_VIEW_DESC;
};

template <typename RESOURCE_IMPL_ = ID3D11Resource,
          typename COM_IMPL_ = IMTLD3D11UnorderedAccessView>
struct tag_unordered_access_view {
  using COM = ID3D11UnorderedAccessView;
  using COM_IMPL = COM_IMPL_;
  using RESOURCE = ID3D11Resource;
  using RESOURCE_IMPL = RESOURCE_IMPL_;
  using DESC = D3D11_UNORDERED_ACCESS_VIEW_DESC;
  using DESC_S = D3D11_UNORDERED_ACCESS_VIEW_DESC;
};

template <typename tag, typename... Base>
class TResourceViewBase
    : public MTLD3D11DeviceChild<typename tag::COM_IMPL, Base...> {
public:
  TResourceViewBase(const tag::DESC_S *pDesc, tag::RESOURCE_IMPL *pResource,
                    IMTLD3D11Device *device)
      : MTLD3D11DeviceChild<typename tag::COM_IMPL, Base...>(device),
        resource(pResource) {
    if (pDesc) {
      desc = *pDesc;
    }
  }

  template <std::size_t n> HRESULT ResolveBase(REFIID riid, void **ppvObject) {
    return E_NOINTERFACE;
  };

  template <std::size_t n, typename V, typename... Args>
  HRESULT ResolveBase(REFIID riid, void **ppvObject) {
    if (riid == __uuidof(V)) {
      *ppvObject = ref_and_cast<V>(this);
      return S_OK;
    }
    return ResolveBase<n + 1, Args...>(riid, ppvObject);
  };

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    auto hr = ResolveBase<0, Base...>(riid, ppvObject);
    if (SUCCEEDED(hr)) {
      return S_OK;
    }

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) || riid == __uuidof(typename tag::COM) ||
        riid == __uuidof(typename tag::COM_IMPL)) {
      *ppvObject = ref_and_cast<typename tag::COM>(this);
      return S_OK;
    }

    if (riid == __uuidof(IMTLDynamicBuffer) || riid == __uuidof(IMTLBindable) ||
        riid == __uuidof(IMTLDynamicBindable)) {
      // silent these interfaces
      return E_NOINTERFACE;
    }

    if (logQueryInterfaceError(__uuidof(typename tag::COM_IMPL), riid)) {
      WARN("D3D11View: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void GetDesc(tag::DESC *pDesc) final { *pDesc = desc; }

  void GetResource(tag::RESOURCE **ppResource) final {
    resource->QueryInterface(IID_PPV_ARGS(ppResource));
  }

  virtual ULONG64 GetUnderlyingResourceId() { return (ULONG64)resource.ptr(); };

  virtual dxmt::ResourceSubset GetViewRange() { return ResourceSubset(desc); };

  virtual bool GetContentionState(uint64_t finishedSeqId) { return true; };

protected:
  tag::DESC_S desc;
  /**
  strong ref to resource
  */
  Com<typename tag::RESOURCE_IMPL> resource;
};

#pragma region Resource Factory

HRESULT
CreateStagingBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                    ID3D11Buffer **ppBuffer);

HRESULT
CreateStagingTexture1D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE1D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture1D **ppTexture);

HRESULT
CreateStagingTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture2D **ppTexture);

HRESULT
CreateStagingTexture3D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE3D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture3D **ppTexture);

HRESULT
CreateDeviceBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                   const D3D11_SUBRESOURCE_DATA *pInitialData,
                   ID3D11Buffer **ppBuffer);

HRESULT CreateDeviceTexture1D(IMTLD3D11Device *pDevice,
                              const D3D11_TEXTURE1D_DESC *pDesc,
                              const D3D11_SUBRESOURCE_DATA *pInitialData,
                              ID3D11Texture1D **ppTexture);

HRESULT CreateDeviceTexture2D(IMTLD3D11Device *pDevice,
                              const D3D11_TEXTURE2D_DESC *pDesc,
                              const D3D11_SUBRESOURCE_DATA *pInitialData,
                              ID3D11Texture2D **ppTexture);

HRESULT CreateDeviceTexture3D(IMTLD3D11Device *pDevice,
                              const D3D11_TEXTURE3D_DESC *pDesc,
                              const D3D11_SUBRESOURCE_DATA *pInitialData,
                              ID3D11Texture3D **ppTexture);

HRESULT
CreateDynamicBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                    ID3D11Buffer **ppBuffer);

HRESULT CreateDynamicTexture2D(IMTLD3D11Device *pDevice,
                               const D3D11_TEXTURE2D_DESC *pDesc,
                               const D3D11_SUBRESOURCE_DATA *pInitialData,
                               ID3D11Texture2D **ppTexture);
#pragma endregion

#pragma region Helper
template <typename RESOURCE_DESC, typename VIEW_DESC>
HRESULT ExtractEntireResourceViewDescription(const RESOURCE_DESC *pResourceDesc,
                                             VIEW_DESC *pViewDescOut);

template <typename RESOURCE_DESC, typename VIEW_DESC>
HRESULT ExtractEntireResourceViewDescription(const RESOURCE_DESC *pResourceDesc,
                                             const VIEW_DESC *pViewDescIn,
                                             VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    if constexpr (!std::is_same_v<RESOURCE_DESC, D3D11_BUFFER_DESC>) {
      if (pViewDescOut->Format == DXGI_FORMAT_UNKNOWN) {
        pViewDescOut->Format = pResourceDesc->Format;
      }
    }
    return S_OK;
  } else {
    return ExtractEntireResourceViewDescription(pResourceDesc, pViewDescOut);
  }
}

template <typename TEXTURE_DESC>
HRESULT CreateMTLTextureDescriptor(IMTLD3D11Device *pDevice,
                                   const TEXTURE_DESC *pDesc,
                                   TEXTURE_DESC *pOutDesc,
                                   MTL::TextureDescriptor **pMtlDescOut);

template <typename VIEW_DESC>
HRESULT CreateMTLTextureView(IMTLD3D11Device *pDevice, MTL::Texture *pResource,
                             const VIEW_DESC *pViewDesc, MTL::Texture **ppView);

template <typename VIEW_DESC>
HRESULT CreateMTLTextureView(IMTLD3D11Device *pDevice, MTL::Buffer *pResource,
                             const VIEW_DESC *pViewDesc, MTL::Texture **ppView);

template <typename TEXTURE_DESC>
void GetMipmapSize(const TEXTURE_DESC *pDesc, uint32_t level, uint32_t *pWidth,
                   uint32_t *pHeight, uint32_t *pDepth);

template <typename TEXTURE_DESC>
HRESULT GetLinearTextureLayout(IMTLD3D11Device *pDevice,
                               const TEXTURE_DESC *pDesc, uint32_t level,
                               uint32_t *pBytesPerRow, uint32_t *pBytesPerImage,
                               uint32_t *pBytesPerSlice);
#pragma endregion

} // namespace dxmt