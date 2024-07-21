#include "d3d11_private.h"
#include "dxmt_binding.hpp"
#include "dxmt_names.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "dxgi_interfaces.h"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"
#include "util_env.hpp"
#include "util_error.hpp"
#include "util_string.hpp"
#include "MetalFX/MTLFXSpatialScaler.hpp"

namespace dxmt {

struct tag_texture_backbuffer {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  using COM = ID3D11Texture2D;
  using DESC = D3D11_TEXTURE2D_DESC;
  using DESC_S = D3D11_TEXTURE2D_DESC;
};

template <bool EnableMetalFX>
class EmulatedBackBufferTexture
    : public TResourceBase<tag_texture_backbuffer, IMTLD3D11BackBuffer,
                           IMTLBindable, BackBufferSource> {

private:
  HWND hWnd;
  IMTLDXGIDevice *layer_factory;
  void *native_view_;
  /**
  don't use a smart pointer
  it's managed by native_view_
  */
  CA::MetalLayer *layer_;
  MTL::PixelFormat pixel_format_;
  Obj<CA::MetalDrawable> current_drawable;
  std::array<Obj<MTL::Texture>, 3> temp_buffer;
  uint32_t buffer_idx = 0;
  Obj<MTLFX::SpatialScaler> metalfx_scaler;
  bool destroyed = false;
  SIMPLE_RESIDENCY_TRACKER tracker{};

  using BackBufferRTVBase = TResourceViewBase<
      tag_render_target_view<EmulatedBackBufferTexture<EnableMetalFX>>>;
  class BackBufferRTV : public BackBufferRTVBase {
    MTL::PixelFormat interpreted_pixel_format;

  public:
    BackBufferRTV(const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                  EmulatedBackBufferTexture<EnableMetalFX> *context,
                  MTL::PixelFormat interpreted_pixel_format,
                  IMTLD3D11Device *pDevice)
        : BackBufferRTVBase(pDesc, context, pDevice),
          interpreted_pixel_format(interpreted_pixel_format) {}
    MTL_RENDER_TARGET_VIEW_DESC props{0, 0, 0, 0}; // FIXME: bugprone

    MTL::PixelFormat GetPixelFormat() final {
      return interpreted_pixel_format;
    };

    BindingRef GetBinding(uint64_t) {
      return BindingRef(static_cast<BackBufferSource *>(this->resource.ptr()));
    };

    MTL_RENDER_TARGET_VIEW_DESC *GetRenderTargetProps() { return &props; };
  };
  friend class BackBufferRTV;

  using BackBufferSRVBase = TResourceViewBase<
      tag_shader_resource_view<EmulatedBackBufferTexture<EnableMetalFX>>,
      IMTLBindable>;
  class BackBufferSRV : public BackBufferSRVBase {
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    BackBufferSRV(const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                  EmulatedBackBufferTexture<EnableMetalFX> *context,
                  IMTLD3D11Device *pDevice)
        : BackBufferSRVBase(pDesc, context, pDevice) {}

    BindingRef UseBindable(uint64_t) override {
      return BindingRef(static_cast<BackBufferSource *>(this->resource.ptr()));
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      return ArgumentData(
          static_cast<BackBufferSource *>(this->resource.ptr()));
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      this->QueryInterface(riid, ppLogicalResource);
    };
  };

  using BackBufferUAVBase = TResourceViewBase<
      tag_unordered_access_view<EmulatedBackBufferTexture<EnableMetalFX>>,
      IMTLBindable>;
  class BackBufferUAV : public BackBufferUAVBase {
    SIMPLE_RESIDENCY_TRACKER tracker{};

  public:
    BackBufferUAV(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                  EmulatedBackBufferTexture<EnableMetalFX> *context,
                  IMTLD3D11Device *pDevice)
        : BackBufferUAVBase(pDesc, context, pDevice) {}

    BindingRef UseBindable(uint64_t) override {
      return BindingRef(static_cast<BackBufferSource *>(this->resource.ptr()));
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      return ArgumentData(
          static_cast<BackBufferSource *>(this->resource.ptr()));
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      this->QueryInterface(riid, ppLogicalResource);
    };
  };

public:
  EmulatedBackBufferTexture(const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                            IMTLD3D11Device *pDevice, HWND hWnd)
      : TResourceBase<tag_texture_backbuffer, IMTLD3D11BackBuffer, IMTLBindable,
                      BackBufferSource>(nullptr, pDevice),
        hWnd(hWnd) {
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
    // FIXME: can't omit it, will cause problem when copying
    // FIXME: this is restricted
    // https://developer.apple.com/documentation/quartzcore/cametallayer/1478155-pixelformat
    // although it never causes problem
    // FIXME: direct to display will never happen
    // if the format is not natively supported by the display
    // (but it's kinda broken on built-in retina display anyway)
    layer_->setPixelFormat(metal_format.PixelFormat);
    layer_->setOpaque(true);
    pixel_format_ = metal_format.PixelFormat;
    if constexpr (EnableMetalFX) {
      layer_->setDrawableSize(
          {(double)pDesc->Width * 2, (double)pDesc->Height * 2});
    } else {
      layer_->setDrawableSize({(double)pDesc->Width, (double)pDesc->Height});
    }
    layer_->setDisplaySyncEnabled(false);
    desc.ArraySize = 1;
    desc.SampleDesc = {1, 0};
    desc.MipLevels = 1;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (pDesc->BufferUsage & DXGI_USAGE_SHADER_INPUT) {
      // can be read
      layer_->setFramebufferOnly(false);
      desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.Format = pDesc->Format;
    desc.Width = pDesc->Width;
    desc.Height = pDesc->Height;

    if constexpr (EnableMetalFX) {
      Obj<MTL::TextureDescriptor> descriptor;
      D3D11_TEXTURE2D_DESC out_desc; // only used as an output
      if (FAILED((CreateMTLTextureDescriptor(pDevice, &desc, &out_desc,
                                             &descriptor)))) {
        throw MTLD3DError(str::format("MTLFX: failed to create texture"));
      }
      for (unsigned i = 0; i < 3; i++) {
        temp_buffer[i] =
            transfer(pDevice->GetMTLDevice()->newTexture(descriptor));
      }
      auto scaler_descriptor =
          transfer(MTLFX::SpatialScalerDescriptor::alloc()->init());
      scaler_descriptor->setInputHeight(desc.Height);
      scaler_descriptor->setInputWidth(desc.Width);
      scaler_descriptor->setOutputHeight(desc.Height * 2);
      scaler_descriptor->setOutputWidth(desc.Width * 2);
      scaler_descriptor->setOutputTextureFormat(layer_->pixelFormat());
      scaler_descriptor->setColorTextureFormat(descriptor->pixelFormat());
      metalfx_scaler = transfer(
          scaler_descriptor->newSpatialScaler(pDevice->GetMTLDevice()));
      D3D11_ASSERT(metalfx_scaler && "otherwise metalfx failed to initialize");
    }
  }

  ~EmulatedBackBufferTexture() { Destroy(); }

  void Destroy() override {
    if (destroyed)
      return;
    // drop reference if any
    current_drawable = nullptr;
    // unnecessary
    // layer_ = nullptr;
    layer_factory->ReleaseMetalLayer(hWnd, native_view_);
    destroyed = true;
  }

  HRESULT CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                                 ID3D11RenderTargetView **ppView) override {
    D3D11_RENDER_TARGET_VIEW_DESC final;
    MTL::PixelFormat interpreted_pixel_format = pixel_format_;
    if (pDesc) {
      MTL_FORMAT_DESC metal_format;
      Com<IMTLDXGIAdatper> adapter;
      m_parent->GetAdapter(&adapter);
      if (FAILED(adapter->QueryFormatDesc(pDesc->Format, &metal_format))) {
        ERR("CreateRenderTargetView: invalid back buffer rtv format ",
            pDesc->Format);
        return E_INVALIDARG;
      }
      interpreted_pixel_format = metal_format.PixelFormat;
      if (pDesc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D) {
        ERR("CreateRenderTargetView: Back buffer RTV must be a 2d texture");
        return E_INVALIDARG;
      }
      if (pDesc->Texture2D.MipSlice != 0) {
        ERR("CreateRenderTargetView: Back buffer RTV mipslice must be 0");
        return E_INVALIDARG;
      }
      final.Format = pDesc->Format;
    } else {
      final.Format = desc.Format;
    }
    final.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    final.Texture2D.MipSlice = 0;
    if (!ppView)
      return S_FALSE;

    *ppView = ref(new BackBufferRTV(&final, this, interpreted_pixel_format,
                                    this->m_parent));

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

  MTL::Texture *GetCurrentFrameBackBuffer() override {
    if (current_drawable == nullptr) {
      current_drawable = layer_->nextDrawable();
      buffer_idx++;
      buffer_idx = buffer_idx % 3;
      D3D11_ASSERT(current_drawable != nullptr);
    }
    if constexpr (EnableMetalFX) {
      return temp_buffer[buffer_idx];
    } else {
      return current_drawable->texture();
    }
  };

  void Present(MTL::CommandBuffer *cmdbuf, double vsync_duration) override {
    auto drawable = current_drawable;
    if (drawable) {
      if constexpr (EnableMetalFX) {
        D3D11_ASSERT(metalfx_scaler);
        metalfx_scaler->setColorTexture(temp_buffer[buffer_idx]);
        metalfx_scaler->setOutputTexture(drawable->texture());
        metalfx_scaler->encodeToCommandBuffer(cmdbuf);
      }
      cmdbuf->presentDrawableAfterMinimumDuration(drawable, vsync_duration);
      current_drawable = nullptr; // "swap"
    }
  }

  BindingRef UseBindable(uint64_t) override {
    return BindingRef(static_cast<BackBufferSource *>(this));
  };

  ArgumentData GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
    *ppTracker = &tracker;
    return ArgumentData(static_cast<BackBufferSource *>(this));
  };

  void GetLogicalResourceOrView(REFIID riid,
                                void **ppLogicalResource) override {
    QueryInterface(riid, ppLogicalResource);
  };

  bool GetContentionState(uint64_t finishedSeqId) override { return true; }
};

Com<IMTLD3D11BackBuffer>
CreateEmulatedBackBuffer(IMTLD3D11Device *pDevice,
                         const DXGI_SWAP_CHAIN_DESC1 *pDesc, HWND hWnd) {
  if (env::getEnvVar("DXMT_METALFX_SPATIAL_SWAPCHAIN") == "1") {
    return new EmulatedBackBufferTexture<true>(pDesc, pDevice, hWnd);
  }
  return new EmulatedBackBufferTexture<false>(pDesc, pDevice, hWnd);
}

} // namespace dxmt