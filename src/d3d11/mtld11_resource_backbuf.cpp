#include "dxmt_names.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "dxgi_interfaces.h"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"
#include "util_error.hpp"
#include "util_string.hpp"

namespace dxmt {

struct tag_texture_backbuffer {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  using COM = IMTLD3D11BackBuffer;
  using DESC = D3D11_TEXTURE2D_DESC;
  using DESC_S = D3D11_TEXTURE2D_DESC;
};

class EmulatedBackBufferTexture : public TResourceBase<tag_texture_backbuffer> {

private:
  HWND hWnd;
  IMTLDXGIDevice *layer_factory;
  void *native_view_;
  /**
  don't use a smart pointer
  it's managed by native_view_
  */
  CA::MetalLayer *layer_;
  Obj<CA::MetalDrawable> current_drawable;

  using BackBufferRTVBase =
      TResourceViewBase<tag_render_target_view<EmulatedBackBufferTexture>>;
  class BackBufferRTV : public BackBufferRTVBase {
  public:
    BackBufferRTV(const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                  EmulatedBackBufferTexture *context, IMTLD3D11Device *pDevice)
        : BackBufferRTVBase(pDesc, context, pDevice) {}

    MTL::PixelFormat GetPixelFormat() final {
      return resource->layer_->pixelFormat();
    };

    MTL::Texture *GetCurrentTexture() final {
      return resource->GetCurrentFrameBackBuffer();
    }
  };
  friend class BackBufferRTV;

  using BackBufferSRVBase =
      TResourceViewBase<tag_shader_resource_view<EmulatedBackBufferTexture>,
                        IMTLBindable>;
  class BackBufferSRV : public BackBufferSRVBase {
  public:
    BackBufferSRV(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                  EmulatedBackBufferTexture *context, IMTLD3D11Device *pDevice)
        : BackBufferSRVBase(pDesc, context, pDevice) {}

    void GetBoundResource(MTL_BIND_RESOURCE *ppResource) {
      (*ppResource).IsTexture = 1;
      (*ppResource).Texture = resource->GetCurrentFrameBackBuffer();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) {
      resource->QueryInterface(riid, ppLogicalResource);
    };
  };

  using BackBufferUAVBase =
      TResourceViewBase<tag_unordered_access_view<EmulatedBackBufferTexture>,
                        IMTLBindable>;
  class BackBufferUAV : public BackBufferUAVBase {
  public:
    BackBufferUAV(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                  EmulatedBackBufferTexture *context, IMTLD3D11Device *pDevice)
        : BackBufferUAVBase(pDesc, context, pDevice) {}

    void GetBoundResource(MTL_BIND_RESOURCE *ppResource) {
      (*ppResource).IsTexture = 1;
      (*ppResource).Texture = resource->GetCurrentFrameBackBuffer();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) {
      resource->QueryInterface(riid, ppLogicalResource);
    };
  };

public:
  EmulatedBackBufferTexture(const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                            IMTLD3D11Device *pDevice, HWND hWnd)
      : TResourceBase<tag_texture_backbuffer>(nullptr, pDevice), hWnd(hWnd) {
    if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&layer_factory)))) {
      throw MTLD3DError("Failed to create CAMetalLayer");
    }
    // this looks weird but we don't need a strong reference
    layer_factory->Release();
    if (FAILED(layer_factory->GetMetalLayerFromHwnd(hWnd, &layer_,
                                                    &native_view_))) {
      throw MTLD3DError("Failed to create CAMetalLayer");
    }
    Com<IMTLDXGIDevice> dxgiDevice = com_cast<IMTLDXGIDevice>(pDevice);
    Com<IMTLDXGIAdatper> adapter;
    if (FAILED(dxgiDevice->GetParent(IID_PPV_ARGS(&adapter)))) {
      throw MTLD3DError("Unknown DXGIAdapter");
    }
    MTL_FORMAT_DESC metal_format;
    if (FAILED(adapter->QueryFormatDesc(pDesc->Format, &metal_format))) {
      throw MTLD3DError(
          str::format("Unsupported swapchain format ", pDesc->Format));
    }
    if (metal_format.PixelFormat == MTL::PixelFormatInvalid) {
      throw MTLD3DError(
          str::format("Unsupported swapchain format ", pDesc->Format));
    }
    layer_->setDevice(pDevice->GetMTLDevice());
    layer_->setPixelFormat(metal_format.PixelFormat);
    layer_->setDrawableSize({(double)pDesc->Width, (double)pDesc->Height});

    desc.ArraySize = 1;
    desc.SampleDesc = {1, 0};
    desc.MipLevels = 1;
    if (pDesc->BufferUsage & DXGI_USAGE_SHADER_INPUT) {
      // can be read
      layer_->setFramebufferOnly(false);
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.Format = pDesc->Format;
    desc.Width = pDesc->Width;
    desc.Height = pDesc->Height;
  }

  ~EmulatedBackBufferTexture() {
    // drop reference if any
    current_drawable = nullptr;
    // unnecessary
    // layer_ = nullptr;
    layer_factory->ReleaseMetalLayer(hWnd, native_view_);
  }

  virtual HRESULT PrivateQueryInterface(REFIID riid,
                                        void **ppvObject) override {
    if (riid == __uuidof(ID3D11Texture2D)) {
      *ppvObject = ref_and_cast<ID3D11Texture2D>(this);
      return S_OK;
    }
    return E_FAIL;
  }

  HRESULT CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                                 ID3D11RenderTargetView **ppView) override {
    D3D11_RENDER_TARGET_VIEW_DESC final;
    if (pDesc) {
      if (pDesc->Format != desc.Format) {
        ERR("CreateRenderTargetView: Try to reinterpret back buffer RTV");
        ERR("previous: ", desc.Format);
        ERR("new: ", pDesc->Format);
        return E_INVALIDARG;
      }
      if (pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D) {
        ERR("CreateRenderTargetView: Back buffer RTV must be a 2d texture");
        return E_INVALIDARG;
      }
      if (pDesc->Texture2D.MipSlice != 0) {
        ERR("CreateRenderTargetView: Back buffer RTV mipslice must be 0");
        return E_INVALIDARG;
      }
    }
    final.Format = desc.Format;
    final.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    final.Texture2D.MipSlice = 0;
    if (!ppView)
      return S_FALSE;

    *ppView = ref(new BackBufferRTV(&final, this, this->m_parent));

    return S_OK;
  }

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                   ID3D11ShaderResourceView **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC final;
    if (pDesc) {
      if (pDesc->Format != desc.Format) {
        ERR("CreateRenderTargetView: Try to reinterpret back buffer SRV");
        ERR("previous: ", desc.Format);
        ERR("new: ", pDesc->Format);
        return E_INVALIDARG;
      }
      if (pDesc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D) {
        ERR("CreateRenderTargetView: Back buffer SRV must be a 2d texture");
        return E_INVALIDARG;
      }
      // TODO: confirm texture from drawable has exactly 1 mipslice
      if (pDesc->Texture2D.MostDetailedMip != 0 ||
          pDesc->Texture2D.MipLevels != 1) {
        ERR("CreateRenderTargetView: Back buffer SRV mipslice must be 0");
        return E_INVALIDARG;
      }
    }
    final.Format = desc.Format;
    final.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    final.Texture2D.MostDetailedMip = 0;
    final.Texture2D.MipLevels = 1;
    if (!ppView)
      return S_FALSE;

    *ppView = ref(new BackBufferSRV(&final, this, this->m_parent));

    return S_OK;
  }

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                            ID3D11UnorderedAccessView **ppView) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC final;
    if (pDesc) {
      if (pDesc->Format != desc.Format) {
        ERR("CreateRenderTargetView: Try to reinterpret back buffer UAV");
        ERR("previous: ", desc.Format);
        ERR("new: ", pDesc->Format);
        return E_INVALIDARG;
      }
      if (pDesc->ViewDimension != D3D11_UAV_DIMENSION_TEXTURE2D) {
        ERR("CreateRenderTargetView: Back buffer UAV must be a 2d texture");
        return E_INVALIDARG;
      }
      // TODO: confirm texture from drawable has exactly 1 mipslice
      if (pDesc->Texture2D.MipSlice != 0) {
        ERR("CreateRenderTargetView: Back buffer UAV mipslice must be 0");
        return E_INVALIDARG;
      }
    }
    final.Format = desc.Format;
    final.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    final.Texture2D.MipSlice = 0;
    if (!ppView)
      return S_FALSE;

    *ppView = ref(new BackBufferUAV(&final, this, this->m_parent));

    return S_OK;
  }

  MTL::Texture *GetCurrentFrameBackBuffer() {
    if (current_drawable == nullptr) {
      current_drawable = layer_->nextDrawable();
      assert(current_drawable != nullptr);
    }
    return current_drawable->texture();
  };

  void Swap() override { current_drawable = nullptr; }

  CA::MetalDrawable *CurrentDrawable() override {
    return current_drawable.ptr();
  }
};

Com<IMTLD3D11BackBuffer>
CreateEmulatedBackBuffer(IMTLD3D11Device *pDevice,
                         const DXGI_SWAP_CHAIN_DESC1 *pDesc, HWND hWnd) {
  return new EmulatedBackBufferTexture(pDesc, pDevice, hWnd);
}

} // namespace dxmt