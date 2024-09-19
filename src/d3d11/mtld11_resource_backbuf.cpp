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

MTL::PixelFormat rgb_to_bgr(MTL::PixelFormat format) {
  switch (format) {
  case MTL::PixelFormatRGBA8Unorm:
    return MTL::PixelFormatBGRA8Unorm;
  case MTL::PixelFormatRGBA8Unorm_sRGB:
    return MTL::PixelFormatBGRA8Unorm_sRGB;
  default:
    return format;
  }
}

struct tag_texture_backbuffer {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  using COM = ID3D11Texture2D;
  using COM_IMPL = ID3D11Texture2D1;
  using DESC = D3D11_TEXTURE2D_DESC;
  using DESC1 = D3D11_TEXTURE2D_DESC1;
  static constexpr std::string_view debug_name = "backbuffer";
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
  Obj<MTL::Texture> current_drawable_srgb_view;
  std::conditional<EnableMetalFX, std::array<Obj<MTL::Texture>, 3>,
                   std::monostate>::type temp_buffer;
  std::conditional<EnableMetalFX, std::array<Obj<MTL::Texture>, 3>,
                   std::monostate>::type temp_buffer_srgb_view;
  Obj<MTLFX::SpatialScaler> metalfx_scaler;
  uint8_t buffer_idx = 0;
  bool destroyed : 1 = false;
  bool rgb_swizzle_fix : 1 = false;
  SIMPLE_RESIDENCY_TRACKER tracker{};

  using BackBufferRTVBase = TResourceViewBase<
      tag_render_target_view<EmulatedBackBufferTexture<EnableMetalFX>>>;
  class BackBufferRTV : public BackBufferRTVBase {
    MTL::PixelFormat interpreted_pixel_format;

  public:
    BackBufferRTV(const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc,
                  EmulatedBackBufferTexture<EnableMetalFX> *context,
                  MTL::PixelFormat interpreted_pixel_format,
                  IMTLD3D11Device *pDevice)
        : BackBufferRTVBase(pDesc, context, pDevice),
          interpreted_pixel_format(interpreted_pixel_format) {}
    MTL_RENDER_TARGET_VIEW_DESC props{0, 0, 0, 0}; // FIXME: bugprone

    MTL::PixelFormat GetPixelFormat() final {
      return this->resource->rgb_swizzle_fix
                 ? rgb_to_bgr(interpreted_pixel_format)
                 : interpreted_pixel_format;
    };

    BindingRef GetBinding(uint64_t) {
      return BindingRef(static_cast<BackBufferSource *>(this->resource.ptr()),
                        Is_sRGBVariant(interpreted_pixel_format));
    };

    MTL_RENDER_TARGET_VIEW_DESC *GetRenderTargetProps() { return &props; };
  };
  friend class BackBufferRTV;

  using BackBufferSRVBase = TResourceViewBase<
      tag_shader_resource_view<EmulatedBackBufferTexture<EnableMetalFX>>,
      IMTLBindable>;
  class BackBufferSRV : public BackBufferSRVBase {
    SIMPLE_RESIDENCY_TRACKER tracker{};
    bool srgb;

  public:
    BackBufferSRV(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
                  EmulatedBackBufferTexture<EnableMetalFX> *context,
                  IMTLD3D11Device *pDevice, bool srgb)
        : BackBufferSRVBase(pDesc, context, pDevice), srgb(srgb) {}

    BindingRef UseBindable(uint64_t) override {
      return BindingRef(static_cast<BackBufferSource *>(this->resource.ptr()),
                        srgb);
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      return ArgumentData(static_cast<BackBufferSource *>(this->resource.ptr()),
                          srgb);
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
    bool srgb;

  public:
    BackBufferUAV(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc,
                  EmulatedBackBufferTexture<EnableMetalFX> *context,
                  IMTLD3D11Device *pDevice, bool srgb)
        : BackBufferUAVBase(pDesc, context, pDevice), srgb(srgb) {}

    BindingRef UseBindable(uint64_t) override {
      return BindingRef(static_cast<BackBufferSource *>(this->resource.ptr()),
                        srgb);
    };

    ArgumentData
    GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
      *ppTracker = &tracker;
      return ArgumentData(static_cast<BackBufferSource *>(this->resource.ptr()),
                          srgb);
    };

    void GetLogicalResourceOrView(REFIID riid,
                                  void **ppLogicalResource) override {
      this->QueryInterface(riid, ppLogicalResource);
    };

    virtual uint64_t SwapCounter(uint64_t handle) override { return handle; };
  };

public:
  EmulatedBackBufferTexture(const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                            IMTLD3D11Device *pDevice, HWND hWnd)
      : TResourceBase<tag_texture_backbuffer, IMTLD3D11BackBuffer, IMTLBindable,
                      BackBufferSource>((tag_texture_backbuffer::DESC1{}),
                                        pDevice),
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
    Com<IMTLDXGIAdatper> adapter;
    if (FAILED(layer_factory->GetParent(IID_PPV_ARGS(&adapter)))) {
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
    switch (metal_format.PixelFormat) {
    case MTL::PixelFormatBGRA8Unorm:
    case MTL::PixelFormatBGRA8Unorm_sRGB: {
      layer_->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
      break;
    }
    case MTL::PixelFormatRGBA8Unorm:
    case MTL::PixelFormatRGBA8Unorm_sRGB: {
      // rgb_swizzle_fix = true;
      // layer_->setPixelFormat(MTL::PixelFormatBGRA8Unorm_sRGB);

      // FIXME: it's emulated, forbids Direct-to-Display
      layer_->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
      break;
    }
    case MTL::PixelFormatRGB10A2Unorm:
    case MTL::PixelFormatRGBA16Float: {
      layer_->setPixelFormat(metal_format.PixelFormat);
      break;
    }
    default:
      throw MTLD3DError(
          str::format("Unsupported swapchain format ", pDesc->Format));
    }
    layer_->setOpaque(true);
    pixel_format_ = metal_format.PixelFormat;
    if constexpr (EnableMetalFX) {
      auto scale_factor = std::max(
          adapter->GetConfigFloat("d3d11.metalSpatialUpscaleFactor", 2), 1.0f);
      layer_->setDrawableSize({(double)pDesc->Width * scale_factor,
                               (double)pDesc->Height * scale_factor});
    } else {
      layer_->setDrawableSize({(double)pDesc->Width, (double)pDesc->Height});
    }
    layer_->setDisplaySyncEnabled(false);
    desc.ArraySize = 1;
    desc.SampleDesc = {1, 0};
    desc.MipLevels = 1;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    /**
    FIXME: a tweak for validation layer
    - framebufferOnly forbids texture view
    - framebufferOnly forbids copy as source
     */
    layer_->setFramebufferOnly(false);
    if (pDesc->BufferUsage & DXGI_USAGE_SHADER_INPUT) {
      // can be read
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
      D3D11_TEXTURE2D_DESC1 out_desc; // only used as an output
      if (FAILED((CreateMTLTextureDescriptor(pDevice, &desc, &out_desc,
                                             &descriptor)))) {
        throw MTLD3DError(str::format("MTLFX: failed to create texture"));
      }
      descriptor->setPixelFormat(layer_->pixelFormat());
      descriptor->setStorageMode(MTL::StorageModePrivate);
      for (unsigned i = 0; i < 3; i++) {
        temp_buffer[i] =
            transfer(pDevice->GetMTLDevice()->newTexture(descriptor));
      }
      for (unsigned i = 0; i < 3; i++) {
        temp_buffer_srgb_view[i] = transfer(
            temp_buffer[i]->newTextureView(Recall_sRGB(layer_->pixelFormat())));
      }
      auto scaler_descriptor =
          transfer(MTLFX::SpatialScalerDescriptor::alloc()->init());
      scaler_descriptor->setInputHeight(desc.Height);
      scaler_descriptor->setInputWidth(desc.Width);
      scaler_descriptor->setOutputHeight(layer_->drawableSize().height);
      scaler_descriptor->setOutputWidth(layer_->drawableSize().width);
      scaler_descriptor->setOutputTextureFormat(layer_->pixelFormat());
      scaler_descriptor->setColorTextureFormat(descriptor->pixelFormat());
      scaler_descriptor->setColorProcessingMode(
          MTLFX::SpatialScalerColorProcessingModeLinear);
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
    current_drawable_srgb_view = nullptr;
    current_drawable = nullptr;
    // unnecessary
    // layer_ = nullptr;
    layer_factory->ReleaseMetalLayer(hWnd, native_view_);
    destroyed = true;
  }

  HRESULT CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc,
                                 ID3D11RenderTargetView1 **ppView) override {
    D3D11_RENDER_TARGET_VIEW_DESC1 final;
    MTL::PixelFormat interpreted_pixel_format = pixel_format_;
    if (pDesc) {
      MTL_FORMAT_DESC metal_format;
      Com<IMTLDXGIAdatper> adapter;
      m_parent->GetAdapter(&adapter);
      final.Format = pDesc->Format;
      if (pDesc && pDesc->Format == DXGI_FORMAT_UNKNOWN) {
        final.Format = desc.Format;
      }
      if (FAILED(adapter->QueryFormatDesc(final.Format, &metal_format))) {
        ERR("CreateRenderTargetView: invalid back buffer rtv format ",
            pDesc->Format);
        return E_INVALIDARG;
      }
      interpreted_pixel_format = metal_format.PixelFormat;
      if (Forget_sRGB(interpreted_pixel_format) != Forget_sRGB(pixel_format_)) {
        ERR("CreateRenderTargetView: Back buffer RTV format is not "
            "compatible");
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
    final.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    final.Texture2D.MipSlice = 0;
    final.Texture2D.PlaneSlice = 0;
    if (!ppView)
      return S_FALSE;

    *ppView = ref(new BackBufferRTV(&final, this, interpreted_pixel_format,
                                    this->m_parent));

    return S_OK;
  }

  HRESULT
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
                           ID3D11ShaderResourceView1 **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC1 final;
    if (pDesc) {
      if (pDesc->Format == DXGI_FORMAT_UNKNOWN) {
        final.Format = desc.Format;
      } else {
        final.Format = pDesc->Format;
      }
      if (final.Format != desc.Format) {
        ERR("CreateRenderTargetView: Try to reinterpret back buffer SRV");
        ERR("previous: ", desc.Format);
        ERR("new: ", final.Format);
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
    } else {
      final.Format = desc.Format;
    }
    final.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    final.Texture2D.MostDetailedMip = 0;
    final.Texture2D.MipLevels = 1;
    final.Texture2D.PlaneSlice = 0;
    if (!ppView)
      return S_FALSE;

    *ppView = ref(new BackBufferSRV(&final, this, this->m_parent,
                                    Is_sRGBVariant(pixel_format_)));

    return S_OK;
  }

  HRESULT
  CreateUnorderedAccessView(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc,
                            ID3D11UnorderedAccessView1 **ppView) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 final;
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
    final.Texture2D.PlaneSlice = 0;
    if (!ppView)
      return S_FALSE;

    *ppView = ref(new BackBufferUAV(&final, this, this->m_parent,
                                    Is_sRGBVariant(pixel_format_)));

    return S_OK;
  }

  MTL::Texture *GetCurrentFrameBackBuffer(bool srgb) override {
    if (current_drawable == nullptr) {
      current_drawable = layer_->nextDrawable();
      buffer_idx++;
      buffer_idx = buffer_idx % 3;
      D3D11_ASSERT(current_drawable != nullptr);
    }
    if (srgb) {
      if constexpr (EnableMetalFX) {
        return temp_buffer_srgb_view[buffer_idx];
      } else {
        if (!current_drawable_srgb_view) {
          current_drawable_srgb_view =
              transfer(current_drawable->texture()->newTextureView(
                  Recall_sRGB(current_drawable->texture()->pixelFormat())));
        }
        return current_drawable_srgb_view.ptr();
      }
    } else {
      if constexpr (EnableMetalFX) {
        return temp_buffer[buffer_idx];
      } else {
        return current_drawable->texture();
      }
    }
  };

  void Present(MTL::CommandBuffer *cmdbuf, double vsync_duration) override {
    auto drawable = current_drawable;
    if (drawable) {
      if constexpr (EnableMetalFX) {
        D3D11_ASSERT(metalfx_scaler);
        metalfx_scaler->setColorTexture(temp_buffer[buffer_idx]);
        metalfx_scaler->setOutputTexture(current_drawable->texture());
        metalfx_scaler->encodeToCommandBuffer(cmdbuf);
      }
      if (vsync_duration > 0) {
        cmdbuf->presentDrawableAfterMinimumDuration(drawable, vsync_duration);
      } else {
        cmdbuf->presentDrawable(drawable);
      }
      current_drawable_srgb_view = nullptr;
      current_drawable = nullptr; // "swap"
    }
  }

  BindingRef UseBindable(uint64_t) override {
    return BindingRef(static_cast<BackBufferSource *>(this),
                      Is_sRGBVariant(pixel_format_));
  };

  ArgumentData GetArgumentData(SIMPLE_RESIDENCY_TRACKER **ppTracker) override {
    *ppTracker = &tracker;
    return ArgumentData(static_cast<BackBufferSource *>(this),
                        Is_sRGBVariant(pixel_format_));
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