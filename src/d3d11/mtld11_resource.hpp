#pragma once
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_1.h"
#include "d3d11_device_child.hpp"
#include "com/com_pointer.hpp"
#include "com/com_guid.hpp"
#include "dxgi_resource.hpp"
#include "log/log.hpp"
#include <memory>

typedef struct MappedResource {
  void *pData;
  uint32_t RowPitch;
  uint32_t DepthPitch;
} MappedResource;

DEFINE_COM_INTERFACE("d8a49d20-9a1f-4bb8-9ee6-442e064dce23", IDXMTResource)
    : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
      const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
      ID3D11ShaderResourceView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
      const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc,
      ID3D11UnorderedAccessView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
      const D3D11_RENDER_TARGET_VIEW_DESC *desc,
      ID3D11RenderTargetView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
      const D3D11_DEPTH_STENCIL_VIEW_DESC *desc,
      ID3D11DepthStencilView **ppView) = 0;
};

DEFINE_COM_INTERFACE("1c7e7c98-6dd4-42f0-867b-67960806886e", IMTLBuffer)
    : public IUnknown {
  virtual MTL::Buffer *Get() = 0;
};

DEFINE_COM_INTERFACE("4fe0ec8e-8be0-4c41-a9a4-11726ceba59c", IMTLTexture)
    : public IUnknown {
  virtual MTL::Texture *Get() = 0;
};

DEFINE_COM_INTERFACE("8f1b6f77-58c4-4bf4-8ce9-d08318ae70b1",
                     IMTLSwappableBuffer)
    : public IUnknown {
  virtual MTL::Buffer *GetCurrent() = 0;
  virtual void Swap() = 0;
};

DEFINE_COM_INTERFACE("daf21510-d136-44dd-bb16-068a94690775",
                     IMTLSwappableTexture)
    : public IUnknown {
  virtual MTL::Texture *GetCurrent() = 0;
  virtual void Swap() = 0;
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
  TResourceBase(const tag::DESC_S *desc, IMTLD3D11Device *device)
      : MTLD3D11DeviceChild<typename tag::COM, IDXMTResource>(device),
        desc(*desc),
        dxgi_resource(new MTLDXGIResource<TResourceBase<tag>>(this)) {}

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

    if (logQueryInterfaceError(__uuidof(typename tag::COM), riid)) {
      Logger::warn("QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
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
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
                           ID3D11ShaderResourceView **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc,
                            ID3D11UnorderedAccessView **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *desc,
                         ID3D11RenderTargetView **ppView) {
    return E_INVALIDARG;
  };
  virtual HRESULT STDMETHODCALLTYPE
  CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *desc,
                         ID3D11DepthStencilView **ppView) {
    return E_INVALIDARG;
  };

protected:
  tag::DESC_S desc;
  std::unique_ptr<IDXGIResource1> dxgi_resource;
};

} // namespace dxmt