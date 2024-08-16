#include "d3d11_swapchain.hpp"
#include "com/com_guid.hpp"
#include "d3d11_private.h"
#include "dxgi_interfaces.h"
#include "dxgi_object.hpp"
#include "d3d11_context.hpp"
#include "log/log.hpp"
#include "mtld11_resource.hpp"
#include "util_error.hpp"
#include "d3d11_device.hpp"
#include "util_string.hpp"
#include "wsi_monitor.hpp"
#include "wsi_platform_win32.hpp"
#include "wsi_window.hpp"
#include <atomic>
#include <cfloat>

/**
Metal support at most 3 swapchain
*/
constexpr size_t kSwapchainLatency = 3;

namespace dxmt {

Com<IMTLD3D11BackBuffer>
CreateEmulatedBackBuffer(IMTLD3D11Device *pDevice,
                         const DXGI_SWAP_CHAIN_DESC1 *pDesc, HWND hWnd);

class MTLD3D11SwapChain final : public MTLDXGISubObject<IDXGISwapChain4> {
public:
  MTLD3D11SwapChain(IDXGIFactory1 *pFactory, IMTLDXGIDevice *pDevice, HWND hWnd,
                    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc)
      : MTLDXGISubObject(pDevice), factory_(pFactory), presentation_count_(0),
        desc_(*pDesc), hWnd(hWnd), monitor_(wsi::getWindowMonitor(hWnd)) {

    device_ = Com<IMTLD3D11Device>::queryFrom(pDevice);

    Com<ID3D11DeviceContext1> context;
    device_->GetImmediateContext1(&context);
    context->QueryInterface(IID_PPV_ARGS(&device_context_));

    frame_latency = kSwapchainLatency;
    present_semaphore_ = CreateSemaphore(nullptr, frame_latency,
                                         DXGI_MAX_SWAP_CHAIN_BUFFERS, nullptr);
    frame_latency_fence = frame_latency;

    if (desc_.Width == 0 || desc_.Height == 0) {
      wsi::getWindowSize(hWnd, &desc_.Width, &desc_.Height);
    }

    if (pFullscreenDesc) {
      fullscreen_desc_ = *pFullscreenDesc;
    } else {
      fullscreen_desc_.Windowed = true;
    }

    Com<IMTLDXGIAdatper> adapter;
    device_->GetAdapter(&adapter);
    preferred_max_frame_rate =
        adapter->GetConfigInt("d3d11.preferredMaxFrameRate", 0);
    wsi::WsiMode current_mode;
    if (wsi::getCurrentDisplayMode(monitor_, &current_mode) &&
        current_mode.refreshRate.denominator != 0 &&
        current_mode.refreshRate.numerator != 0) {
      init_refresh_rate_ = (double)current_mode.refreshRate.numerator /
                           (double)current_mode.refreshRate.denominator;
    }
  };

  ~MTLD3D11SwapChain() {
    CloseHandle(present_semaphore_);
    frame_latency_fence = frame_latency;
    destroyed = true;
    frame_latency_fence.notify_all();
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
      if (!backbuffer_) {
        backbuffer_ = CreateEmulatedBackBuffer(device_.ptr(), &desc_, hWnd);
      }
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
    // FIXME: ... weird
    if (backbuffer_) {
      device_context_->WaitUntilGPUIdle();
      auto _ = backbuffer_.takeOwnership();
      // required behavior...
      if (auto x = _->Release() != 0) {
        ERR("outstanding buffer hold! ", x);
        /**
         usually shouldn't call it, but in case devs forget to
         unbind RTV before calling `ResizeBuffers`
         */
        _->Destroy();
      }
    }
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

    frame_latency_fence.wait(0, std::memory_order_relaxed);
    if (destroyed)
      return S_OK;
    frame_latency_fence.fetch_sub(1, std::memory_order_relaxed);

    double vsync_duration =
        std::max(SyncInterval * 1.0 /
                     (preferred_max_frame_rate ? preferred_max_frame_rate
                                               : init_refresh_rate_),
                 preferred_max_frame_rate ? 1.0 / preferred_max_frame_rate : 0);
    device_context_->FlushInternal(
        [this, backbuffer = backbuffer_,
         vsync_duration](MTL::CommandBuffer *cmdbuf) {
          backbuffer->Present(cmdbuf, vsync_duration);
          ReleaseSemaphore(present_semaphore_, 1, nullptr);
        },
        [this]() {
          if (frame_latency_diff) {
            frame_latency_diff--;
          } else {
            frame_latency_fence.fetch_add(1, std::memory_order_relaxed);
            frame_latency_fence.notify_one();
          }
        },
        true);

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
      frame_latency_fence.fetch_add(max_latency - frame_latency);
      frame_latency_fence.notify_one();
      ReleaseSemaphore(present_semaphore_, max_latency - frame_latency,
                       nullptr);
    } else {
      frame_latency_diff = frame_latency - max_latency;
    }
    frame_latency = max_latency;

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
  ULONG presentation_count_;
  DXGI_SWAP_CHAIN_DESC1 desc_;
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc_;
  Com<IMTLD3D11Device> device_;
  Com<IMTLD3D11DeviceContext> device_context_;
  Com<IMTLD3D11BackBuffer> backbuffer_;
  HANDLE present_semaphore_;
  std::atomic_uint64_t frame_latency_fence;
  HWND hWnd;
  HMONITOR monitor_;
  uint32_t frame_latency;
  uint32_t frame_latency_diff = 0;
  double init_refresh_rate_ = DBL_MAX;
  int preferred_max_frame_rate = 0;

  bool destroyed = false;
};

HRESULT CreateSwapChain(IDXGIFactory1 *pFactory, IMTLDXGIDevice *pDevice,
                        HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
                        IDXGISwapChain1 **ppSwapChain) {
  if (pDesc == NULL) {
    return DXGI_ERROR_INVALID_CALL;
  }
  if (ppSwapChain == NULL) {
    return DXGI_ERROR_INVALID_CALL;
  }
  InitReturnPtr(ppSwapChain);
  try {
    *ppSwapChain =
        new MTLD3D11SwapChain(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc);
    return S_OK;
  } catch (MTLD3DError &err) {
    ERR(err.message());
    return E_FAIL;
  }
};

} // namespace dxmt