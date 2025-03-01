#include "d3d11_swapchain.hpp"
#include "MetalFX/MTLFXSpatialScaler.hpp"
#include "QuartzCore/CADeveloperHUDProperties.hpp"
#include "com/com_guid.hpp"
#include "config/config.hpp"
#include "d3d11_private.h"
#include "dxgi_interfaces.h"
#include "dxgi_object.hpp"
#include "d3d11_context.hpp"
#include "dxmt_context.hpp"
#include "dxmt_hud_state.hpp"
#include "dxmt_statistics.hpp"
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
#include <format>

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

    hud.initialize(GetVersionDescriptionText(11, m_device->GetFeatureLevel()));

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
      layer_weak_->setContentsScale(layer_weak_->contentsScale() * scale_factor);
    }

    // FIXME: check HRESULT!
    ResizeBuffers(0, desc_.Width, desc_.Height, DXGI_FORMAT_UNKNOWN, desc_.Flags);
    if (!fullscreen_desc_.Windowed)
      EnterFullscreenMode(nullptr);
  };

  ~MTLD3D11SwapChain() {
    device_context_->WaitUntilGPUIdle();
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
      return DXGI_ERROR_UNSUPPORTED;
    }
  };

  HRESULT
  STDMETHODCALLTYPE
  SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget) final {
    Com<IDXGIOutput1> target;

    if (pTarget) {
      DXGI_OUTPUT_DESC desc;

      pTarget->QueryInterface(IID_PPV_ARGS(&target));
      target->GetDesc(&desc);

      if (!fullscreen_desc_.Windowed && Fullscreen && monitor_ != desc.Monitor) {
        HRESULT hr = LeaveFullscreenMode();
        if (FAILED(hr))
          return hr;
      }
    }

    if (fullscreen_desc_.Windowed && Fullscreen)
      return EnterFullscreenMode(target.ptr());
    else if (!fullscreen_desc_.Windowed && !Fullscreen)
      return LeaveFullscreenMode();

    return S_OK;
  };

  HRESULT EnterFullscreenMode(IDXGIOutput1* pTarget) {
    Com<IDXGIOutput1> output = pTarget;

    if (!wsi::isWindow(hWnd))
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    
    if (output == nullptr) {
      if (FAILED(GetOutputFromMonitor(wsi::getWindowMonitor(hWnd), &output))) {
       ERR("DXGI: EnterFullscreenMode: Cannot query containing output");
        return E_FAIL;
      }
    }
    
    // Update swap chain description
    fullscreen_desc_.Windowed = FALSE;
    
    // Move the window so that it covers the entire output
    bool modeSwitch = (desc_.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH) != 0u;

    DXGI_OUTPUT_DESC desc;
    output->GetDesc(&desc);

    if (!wsi::enterFullscreenMode(desc.Monitor, hWnd, &window_state_, modeSwitch)) {
      ERR("DXGI: EnterFullscreenMode: Failed to enter fullscreen mode");
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    
    monitor_ = desc.Monitor;
    target_  = std::move(output);

    return S_OK;
  }
  
  
  HRESULT LeaveFullscreenMode() {
    // Restore internal state
    fullscreen_desc_.Windowed = TRUE;
    target_  = nullptr;
    monitor_ = wsi::getWindowMonitor(hWnd);
    
    if (!wsi::isWindow(hWnd))
      return S_OK;
    
    if (!wsi::leaveFullscreenMode(hWnd, &window_state_, true)) {
      ERR("DXGI: LeaveFullscreenMode: Failed to exit fullscreen mode");
      return DXGI_ERROR_NOT_CURRENTLY_AVAILABLE;
    }
    
    return S_OK;
  }

  HRESULT
  STDMETHODCALLTYPE
  GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget) final {
    HRESULT hr = S_OK;

    if (pFullscreen != nullptr)
      *pFullscreen = !fullscreen_desc_.Windowed;
    
    if (ppTarget != nullptr)
      *ppTarget = target_.ref();

    return hr;
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

    backbuffer_ = nullptr;
    if (FAILED(dxmt::CreateDeviceTexture2D(
            m_device, &backbuffer_desc_, nullptr, reinterpret_cast<ID3D11Texture2D1 **>(&backbuffer_)
        )))
      return E_FAIL;
    // CreateDeviceTexture2D returns public reference, change to private one here
    backbuffer_->AddRefPrivate();
    backbuffer_->Release();

    if constexpr (EnableMetalFX) {
      D3D11_TEXTURE2D_DESC1 upscaled_desc_ = backbuffer_desc_;
      upscaled_desc_.Height *= scale_factor;
      upscaled_desc_.Width *= scale_factor;
      upscaled_backbuffer_ = nullptr;
      if (FAILED(dxmt::CreateDeviceTexture2D(
              m_device, &upscaled_desc_, nullptr, reinterpret_cast<ID3D11Texture2D1 **>(&upscaled_backbuffer_)
          )))
        return E_FAIL;

      auto scaler_descriptor = transfer(MTLFX::SpatialScalerDescriptor::alloc()->init());
      scaler_descriptor->setInputHeight(desc_.Height);
      scaler_descriptor->setInputWidth(desc_.Width);
      scaler_descriptor->setOutputHeight(layer_weak_->drawableSize().height);
      scaler_descriptor->setOutputWidth(layer_weak_->drawableSize().width);
      scaler_descriptor->setColorTextureFormat(backbuffer_->texture()->pixelFormat());
      scaler_descriptor->setOutputTextureFormat(upscaled_backbuffer_->texture()->pixelFormat());
      metalfx_scaler = transfer(scaler_descriptor->newSpatialScaler(m_device->GetMTLDevice()));
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

    if (fullscreen_desc_.Windowed) {
      wsi::resizeWindow(hWnd, &window_state_, newDisplayMode.Width,
                        newDisplayMode.Height);
    } else {
      /* 
      FIXME: are we ignoring display mode on purpose?
      */
      wsi::updateFullscreenWindow(monitor_, hWnd, false);

      /* 
      FIXME: this is not elegant because the size of window is not changed!
      However some games only invoke ResizeBuffers() on WM_SIZE, which should be actually
      sent by changing display mode?
       */
      SendMessage(hWnd, WM_SIZE, 0, MAKELONG(newDisplayMode.Width, newDisplayMode.Height));
    }

    return S_OK;
  };

  void
  ApplyResize() {
    layer_weak_->setDrawableSize({(double)(desc_.Width * scale_factor), (double)(desc_.Height * scale_factor)});
  };
  
  HRESULT GetOutputFromMonitor(
          HMONITOR                  Monitor,
          IDXGIOutput1**            ppOutput) {
    if (!ppOutput)
      return DXGI_ERROR_INVALID_CALL;

    Com<IDXGIAdapter> adapter;
    Com<IDXGIOutput> output;

    if (FAILED(layer_factory_weak_->GetAdapter(&adapter)))
      return E_FAIL;

    for (uint32_t i = 0; SUCCEEDED(adapter->EnumOutputs(i, &output)); i++) {
      DXGI_OUTPUT_DESC outputDesc;
      output->GetDesc(&outputDesc);
      
      if (outputDesc.Monitor == Monitor)
        return output->QueryInterface(IID_PPV_ARGS(ppOutput));
      
      output = nullptr;
    }
    
    return DXGI_ERROR_NOT_FOUND;
  }

  HRESULT
  STDMETHODCALLTYPE
  GetContainingOutput(IDXGIOutput **ppOutput) final {
    InitReturnPtr(ppOutput);
    
    if (!wsi::isWindow(hWnd))
      return DXGI_ERROR_INVALID_CALL;
    
    Com<IDXGIOutput1> output;

    if (target_ == nullptr) {
      HRESULT hr = GetOutputFromMonitor(wsi::getWindowMonitor(hWnd), &output);

      if (FAILED(hr))
        return hr;
    } else {
      output = target_;
    }

    *ppOutput = output.ref();
    return S_OK;
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
    device_context_->PrepareFlush();
    auto &cmd_queue = m_device->GetDXMTDevice().queue();
    auto chunk = cmd_queue.CurrentChunk();
    if constexpr (EnableMetalFX) {
      chunk->emitcc([this, vsync_duration, backbuffer = backbuffer_->texture(),
                     upscaled = upscaled_backbuffer_->texture(),
                     scaler = this->metalfx_scaler](ArgumentEncodingContext &ctx) mutable {
        auto &scaler_info = ctx.currentFrameStatistics().last_scaler_info;
        scaler_info.type = ScalerType::Spatial;
        scaler_info.input_width = backbuffer->width();
        scaler_info.input_height = backbuffer->height();
        scaler_info.output_width = upscaled->width();
        scaler_info.output_height = upscaled->height();
        ctx.upscale(backbuffer, upscaled, scaler);
        ctx.present(upscaled, layer_weak_, vsync_duration);
        ReleaseSemaphore(present_semaphore_, 1, nullptr);
        this->UpdateStatistics(ctx.queue().statistics, ctx.currentFrameId());
      });
    } else {
      chunk->emitcc([this, vsync_duration, backbuffer = backbuffer_->texture()](ArgumentEncodingContext &ctx) mutable {
        ctx.present(backbuffer, layer_weak_, vsync_duration);
        ReleaseSemaphore(present_semaphore_, 1, nullptr);
        this->UpdateStatistics(ctx.queue().statistics, ctx.currentFrameId());
      });
    }
    chunk->signal_frame_latency_fence_ = cmd_queue.CurrentFrameSeq();
    device_context_->Commit();
    cmd_queue.PresentBoundary();

    presentation_count_ += 1;

    return hr;
  };

  void UpdateStatistics(const FrameStatisticsContainer& statistics, uint64_t frame_id) {
    hud.begin();
    auto &frame = statistics.at(frame_id - 1); // show the previous one frame statistics
    auto &average = statistics.average();
    Flags<FeatureCompatibility> flags = frame.compatibility_flags;
    char text[] = "---------------------------";
    if (flags.test(FeatureCompatibility::UnsupportedGeometryDraw)) {
      text[3] = 'G';
    }
    if (flags.test(FeatureCompatibility::UnsupportedTessellationOutputPrimitive)) {
      text[5] = 'T';
      text[6] = 'O';
    }
    if (flags.test(FeatureCompatibility::UnsupportedIndirectTessellationDraw)) {
      text[8] = 'I';
      text[9] = 'T';
    }
    if (flags.test(FeatureCompatibility::UnsupportedGeometryTessellationDraw)) {
      text[11] = 'G';
      text[12] = 'T';
    }
    if (flags.test(FeatureCompatibility::UnsupportedDrawAuto)) {
      text[14] = 'A';
    }
    if (flags.test(FeatureCompatibility::UnsupportedPredication)) {
      text[16] = 'P';
    }
    if (flags.test(FeatureCompatibility::UnsupportedStreamOutputAppending)) {
      text[18] = 'S';
      text[19] = 'A';
    }
    if (flags.test(FeatureCompatibility::UnsupportedMultipleStreamOutput)) {
      text[21] = 'M';
      text[22] = 'S';
    }
    hud.printLine(text);
    hud.printLine(std::format(
        "Commit: {:2} -{:4.1f} -{:4.1f}", std::min(frame.command_buffer_count, 99u),
        std::min(average.commit_interval.count() / 1000000.0, 99.9),
        std::min(statistics.max().commit_interval.count() / 1000000.0, 99.9)
    ));
    hud.printLine(std::format(
        "Sync:   {:2} {:4.1f}  {:2} {:4.1f} {:2}", std::min(frame.sync_count, 99u),
        std::min(average.sync_interval.count() / 1000000.0, 99.9), std::min(statistics.max().event_stall, 99u),
        std::min(average.present_lantency_interval.count() / 1000000.0, 99.9), frame.latency
    ));
    hud.printLine(std::format(
        "Encode: {:4.1f}+{:4.1f}+{:4.1f}={:4.1f}", std::min(average.encode_prepare_interval.count() / 1000000.0, 99.9),
        std::min((average.encode_flush_interval - average.drawable_blocking_interval).count() / 1000000.0, 99.9),
        std::min(average.drawable_blocking_interval.count() / 1000000.0, 99.9),
        std::min((average.encode_prepare_interval + average.encode_flush_interval).count() / 1000000.0, 99.9)
    ));
    hud.printLine(std::format(
        "Render:{:3}+{:<3} Clear:{:3}+{:<2}", std::min(frame.render_pass_count - frame.render_pass_optimized, 999u),
        std::min(frame.render_pass_optimized, 999u),
        std::min(frame.clear_pass_count - frame.clear_pass_optimized, 999u), std::min(frame.clear_pass_optimized, 99u)
    ));
    {
      /* scaler info */
      auto &info = frame.last_scaler_info;
      if (info.type == ScalerType::Temporal) {
        auto &info = frame.last_scaler_info;
        hud.printLine(std::format(
            "MetalFX: Temporal {} {}", info.auto_exposure ? "AEXP" : "", info.motion_vector_highres ? "HMV" : ""
        ));
        hud.printLine(std::format(
            "Scale: {:4}x{:4}->{:4}x{:4}", info.input_width, info.input_height, info.output_width, info.output_height
        ));
      }
      if (info.type == ScalerType::Spatial) {
        hud.printLine(std::format(
            "MetalFX: Spatial"
        ));
        hud.printLine(std::format(
            "Scale: {:4}x{:4}->{:4}x{:4}", info.input_width, info.input_height, info.output_width, info.output_height
        ));
      }
    }
    hud.end();
  }

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
  Com<D3D11ResourceCommon, false> backbuffer_;
  HANDLE present_semaphore_;
  HWND hWnd;
  HMONITOR monitor_;
  Com<IDXGIOutput1> target_;
  wsi::DXMTWindowState window_state_;
  uint32_t frame_latency;
  double init_refresh_rate_ = DBL_MAX;
  int preferred_max_frame_rate = 0;
  HUDState hud;

  std::conditional<EnableMetalFX, Obj<MTLFX::SpatialScaler>, std::monostate>::type metalfx_scaler;
  std::conditional<EnableMetalFX, Com<D3D11ResourceCommon>, std::monostate>::type upscaled_backbuffer_;
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
  if ((pDesc->SwapEffect != DXGI_SWAP_EFFECT_DISCARD &&
       pDesc->SwapEffect != DXGI_SWAP_EFFECT_FLIP_DISCARD) &&
      pDesc->BufferCount != 1) {
    ERR("CreateSwapChain: unsupported swap effect ", pDesc->SwapEffect, " with backbuffer size ", pDesc->BufferCount);
    return E_FAIL;
  }
  layer->setDevice(pDevice->GetMTLDevice());
  layer->setOpaque(false);
  layer->setDisplaySyncEnabled(false);
  layer->setFramebufferOnly(true);
  if (env::getEnvVar("DXMT_METALFX_SPATIAL_SWAPCHAIN") == "1") {
    if (MTLFX::SpatialScalerDescriptor::supportsDevice(pDevice->GetMTLDevice())) {
      *ppSwapChain = new MTLD3D11SwapChain<true>(
          pFactory, pDevice, layer_factory.ptr(), layer, hWnd, native_view, pDesc, pFullscreenDesc
      );
      return S_OK;
    } else {
      WARN("MetalFX spatial scaler is not supported on this device");
    }
  }
  *ppSwapChain = new MTLD3D11SwapChain<false>(
      pFactory, pDevice, layer_factory.ptr(), layer, hWnd, native_view, pDesc, pFullscreenDesc
  );
  return S_OK;
};

} // namespace dxmt