#include "d3d11_swapchain.hpp"
#include "MetalFX/MTLFXSpatialScaler.hpp"
#include "QuartzCore/CADeveloperHUDProperties.hpp"
#include "com/com_guid.hpp"
#include "config/config.hpp"
#include "d3d11_private.h"
#include "dxgi_interfaces.h"
#include "dxgi_object.hpp"
#include "d3d11_context.hpp"
#include "log/log.hpp"
#include "mtld11_resource.hpp"
#include "d3d11_device.hpp"
#include "util_env.hpp"
#include "util_string.hpp"
#include "wsi_monitor.hpp"
#include "wsi_platform_win32.hpp"
#include "wsi_window.hpp"
#include "dxmt_info.hpp"
#include <cfloat>

/**
Metal support at most 3 swapchain
*/
constexpr size_t kSwapchainLatency = 3;

namespace dxmt {


template <bool EnableMetalFX>
class MTLD3D11SwapChain final : public MTLDXGISubObject<IDXGISwapChain4, MTLD3D11Device> {
public:
  MTLD3D11SwapChain(
      IDXGIFactory1 *pFactory, MTLD3D11Device *pDevice, IMTLDXGIDevice *pLayerFactoryWeakref,
      CA::MetalLayer *pLayerWeakref, HWND hWnd, void *hNativeView, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
      const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc
  ) :
      MTLDXGISubObject(pDevice),
      factory_(pFactory),
      layer_factory_weak_(pLayerFactoryWeakref),
      native_view_(hNativeView),
      layer_weak_(pLayerWeakref),
      presentation_count_(0),
      desc_(*pDesc),
      hWnd(hWnd),
      monitor_(wsi::getWindowMonitor(hWnd)),
      hud(CA::DeveloperHUDProperties::instance()) {

    Com<ID3D11DeviceContext1> context;
    m_device->GetImmediateContext1(&context);
    context->QueryInterface(IID_PPV_ARGS(&device_context_));

    frame_latency = kSwapchainLatency;
    present_semaphore_ = CreateSemaphore(nullptr, frame_latency,
                                         DXGI_MAX_SWAP_CHAIN_BUFFERS, nullptr);

    if (desc_.Width == 0 || desc_.Height == 0) {
      wsi::getWindowSize(hWnd, &desc_.Width, &desc_.Height);
    }

    if (pFullscreenDesc) {
      fullscreen_desc_ = *pFullscreenDesc;
    } else {
      fullscreen_desc_.Windowed = true;
    }

    preferred_max_frame_rate =
        Config::getInstance().getOption<int>("d3d11.preferredMaxFrameRate", 0);
    wsi::WsiMode current_mode;
    if (wsi::getCurrentDisplayMode(monitor_, &current_mode) &&
        current_mode.refreshRate.denominator != 0 &&
        current_mode.refreshRate.numerator != 0) {
      init_refresh_rate_ = (double)current_mode.refreshRate.numerator /
                           (double)current_mode.refreshRate.denominator;
    }

    auto pool = transfer(NS::AutoreleasePool::alloc()->init());
    auto str_dxmt_version = NS::String::string("com.github.3shain.dxmt-version",
                                               NS::ASCIIStringEncoding);
    hud->addLabel(str_dxmt_version,
                  NS::String::string("com.apple.hud-graph.default",
                                     NS::ASCIIStringEncoding));
    hud->updateLabel(str_dxmt_version,
                     NS::String::string(GetVersionDescriptionText(11, m_device->GetFeatureLevel()).c_str(),
                                        NS::UTF8StringEncoding));

    backbuffer_desc_ = D3D11_TEXTURE2D_DESC1 {
      .Width = desc_.Width,
      .Height = desc_.Height,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = desc_.Format,
      .SampleDesc = desc_.SampleDesc,
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_RENDER_TARGET,
      .CPUAccessFlags = {},
      .MiscFlags = {},
      .TextureLayout = {},
    };
    // if (desc_.BufferUsage & DXGI_USAGE_SHADER_INPUT)
    backbuffer_desc_.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    if (desc_.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
      backbuffer_desc_.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

    if constexpr (EnableMetalFX) {
      scale_factor = std::max(Config::getInstance().getOption<float>("d3d11.metalSpatialUpscaleFactor", 2), 1.0f);
    }

    ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, desc_.Flags);
  };

  ~MTLD3D11SwapChain() {
    layer_factory_weak_->ReleaseMetalLayer(hWnd, native_view_);
    CloseHandle(present_semaphore_);
  };

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1) ||
        riid == __uuidof(IDXGISwapChain2) ||
        riid == __uuidof(IDXGISwapChain3) ||
        riid == __uuidof(IDXGISwapChain4)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IDXGISwapChain1), riid)) {
      WARN("DXGISwapChain: Unknown interface query ", str::format(riid));
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
      return backbuffer_->QueryInterface(riid, surface);
    } else {
      ERR("Non zero-index buffer is not supported");
      return E_FAIL;
    }
  };

  HRESULT
  STDMETHODCALLTYPE
  SetFullscreenState(BOOL fullscreen, IDXGIOutput *target) final {
    WARN("SetFullscreenState: stub");
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetFullscreenState(BOOL *fullscreen, IDXGIOutput **target) final {
    if (fullscreen) {
      *fullscreen = !fullscreen_desc_.Windowed;
    }
    if (target) {
      *target = NULL;
      // TODO
      if (!fullscreen_desc_.Windowed)
        WARN("GetFullscreenState return null");
    }
    return S_OK;
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
    pDesc->OutputWindow = hWnd;
    pDesc->Windowed = fullscreen_desc_.Windowed;
    pDesc->SwapEffect = desc_.SwapEffect;
    pDesc->Flags = desc_.Flags;
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format,
                UINT flags) final {
    /* BufferCount ignored */
    if (Width == 0 || Height == 0) {
      wsi::getWindowSize(hWnd, &desc_.Width, &desc_.Height);
    } else {
      desc_.Width = Width;
      desc_.Height = Height;
    }
    if (Format != DXGI_FORMAT_UNKNOWN) {
      desc_.Format = Format;
    }

    backbuffer_ = nullptr;
    if (desc_.Width == 0 || desc_.Height == 0) {
      backbuffer_desc_.Width = 1;
      backbuffer_desc_.Height = 1;
    } else {
      backbuffer_desc_.Width = desc_.Width;
      backbuffer_desc_.Height = desc_.Height;
      ApplyResize();
    }

    backbuffer_desc_.Format = desc_.Format;

    Com<ID3D11Texture2D1> backbuffer_texture2d;
    if (FAILED(dxmt::CreateDeviceTexture2D(m_device, &backbuffer_desc_, nullptr, &backbuffer_texture2d)))
      return E_FAIL;
    backbuffer_ = nullptr;
    backbuffer_texture2d->QueryInterface(IID_PPV_ARGS(&backbuffer_));

    if constexpr (EnableMetalFX) {
      auto scaler_descriptor =
          transfer(MTLFX::SpatialScalerDescriptor::alloc()->init());
      scaler_descriptor->setInputHeight(desc_.Height);
      scaler_descriptor->setInputWidth(desc_.Width);
      scaler_descriptor->setOutputHeight(layer_weak_->drawableSize().height);
      scaler_descriptor->setOutputWidth(layer_weak_->drawableSize().width);
      scaler_descriptor->setOutputTextureFormat(MTL::PixelFormatBGRA8Unorm_sRGB);
      scaler_descriptor->setColorTextureFormat(MTL::PixelFormatBGRA8Unorm_sRGB);
      scaler_descriptor->setColorProcessingMode(
          MTLFX::SpatialScalerColorProcessingModePerceptual);
      metalfx_scaler = transfer(
          scaler_descriptor->newSpatialScaler(m_device->GetMTLDevice()));
      D3D11_ASSERT(metalfx_scaler && "otherwise metalfx failed to initialize");
    }

    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  ResizeTarget(const DXGI_MODE_DESC *pDesc) final {
    if (!pDesc)
      return DXGI_ERROR_INVALID_CALL;

    if (!wsi::isWindow(hWnd))
      return DXGI_ERROR_INVALID_CALL;

    // Promote display mode
    DXGI_MODE_DESC1 newDisplayMode = {};
    newDisplayMode.Width = pDesc->Width;
    newDisplayMode.Height = pDesc->Height;
    newDisplayMode.RefreshRate = pDesc->RefreshRate;
    newDisplayMode.Format = pDesc->Format;
    newDisplayMode.ScanlineOrdering = pDesc->ScanlineOrdering;
    newDisplayMode.Scaling = pDesc->Scaling;

    // Update the swap chain description
    if (newDisplayMode.RefreshRate.Numerator != 0)
      fullscreen_desc_.RefreshRate = newDisplayMode.RefreshRate;

    fullscreen_desc_.ScanlineOrdering = newDisplayMode.ScanlineOrdering;
    fullscreen_desc_.Scaling = newDisplayMode.Scaling;

    wsi::DXMTWindowState state;
    if (fullscreen_desc_.Windowed) {
      // TODO: window state is concerned by fullscreen state
      wsi::resizeWindow(hWnd, &state, newDisplayMode.Width,
                        newDisplayMode.Height);
    } else {
      ERR("DXGISwapChain::ResizeTarget: fullscreen mode not supported yet.");
      return E_FAIL;
    }

    return S_OK;
  };

  void
  ApplyResize() {
    layer_weak_->setDrawableSize({(double)(desc_.Width * scale_factor), (double)(desc_.Height * scale_factor)});
  };

  HRESULT
  STDMETHODCALLTYPE
  GetContainingOutput(IDXGIOutput **output) final {
    WARN("DXGISwapChain::GetContainingOutput: stub");
    return E_FAIL;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetFrameStatistics(DXGI_FRAME_STATISTICS *stats) final {
    WARN("DXGISwapChain::GetFrameStatistics: stub");
    stats->PresentCount = presentation_count_;
    stats->SyncRefreshCount = presentation_count_;
    stats->PresentRefreshCount = presentation_count_;
    stats->SyncGPUTime = {};
    stats->SyncQPCTime = {};
    return S_OK;
  };

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
    *pHwnd = hWnd;
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
    HRESULT hr = S_OK;
    if (desc_.Width == 0 || desc_.Height == 0)
      hr = DXGI_STATUS_OCCLUDED;
    if (PresentFlags & DXGI_PRESENT_TEST)
      return hr;

    double vsync_duration =
        std::max(SyncInterval * 1.0 /
                     (preferred_max_frame_rate ? preferred_max_frame_rate
                                               : init_refresh_rate_),
                 preferred_max_frame_rate ? 1.0 / preferred_max_frame_rate : 0);
    device_context_->Flush();
    auto &cmd_queue = m_device->GetDXMTDevice().queue();
    auto chunk = cmd_queue.CurrentChunk();
    chunk->emit([this, vsync_duration, backbuffer = backbuffer_->UseBindable(chunk->chunk_id)](auto &ctx) {
      auto drawable = layer_weak_->nextDrawable();
      auto out = drawable->texture();
      auto buf = backbuffer.texture();
      if constexpr (EnableMetalFX) {
        D3D11_ASSERT(metalfx_scaler);
        metalfx_scaler->setColorTexture(buf);
        metalfx_scaler->setOutputTexture(out);
        metalfx_scaler->encodeToCommandBuffer(ctx.cmdbuf);
      } else {
        ctx.queue->emulated_cmd.PresentToDrawable(ctx.cmdbuf, buf, out);
      }

      if (vsync_duration > 0)
        ctx.cmdbuf->presentDrawableAfterMinimumDuration(drawable, vsync_duration);
      else
        ctx.cmdbuf->presentDrawable(drawable);
      ReleaseSemaphore(present_semaphore_, 1, nullptr);
    });
    device_context_->Commit();
    cmd_queue.PresentBoundary();

    presentation_count_ += 1;

    return hr;
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

  HRESULT STDMETHODCALLTYPE SetSourceSize(UINT width, UINT height) override {
    IMPLEMENT_ME
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE GetSourceSize(UINT *width, UINT *height) override {
    IMPLEMENT_ME
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT max_latency) override {
    if (max_latency == 0 || max_latency > DXGI_MAX_SWAP_CHAIN_BUFFERS) {
      return E_INVALIDARG;
    }
    if (max_latency > frame_latency) {
      ReleaseSemaphore(present_semaphore_, max_latency - frame_latency,
                       nullptr);
    }
    frame_latency = max_latency;
    WARN("SetMaximumFrameLatency: stub: ", max_latency);

    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *max_latency) override {
    if (max_latency) {
      *max_latency = frame_latency;
    }
    return S_OK;
  };

  HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override {
    HANDLE result = nullptr;
    HANDLE processHandle = GetCurrentProcess();

    if (!DuplicateHandle(processHandle, present_semaphore_, processHandle,
                         &result, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
      return nullptr;
    }

    return result;
  };

  HRESULT STDMETHODCALLTYPE
  SetMatrixTransform(const DXGI_MATRIX_3X2_F *matrix) override {
    return DXGI_ERROR_INVALID_CALL;
  };

  HRESULT STDMETHODCALLTYPE
  GetMatrixTransform(DXGI_MATRIX_3X2_F *matrix) override {
    return DXGI_ERROR_INVALID_CALL;
  };

  HRESULT CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace,
                                 UINT *pColorSpaceSupport) override {
    WARN("DXGISwapChain3::CheckColorSpaceSupport: stub");
    if (pColorSpaceSupport) {
      *pColorSpaceSupport = 0;
    }
    return S_OK;
  };

  UINT GetCurrentBackBufferIndex() override {
    // should always 0?
    return 0;
  }

  HRESULT ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height,
                         DXGI_FORMAT Format, UINT SwapChainFlags,
                         const UINT *pCreationNodeMask,
                         IUnknown *const *ppPresentQueue) override {
    WARN("DXGISwapChain3::ResizeBuffers1: ignoring d3d12 related parameters");
    return ResizeBuffers(BufferCount, Width, Height, Format, SwapChainFlags);
  }

  HRESULT SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override {
    WARN("DXGISwapChain3::SetColorSpace1: stub");
    return S_OK;
  }

  HRESULT SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size,
                         void *pMetaData) override {
    WARN("DXGISwapChain4::SetHDRMetaData: stub");
    return DXGI_ERROR_UNSUPPORTED;
  }

private:
  Com<IDXGIFactory1> factory_;
  IMTLDXGIDevice *layer_factory_weak_;
  void *native_view_;
  CA::MetalLayer *layer_weak_;
  ULONG presentation_count_;
  DXGI_SWAP_CHAIN_DESC1 desc_;
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc_;
  D3D11_TEXTURE2D_DESC1 backbuffer_desc_;
  Com<IMTLD3D11DeviceContext> device_context_;
  Com<IMTLBindable> backbuffer_;
  HANDLE present_semaphore_;
  HWND hWnd;
  HMONITOR monitor_;
  uint32_t frame_latency;
  double init_refresh_rate_ = DBL_MAX;
  int preferred_max_frame_rate = 0;
  CA::DeveloperHUDProperties* hud;

  std::conditional<EnableMetalFX, Obj<MTLFX::SpatialScaler>, std::monostate>::type metalfx_scaler;
  float scale_factor = 1.0;
};

HRESULT
CreateSwapChain(
    IDXGIFactory1 *pFactory, MTLD3D11Device *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGISwapChain1 **ppSwapChain
) {
  if (pDesc == NULL)
    return DXGI_ERROR_INVALID_CALL;
  if (ppSwapChain == NULL)
    return DXGI_ERROR_INVALID_CALL;
  InitReturnPtr(ppSwapChain);

  Com<IMTLDXGIDevice> layer_factory;
  if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&layer_factory)))) {
    ERR("CreateSwapChain: failed to get IMTLDXGIDevice");
    return E_FAIL;
  }
  CA::MetalLayer *layer;
  void *native_view;
  if (FAILED(layer_factory->GetMetalLayerFromHwnd(hWnd, &layer, &native_view))) {
    ERR("CreateSwapChain: failed to create CAMetalLayer");
    return E_FAIL;
  }
  layer->setDevice(pDevice->GetMTLDevice());
  layer->setOpaque(true);
  layer->setDisplaySyncEnabled(false);
  layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  layer->setFramebufferOnly(true);
  if (env::getEnvVar("DXMT_METALFX_SPATIAL_SWAPCHAIN") == "1") {
    *ppSwapChain = new MTLD3D11SwapChain<true>(
        pFactory, pDevice, layer_factory.ptr(), layer, hWnd, native_view, pDesc, pFullscreenDesc
    );
    return S_OK;
  }
  *ppSwapChain = new MTLD3D11SwapChain<false>(
      pFactory, pDevice, layer_factory.ptr(), layer, hWnd, native_view, pDesc, pFullscreenDesc
  );
  return S_OK;
};

} // namespace dxmt