#pragma once
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_1.h"
#include "d3d11_device_child.hpp"
#include "com/com_pointer.hpp"
#include "com/com_guid.hpp"
#include "d3d11_view.hpp"
#include "dxgi_resource.hpp"
#include "dxmt_resource_binding.hpp"
#include "log/log.hpp"
#include "mtld11_interfaces.hpp"
#include <memory>

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

struct MTL_BIND_RESOURCE {
  UINT IsTexture;
  UINT Padding_unused;
  union {
    MTL::Buffer *Buffer;
    MTL::Texture *Texture;
  };
};

DEFINE_COM_INTERFACE("1c7e7c98-6dd4-42f0-867b-67960806886e", IMTLBindable)
    : public IUnknown {
  /**
  Note: the resource object is NOT retained by design
  */
  virtual void GetBoundResource(MTL_BIND_RESOURCE * ppResource) = 0;
  virtual void GetLogicalResourceOrView(REFIID riid,
                                        void **ppLogicalResource) = 0;
};

DEFINE_COM_INTERFACE("daf21510-d136-44dd-bb16-068a94690775",
                     IMTLD3D11BackBuffer)
    : public ID3D11Texture2D {
  virtual void Swap() = 0;
  virtual CA::MetalDrawable *CurrentDrawable() = 0;
};

DEFINE_COM_INTERFACE("65feb8c5-01de-49df-bf58-d115007a117d", IMTLDynamicBuffer)
    : public IUnknown {
  virtual MTL::Buffer *GetCurrentBuffer(UINT * pBytesPerRow) = 0;
  virtual void RotateBuffer(IMTLDynamicBufferPool * pool) = 0;
};

DEFINE_COM_INTERFACE("0988488c-75fb-44f3-859a-b6fb2d022239",
                     IMTLDynamicBindable)
    : public IUnknown {
  /**
   */
  virtual void GetBindable(IMTLBindable * *ppResource,
                           std::function<void()> && onBufferSwap) = 0;
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

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

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

    if (SUCCEEDED(this->PrivateQueryInterface(riid, ppvObject))) {
      return S_OK;
    }

    if (riid == __uuidof(IMTLDynamicBuffer) || riid == __uuidof(IMTLBindable) ||
        riid == __uuidof(IMTLDynamicBindable)) {
      // silent these interfaces if PrivateQueryInterface doesn't provide
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

  virtual HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {
    return E_FAIL;
  };

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
          typename COM_IMPL_ = ID3D11UnorderedAccessView>
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

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) || riid == __uuidof(typename tag::COM) ||
        riid == __uuidof(typename tag::COM_IMPL)) {
      *ppvObject = ref_and_cast<typename tag::COM>(this);
      return S_OK;
    }

    if (SUCCEEDED(this->PrivateQueryInterface(riid, ppvObject))) {
      return S_OK;
    }

    if (riid == __uuidof(IMTLDynamicBuffer) || riid == __uuidof(IMTLBindable) ||
        riid == __uuidof(IMTLDynamicBindable)) {
      // silent these interfaces if PrivateQueryInterface doesn't provide
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

  virtual HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {
    return E_FAIL;
  };

  virtual ULONG64 GetUnderlyingResourceId() { return (ULONG64)resource.ptr(); };

  virtual dxmt::ResourceSubset GetViewRange() { return ResourceSubset(desc); };

protected:
  tag::DESC_S desc;
  /**
  strong ref to resource
  */
  Com<typename tag::RESOURCE_IMPL> resource;
};

#pragma region Resource Factory

Com<ID3D11Buffer>
CreateStagingBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData);

Com<ID3D11Texture1D>
CreateStagingTexture1D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE1D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData);

Com<ID3D11Texture2D>
CreateStagingTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData);

Com<ID3D11Buffer>
CreateDeviceBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                   const D3D11_SUBRESOURCE_DATA *pInitialData);

Com<ID3D11Texture1D>
CreateDeviceTexture1D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE1D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData);

Com<ID3D11Texture2D>
CreateDeviceTexture2D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE2D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData);

Com<ID3D11Texture3D>
CreateDeviceTexture3D(IMTLD3D11Device *pDevice,
                      const D3D11_TEXTURE3D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData);

Com<ID3D11Buffer>
CreateDynamicBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData);

Com<ID3D11Texture2D>
CreateDynamicTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData);
#pragma endregion

} // namespace dxmt