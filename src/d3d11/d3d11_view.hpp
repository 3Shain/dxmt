#pragma once
#include "Metal/MTLPixelFormat.hpp"
#include "d3d11_private.h"
#include "d3d11_device_child.h"
#include "Metal/MTLRenderPass.hpp"
#include "mtld11_resource.hpp"
#include "../dxmt/dxmt_binding.hpp"

MIDL_INTERFACE("f1d21087-fbde-44b3-bc2c-b69be540a0ad")
IMTLD3D11RenderTargetView : public ID3D11RenderTargetView {
  virtual dxmt::RenderTargetBinding *STDMETHODCALLTYPE Pin() = 0;
  virtual MTL::PixelFormat GetPixelFormat() = 0;
};
__CRT_UUID_DECL(IMTLD3D11RenderTargetView, 0xf1d21087, 0xfbde, 0x44b3, 0xbc,
                0x2c, 0xb6, 0x9b, 0xe5, 0x40, 0xa0, 0xad);

MIDL_INTERFACE("42e48164-8733-422b-8421-4c57229641f9")
IMTLD3D11DepthStencilView : public ID3D11DepthStencilView {
  virtual dxmt::DepthStencilBinding *Pin(dxmt::DXMTCommandStream* cs) = 0;
  virtual MTL::PixelFormat GetPixelFormat() = 0;
};
__CRT_UUID_DECL(IMTLD3D11DepthStencilView, 0x42e48164, 0x8733, 0x422b, 0x84,
                0x21, 0x4c, 0x57, 0x22, 0x96, 0x41, 0xf9);

MIDL_INTERFACE("a8f906f1-137a-49a6-b9fa-3f89ef52e3eb")
IMTLD3D11UnorderedAccessView : public ID3D11UnorderedAccessView {
  virtual void STDMETHODCALLTYPE BindToArgumentBuffer(
      MTL::ArgumentEncoder * encoder, UINT index) = 0;
};
__CRT_UUID_DECL(IMTLD3D11UnorderedAccessView, 0xa8f906f1, 0x137a, 0x49a6, 0xb9,
                0xfa, 0x3f, 0x89, 0xef, 0x52, 0xe3, 0xeb);

MIDL_INTERFACE("acfa8c6e-699a-4607-b229-a55532dde5fd")
IMTLD3D11ShaderResourceView : public ID3D11ShaderResourceView {
  virtual dxmt::ShaderResourceBinding *STDMETHODCALLTYPE Pin(
      dxmt::DXMTCommandStream * cs) = 0;
};
__CRT_UUID_DECL(IMTLD3D11ShaderResourceView, 0xacfa8c6e, 0x699a, 0x4607, 0xb2,
                0x29, 0xa5, 0x55, 0x32, 0xdd, 0xe5, 0xfd);

namespace dxmt {

template <typename ResImpl, typename ViewImpl>
class TMTLD3D11ShaderResourceView
    : public MTLD3D11DeviceChild<IMTLD3D11ShaderResourceView> {
public:
  TMTLD3D11ShaderResourceView(IMTLD3D11Device *device,
                              typename ResImpl::Interface *resource,
                              const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
                              ViewImpl data)
      : MTLD3D11DeviceChild<IMTLD3D11ShaderResourceView>(device),
        resource_(resource), desc_(*desc), data_(data){};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) ||
        riid == __uuidof(ID3D11ShaderResourceView) ||
        riid == __uuidof(IMTLD3D11ShaderResourceView)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11ShaderResourceView), riid)) {
      Logger::warn(
          "D3D11ShaderResourceView::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  };

  void STDMETHODCALLTYPE GetResource(ID3D11Resource **ppResource) final {
    *ppResource = resource_.ref();
  }

  void STDMETHODCALLTYPE GetDesc(D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc) final {
    pDesc->Format = desc_.Format;
    pDesc->ViewDimension = desc_.ViewDimension;
  }

  ShaderResourceBinding *Pin(DXMTCommandStream *cs) final;

  Com<typename ResImpl::Interface> resource_;
  D3D11_SHADER_RESOURCE_VIEW_DESC desc_;
  ViewImpl data_;
};

template <typename ResImpl, typename ViewImpl>
class TMTLD3D11RenderTargetView
    : public MTLD3D11DeviceChild<IMTLD3D11RenderTargetView> {
public:
  TMTLD3D11RenderTargetView(IMTLD3D11Device *pDevice,
                            typename ResImpl::Interface *pResource,
                            const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                            ViewImpl data)
      : MTLD3D11DeviceChild<IMTLD3D11RenderTargetView>(pDevice),
        resource_(pResource), desc_(*pDesc), data_(data){};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) ||
        riid == __uuidof(ID3D11RenderTargetView) ||
        riid == __uuidof(IMTLD3D11RenderTargetView)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IMTLD3D11RenderTargetView), riid)) {
      WARN("Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  };

  void STDMETHODCALLTYPE GetResource(ID3D11Resource **ppResource) final {
    *ppResource = resource_.ref();
  };

  void STDMETHODCALLTYPE GetDesc(D3D11_RENDER_TARGET_VIEW_DESC *pDesc) final {
    pDesc->Format = desc_.Format;
    pDesc->ViewDimension = desc_.ViewDimension;

    switch (desc_.ViewDimension) {
    case D3D11_RTV_DIMENSION_UNKNOWN:
      break;

    case D3D11_RTV_DIMENSION_BUFFER:
      pDesc->Buffer = desc_.Buffer;
      break;

    case D3D11_RTV_DIMENSION_TEXTURE1D:
      pDesc->Texture1D = desc_.Texture1D;
      break;

    case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
      pDesc->Texture1DArray = desc_.Texture1DArray;
      break;

    case D3D11_RTV_DIMENSION_TEXTURE2D:
      pDesc->Texture2D.MipSlice = desc_.Texture2D.MipSlice;
      break;

    case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
      pDesc->Texture2DArray.MipSlice = desc_.Texture2DArray.MipSlice;
      pDesc->Texture2DArray.FirstArraySlice =
          desc_.Texture2DArray.FirstArraySlice;
      pDesc->Texture2DArray.ArraySize = desc_.Texture2DArray.ArraySize;
      break;

    case D3D11_RTV_DIMENSION_TEXTURE2DMS:
      pDesc->Texture2DMS = desc_.Texture2DMS;
      break;

    case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
      pDesc->Texture2DMSArray = desc_.Texture2DMSArray;
      break;

    case D3D11_RTV_DIMENSION_TEXTURE3D:
      pDesc->Texture3D = desc_.Texture3D;
      break;
    }
  };

  RenderTargetBinding *Pin() final;

  MTL::PixelFormat GetPixelFormat() final;

private:
  Com<typename ResImpl::Interface> resource_;
  D3D11_RENDER_TARGET_VIEW_DESC desc_;
  ViewImpl data_;
};

template <typename ResImpl, typename ViewImpl>
class TMTLD3D11DepthStencilView
    : public MTLD3D11DeviceChild<IMTLD3D11DepthStencilView> {
public:
  TMTLD3D11DepthStencilView(IMTLD3D11Device *pDevice,
                            typename ResImpl::Interface *pResource,
                            const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                            ViewImpl data)
      : MTLD3D11DeviceChild<IMTLD3D11DepthStencilView>(pDevice),
        resource_(pResource), desc_(*pDesc), data_(data){};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) ||
        riid == __uuidof(ID3D11DepthStencilView) ||
        riid == __uuidof(IMTLD3D11DepthStencilView)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IMTLD3D11DepthStencilView), riid)) {
      WARN("Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  };

  void STDMETHODCALLTYPE GetResource(ID3D11Resource **ppResource) final {
    *ppResource = resource_.ref();
  };

  void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc) final {
    pDesc->Format = desc_.Format;
    pDesc->ViewDimension = desc_.ViewDimension;

    switch (desc_.ViewDimension) {
    case D3D11_DSV_DIMENSION_UNKNOWN:
      break;
    case D3D11_DSV_DIMENSION_TEXTURE1D:
      pDesc->Texture1D = desc_.Texture1D;
      break;

    case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
      pDesc->Texture1DArray = desc_.Texture1DArray;
      break;

    case D3D11_DSV_DIMENSION_TEXTURE2D:
      pDesc->Texture2D.MipSlice = desc_.Texture2D.MipSlice;
      break;

    case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
      pDesc->Texture2DArray.MipSlice = desc_.Texture2DArray.MipSlice;
      pDesc->Texture2DArray.FirstArraySlice =
          desc_.Texture2DArray.FirstArraySlice;
      pDesc->Texture2DArray.ArraySize = desc_.Texture2DArray.ArraySize;
      break;

    case D3D11_DSV_DIMENSION_TEXTURE2DMS:
      pDesc->Texture2DMS = desc_.Texture2DMS;
      break;

    case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
      pDesc->Texture2DMSArray = desc_.Texture2DMSArray;
      break;
    };
  }

  DepthStencilBinding *Pin(DXMTCommandStream* cs) final;

  MTL::PixelFormat GetPixelFormat() final;

private:
  Com<typename ResImpl::Interface> resource_;
  D3D11_DEPTH_STENCIL_VIEW_DESC desc_;
  ViewImpl data_;
};

} // namespace dxmt