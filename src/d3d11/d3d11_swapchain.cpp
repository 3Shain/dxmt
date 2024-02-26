#include "d3d11_swapchain.hpp"
#include "../dxgi/dxgi_format.hpp"
#include "Foundation/NSAutoreleasePool.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_context.hpp"
#include "../util/wsi_window.h"
#include "d3d11_device.hpp"

#include "objc-wrapper/dispatch.h"
#include <cassert>

namespace dxmt {

class EmulatedSwapChain {
public:
  EmulatedSwapChain(HWND hWnd, IDXGIMetalLayerFactory *layer_factory)
      : layer_factory_(layer_factory), window_handle_(hWnd) {
    if (unlikely(FAILED(layer_factory->GetMetalLayerFromHwnd(hWnd, &layer_,
                                                             &native_view_)))) {
      throw MTLD3DError("Unknown window handle");
    }
  };

  ~EmulatedSwapChain() {
    {
      current_drawable = nullptr;
      layer_factory_->ReleaseMetalLayer(window_handle_, native_view_);
    }
  }

  MTL::Texture *GetCurrentFrameBackBuffer() {
    if (current_drawable == nullptr) {
      current_drawable = layer_->nextDrawable();
      assert(current_drawable != nullptr);
    }
    return current_drawable->texture();
  };

  void Swap() { current_drawable = nullptr; }

  CA::MetalDrawable *CurrentDrawable() { return current_drawable.ptr(); }

  HWND hWnd() { return window_handle_; }

  CA::MetalLayer *layer() { return layer_.ptr(); }

private:
  Obj<CA::MetalLayer> layer_;
  Obj<CA::MetalDrawable> current_drawable;
  Com<IDXGIMetalLayerFactory> layer_factory_;
  HWND window_handle_;
  void *native_view_;
};

class __SwapChainTexture {
public:
  static const D3D11_RESOURCE_DIMENSION Dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  static const D3D11_USAGE Usage = D3D11_USAGE_DEFAULT;
  typedef D3D11_TEXTURE2D_DESC Description;
  typedef ID3D11Texture2D Interface;
  typedef EmulatedSwapChain *Data;

  __SwapChainTexture(IMTLD3D11Device *pDevice, Data data,
                     const Description *pDesc,
                     const D3D11_SUBRESOURCE_DATA *pData)
      : swapchain(data) {}

  Data swapchain;
  IMTLD3D11Device *__device__;
  Interface *__resource__;

  HRESULT CreateRenderTargetView(const D3D11_RENDER_TARGET_VIEW_DESC *desc,
                                 ID3D11RenderTargetView **ppvRTV) {
    if (desc) {
      if (desc->ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D &&
          desc->ViewDimension != D3D11_RTV_DIMENSION_UNKNOWN) {
        ERR("Try to create a non 2D texture RTV from swap chain buffer");
        return E_INVALIDARG;
      }
    }

    D3D11_RENDER_TARGET_VIEW_DESC true_desc;
    true_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    true_desc.Format = DXGI_FORMAT_UNKNOWN; // FIXME: sync with metal texture
    true_desc.Texture2D.MipSlice = 0;       // must be 0

    // FIXME: format check?

    *ppvRTV = ref(
        new TMTLD3D11RenderTargetView<__SwapChainTexture, EmulatedSwapChain *>(
            __device__, __resource__, desc == nullptr ? &true_desc : desc,
            swapchain));
    return S_OK;
  }
};

using SwapChainRenderTargetView =
    TMTLD3D11RenderTargetView<__SwapChainTexture, EmulatedSwapChain *>;

template <> MTL::PixelFormat SwapChainRenderTargetView::GetPixelFormat() {
  return data_->layer()->pixelFormat();
}

template <> RenderTargetBinding *SwapChainRenderTargetView::Pin() {
  return new SimpleLazyTextureRenderTargetBinding(
      [this]() { return (this->data_->GetCurrentFrameBackBuffer()); },
      desc_.Texture2D.MipSlice, 0);
}

class MTLD3D11SwapChain final : public MTLDXGISubObject<IDXGISwapChain1> {
public:
  MTLD3D11SwapChain(IDXGIFactory1 *pFactory, IMTLDXGIDevice *pDevice, HWND hWnd,
                    IDXGIMetalLayerFactory *pMetalLayerFactory,
                    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc)
      : MTLDXGISubObject(pDevice), swapchain_(hWnd, pMetalLayerFactory),
        factory_(pFactory), presentation_count_(0), desc_(*pDesc),
        fullscreen_desc_(*pFullscreenDesc) {

    swapchain_.layer()->setAllowsNextDrawableTimeout(false);

    D3D11_TEXTURE2D_DESC desc;
    desc.SampleDesc = {.Count = 1, .Quality = 0};
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Format = pDesc->Format;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.MipLevels = 1;
    desc.Height = pDesc->Height;
    desc.Width = pDesc->Width;

    device_ = Com<IMTLD3D11Device>::queryFrom(pDevice);

    // https://developer.apple.com/documentation/quartzcore/cametallayer/1478163-device
    // you must set the device for a layer before rendering
    swapchain_.layer()->setDevice(device_->GetMTLDevice());

    auto metal_format = g_metal_format_map[desc.Format];
    // FIXME: actually render target format is very limited
    if (metal_format.pixel_format == MTL::PixelFormatInvalid) {
      throw MTLD3DError("Unsupported pixel format");
    }

    swapchain_.layer()->setDrawableSize(
        {.width = (double)desc.Width, .height = (double)desc.Height});
    swapchain_.layer()->setPixelFormat(metal_format.pixel_format);
    buffer_delegate_ = new D3D11Resource<__SwapChainTexture>(
        device_.ptr(), &swapchain_, &desc, NULL);

    Com<ID3D11DeviceContext1> context;
    device_->GetImmediateContext1(&context);
    context->QueryInterface(IID_PPV_ARGS(&device_context_));

    semaphore = dispatch_semaphore_create(3);
  };

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDXGISwapChain1), riid)) {
      WARN("Unknown interface query", str::format(riid));
    }

    return E_NOINTERFACE;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetParent(REFIID riid, void **parent) final {
    return factory_->QueryInterface(riid, parent);
  };

  HRESULT
  STDMETHODCALLTYPE
  Present(UINT sync_interval, UINT flags) final {
    return Present1(sync_interval, flags, nullptr);
  };

  HRESULT
  STDMETHODCALLTYPE
  GetBuffer(UINT buffer_idx, REFIID riid, void **surface) final {
    if (buffer_idx == 0) {
      return buffer_delegate_->QueryInterface(riid, surface);
    } else {
      ERR("Non zero-index buffer is not supported");
      return E_FAIL;
    }
  };

  HRESULT
  STDMETHODCALLTYPE
  SetFullscreenState(BOOL fullscreen, IDXGIOutput *target) final {
    IMPLEMENT_ME;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetFullscreenState(BOOL *fullscreen, IDXGIOutput **target) final {
    IMPLEMENT_ME;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc) final {
    if (!pDesc)
      return E_INVALIDARG;

    pDesc->BufferDesc.Width = desc_.Width;
    pDesc->BufferDesc.Height = desc_.Height;
    pDesc->BufferDesc.RefreshRate = fullscreen_desc_.RefreshRate;
    pDesc->BufferDesc.Format = desc_.Format;
    pDesc->BufferDesc.ScanlineOrdering = fullscreen_desc_.ScanlineOrdering;
    pDesc->BufferDesc.Scaling = fullscreen_desc_.Scaling;
    pDesc->SampleDesc = desc_.SampleDesc;
    pDesc->BufferUsage = desc_.BufferUsage;
    pDesc->BufferCount = desc_.BufferCount;
    pDesc->OutputWindow = swapchain_.hWnd();
    pDesc->Windowed = fullscreen_desc_.Windowed;
    pDesc->SwapEffect = desc_.SwapEffect;
    pDesc->Flags = desc_.Flags;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  ResizeBuffers(UINT buffer_count, UINT width, UINT height, DXGI_FORMAT format,
                UINT flags) final {
    if (width == 0 && height == 0) {
      wsi::getWindowSize(swapchain_.hWnd(), &desc_.Width, &desc_.Height);
    } else {
      desc_.Width = width;
      desc_.Height = height;
    }
    swapchain_.layer()->setDrawableSize(
        {.width = (double)desc_.Width, .height = (double)desc_.Height});
    if (format == DXGI_FORMAT_UNKNOWN) {
      // TODO: NOP
    } else {
      auto metal_format = g_metal_format_map[format];
      if (metal_format.pixel_format == MTL::PixelFormatInvalid) {
        return E_FAIL;
      }
      swapchain_.layer()->setPixelFormat(metal_format.pixel_format);
    }
    D3D11_TEXTURE2D_DESC desc;
    buffer_delegate_->GetDesc(&desc);
    desc.Width = desc_.Width;
    desc.Height = desc_.Height;
    desc.Format = format;
    buffer_delegate_ = new D3D11Resource<__SwapChainTexture>(
        device_.ptr(), &swapchain_, &desc, NULL);
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  ResizeTarget(const DXGI_MODE_DESC *target_mode_desc) final {

    return ResizeBuffers(1 /* FIXME: */, target_mode_desc->Width,
                         target_mode_desc->Height, target_mode_desc->Format, 0);
  };

  HRESULT
  STDMETHODCALLTYPE
  GetContainingOutput(IDXGIOutput **output) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  GetFrameStatistics(DXGI_FRAME_STATISTICS *stats) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  GetLastPresentCount(UINT *last_present_count) final {
    if (last_present_count == NULL) {
      return E_POINTER;
    }
    *last_present_count = presentation_count_;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) final {
    if (pDesc == NULL) {
      return E_POINTER;
    }
    *pDesc = desc_;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) final {
    if (pDesc == NULL) {
      return E_POINTER;
    }
    *pDesc = fullscreen_desc_;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetHwnd(HWND *pHwnd) final {
    if (pHwnd == NULL) {
      return E_POINTER;
    }
    *pHwnd = swapchain_.hWnd();
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetCoreWindow(REFIID refiid, void **ppUnk) final {
    ERR("Not implemented");
    return E_NOTIMPL;
  };

  HRESULT
  STDMETHODCALLTYPE
  Present1(UINT SyncInterval, UINT PresentFlags,
           const DXGI_PRESENT_PARAMETERS *pPresentParameters) final {

    // dispatch_semaphore_wait(semaphore, (~0ull));

    auto w = transfer(NS::AutoreleasePool::alloc()->init());

    device_context_->Flush2([&swapchain_ = swapchain_, semaphore = semaphore,
                             presentation_count_ = presentation_count_](
                                MTL::CommandBuffer *cbuffer) {
      // swapchain_.GetCurrentFrameBackBuffer(); // ensure not null
      cbuffer->presentDrawable(swapchain_.CurrentDrawable());
      cbuffer->addCompletedHandler(
          [presentation_count_](void *cbuffer) {
            unix_printf("A frame end. pc: %d\n", presentation_count_);
            // dispatch_semaphore_signal(semaphore);
          });
          // assert(0);
    });
    swapchain_.Swap();

    presentation_count_ += 1;

    return S_OK;
  };

  BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() final { return FALSE; };

  HRESULT
  STDMETHODCALLTYPE
  GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  SetBackgroundColor(const DXGI_RGBA *pColor) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  GetBackgroundColor(DXGI_RGBA *pColor) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  SetRotation(DXGI_MODE_ROTATION Rotation) final { IMPLEMENT_ME; };

  HRESULT
  STDMETHODCALLTYPE
  GetRotation(DXGI_MODE_ROTATION *pRotation) final { IMPLEMENT_ME; };

private:
  Com<ID3D11Texture2D> buffer_delegate_;
  EmulatedSwapChain swapchain_;
  Com<IDXGIFactory1> factory_;
  ULONG presentation_count_;
  DXGI_SWAP_CHAIN_DESC1 desc_;
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc_;
  Com<IMTLD3D11Device> device_;
  Com<IMTLD3D11DeviceContext> device_context_;
  dispatch_semaphore_t semaphore;
};

HRESULT CreateSwapChain(IDXGIFactory1 *pFactory, IMTLDXGIDevice *pDevice,
                        HWND hWnd, IDXGIMetalLayerFactory *pMetalLayerFactory,
                        const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
                        IDXGISwapChain1 **ppSwapChain) {
  if (ppSwapChain == NULL) {
    return E_POINTER;
  }
  InitReturnPtr(ppSwapChain);
  try {
    *ppSwapChain = ref(new MTLD3D11SwapChain(
        pFactory, pDevice, hWnd, pMetalLayerFactory, pDesc, pFullscreenDesc));
    return S_OK;
  } catch (MTLD3DError &err) {
    ERR(err.message());
    return E_FAIL;
  }
};

} // namespace dxmt