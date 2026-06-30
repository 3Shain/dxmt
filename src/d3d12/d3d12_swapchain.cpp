/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "d3d12_device.hpp"
#include "dxgi_interfaces.h"
#include "dxgi_object.hpp"
#include "dxmt_hud_state.hpp"
#include "dxmt_presenter.hpp"
#include "dxmt_info.hpp"
#include "com/com_pointer.hpp"
#include "log/log.hpp"
#include "wsi_window.hpp"

/**
Ref: https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-setmaximumframelatency
This value is 1 by default.
*/
constexpr size_t kSwapchainLatency = 1;

/**
TODO: some implementations can be shared with d3d11 swapchain
*/

namespace dxmt {

WMTPixelFormat
ConvertSwapChainFormat(DXGI_FORMAT format) {
  switch (format) {
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    return WMTPixelFormatBGRA8Unorm_sRGB;
  case DXGI_FORMAT_B8G8R8A8_UNORM:
  case DXGI_FORMAT_R8G8B8A8_UNORM:
    return WMTPixelFormatBGRA8Unorm;
  case DXGI_FORMAT_R10G10B10A2_UNORM:
    return WMTPixelFormatRGB10A2Unorm;
  case DXGI_FORMAT_R16G16B16A16_FLOAT:
    return WMTPixelFormatRGBA16Float;
  default:
    return WMTPixelFormatInvalid;
  }
}

WMTColorSpace
ConvertColorSpace(DXGI_COLOR_SPACE_TYPE color_space, bool hdr) {
  switch (color_space) {
  case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
    return WMTColorSpaceSRGB;
  case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
    return hdr ? WMTColorSpaceHDR_scRGB : WMTColorSpaceSRGBLinear;
  case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
    return WMTColorSpaceHDR_PQ;
  case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
    return WMTColorSpaceBT2020;
  default:
    return WMTColorSpaceInvalid;
  }
}

class MTLD3D12SwapChain final : public MTLDXGISubObject<IDXGISwapChain4, MTLD3D12Device> {

  Com<IDXGIFactory1> factory_;
  MTLD3D12CommandQueue *queue_;
  HWND hWnd;
  HMONITOR monitor_;
  Com<IDXGIOutput1> target_;
  wsi::DXMTWindowState window_state_;
  ULONG presentation_count_ = 0;
  DXGI_SWAP_CHAIN_DESC1 desc_;
  D3D12_RESOURCE_DESC backbuffer_desc_;
  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc_;
  HANDLE present_semaphore_;
  std::unique_ptr<CpuFence> frame_latency_fence_;
  WMT::Object native_view_;
  WMT::MetalLayer layer_weak_;
  Rc<Presenter> presenter;
  uint32_t frame_latency;
  DXGI_COLOR_SPACE_TYPE colorspace_ = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  float scale_factor = 1.0;
  HUDState hud;

  std::vector<Com<MTLD3D12Resource>> backbuffers_;

  bool
  LayerSupportEDR() {
    WMTEDRValue edr_value;
    MetalLayer_getEDRValue(layer_weak_, &edr_value);
    return edr_value.maximum_potential_edr_color_component_value > 1.0f;
  };

  InternalCommandLibrary lib;

public:
  MTLD3D12SwapChain(
      IDXGIFactory1 *pFactory, MTLD3D12Device *pDevice, MTLD3D12CommandQueue *pQueue, HWND hWnd,
      const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc
  ) :
      MTLDXGISubObject(pDevice),
      factory_(pFactory),
      queue_(pQueue),
      hWnd(hWnd),
      monitor_(wsi::getWindowMonitor(hWnd)),
      desc_(*pDesc),
      hud(WMT::DeveloperHUDProperties::instance()),
      lib(pDevice->GetMTLDevice()) {

    native_view_ = WMT::CreateMetalViewFromHWND((intptr_t)hWnd, pDevice->GetMTLDevice(), layer_weak_);

    if (!native_view_) {
      ERR("Failed to create metal view, it seems like your Wine has no exported symbols needed by DXMT.");
      abort();
    }

    presenter = Rc(new Presenter(pDevice->GetMTLDevice(), layer_weak_, lib, scale_factor, desc_.SampleDesc.Count));

    frame_latency = kSwapchainLatency;
    present_semaphore_ = CreateSemaphore(nullptr, frame_latency, DXGI_MAX_SWAP_CHAIN_BUFFERS, nullptr);

    if (desc_.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) {
      frame_latency_fence_ = std::make_unique<CpuFence>();
    }

    if (desc_.Width == 0 || desc_.Height == 0) {
      wsi::getWindowSize(hWnd, &desc_.Width, &desc_.Height);
    }

    if (pFullscreenDesc) {
      fullscreen_desc_ = *pFullscreenDesc;
    } else {
      fullscreen_desc_.Windowed = true;
    }

    hud.initialize(GetVersionDescriptionText(12, device_->GetFeatureLevel()));

    backbuffer_desc_ = D3D12_RESOURCE_DESC{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 1,
        .Width = desc_.Width,
        .Height = desc_.Height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = desc_.Format,
        .SampleDesc =
            {
                .Count = 1,
                .Quality = 0,
            },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    };
    if (desc_.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
      backbuffer_desc_.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }

  ~MTLD3D12SwapChain() {
    WMT::ReleaseMetalView(native_view_);
    native_view_ = {};
    CloseHandle(present_semaphore_);
  };

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1) || riid == __uuidof(IDXGISwapChain2) ||
        riid == __uuidof(IDXGISwapChain3) || riid == __uuidof(IDXGISwapChain4)) {
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
  GetBuffer(UINT Buffer, REFIID riid, void **ppSurface) final {
    if (Buffer >= backbuffers_.size())
      return DXGI_ERROR_NOT_FOUND;
    return backbuffers_[Buffer]->QueryInterface(riid, ppSurface);
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

  HRESULT
  EnterFullscreenMode(IDXGIOutput1 *pTarget) {
    IMPLEMENT_ME
    return S_OK;
  }

  HRESULT
  LeaveFullscreenMode() {
    IMPLEMENT_ME
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
  ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT flags) final {
    if (Width == 0 || Height == 0) {
      wsi::getWindowSize(hWnd, &desc_.Width, &desc_.Height);
    } else {
      desc_.Width = Width;
      desc_.Height = Height;
    }
    if (Format != DXGI_FORMAT_UNKNOWN) {
      if (ConvertSwapChainFormat(Format) != WMTPixelFormatInvalid)
        desc_.Format = Format;
    }

    if (desc_.Width == 0 || desc_.Height == 0) {
      backbuffer_desc_.Width = 1;
      backbuffer_desc_.Height = 1;
    } else {
      backbuffer_desc_.Width = desc_.Width;
      backbuffer_desc_.Height = desc_.Height;
    }

    ApplyLayerProps();

    backbuffer_desc_.Format = desc_.Format;

    backbuffers_.clear();
    if (BufferCount == 0) {
      BufferCount = desc_.BufferCount;
    } else {
      desc_.BufferCount = BufferCount;
    }
    D3D12_HEAP_PROPERTIES heap_props{
        D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE, D3D12_MEMORY_POOL_L1, 0, 0
    };
    for (unsigned i = 0; i < BufferCount; i++) {
      Com<ID3D12Resource> backbuffer;

      if (FAILED(dxmt::CreateCommittedTexture(
              device_, &heap_props, D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES, &backbuffer_desc_,
              D3D12_RESOURCE_STATE_PRESENT, nullptr, IID_PPV_ARGS(&backbuffer)
          ))) {
        backbuffers_.clear();
        return E_FAIL;
      }
      backbuffers_.push_back(reinterpret_cast<MTLD3D12Resource *>(backbuffer.ptr()));
    }

    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  ResizeTarget(const DXGI_MODE_DESC *pDesc) final {
    IMPLEMENT_ME
    return S_OK;
  };

  void
  ApplyLayerProps() {
    auto target_color_space = ConvertColorSpace(
        desc_.Format == DXGI_FORMAT_R16G16B16A16_FLOAT ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 : colorspace_,
        LayerSupportEDR()
    );
    if (presenter->changeLayerProperties(
            ConvertSwapChainFormat(desc_.Format), target_color_space, desc_.Width * scale_factor,
            desc_.Height * scale_factor, desc_.SampleDesc.Count
        )) {
      // TODO(d3d12): flush command queue
    }
  };

  HRESULT
  STDMETHODCALLTYPE
  GetContainingOutput(IDXGIOutput **ppOutput) final {
    IMPLEMENT_ME
    return S_OK;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetFrameStatistics(DXGI_FRAME_STATISTICS *stats) final {
    DEBUG("DXGISwapChain::GetFrameStatistics: stub");
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
  Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS *pPresentParameters) final {
    HRESULT hr = S_OK;
    if (desc_.Width == 0 || desc_.Height == 0)
      hr = DXGI_STATUS_OCCLUDED;
    if (PresentFlags & DXGI_PRESENT_TEST)
      return hr;

    auto &backbuffer = backbuffers_[presentation_count_ % backbuffers_.size()];
    // TODO(d3d12): flush command queue and present
    hr = queue_->Present(this->presenter.ptr(), backbuffer.ptr(), present_semaphore_);

    presentation_count_ += 1;

    return hr;
  };

  BOOL STDMETHODCALLTYPE
  IsTemporaryMonoSupported() final {
    return FALSE;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) final {
    IMPLEMENT_ME;
  };

  HRESULT
  STDMETHODCALLTYPE
  SetBackgroundColor(const DXGI_RGBA *pColor) final {
    IMPLEMENT_ME;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetBackgroundColor(DXGI_RGBA *pColor) final {
    IMPLEMENT_ME;
  };

  HRESULT
  STDMETHODCALLTYPE
  SetRotation(DXGI_MODE_ROTATION Rotation) final {
    IMPLEMENT_ME;
  };

  HRESULT
  STDMETHODCALLTYPE
  GetRotation(DXGI_MODE_ROTATION *pRotation) final {
    IMPLEMENT_ME;
  };

  HRESULT STDMETHODCALLTYPE
  SetSourceSize(UINT width, UINT height) override {
    IMPLEMENT_ME
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE
  GetSourceSize(UINT *width, UINT *height) override {
    IMPLEMENT_ME
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE
  SetMaximumFrameLatency(UINT max_latency) override {
    if (max_latency == 0 || max_latency > DXGI_MAX_SWAP_CHAIN_BUFFERS) {
      return E_INVALIDARG;
    }
    if (max_latency > frame_latency) {
      ReleaseSemaphore(present_semaphore_, max_latency - frame_latency, nullptr);
    }
    frame_latency = max_latency;

    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE
  GetMaximumFrameLatency(UINT *max_latency) override {
    if (max_latency) {
      *max_latency = frame_latency;
    }
    return S_OK;
  };

  HANDLE STDMETHODCALLTYPE
  GetFrameLatencyWaitableObject() override {
    if (!(desc_.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)) {
      return nullptr;
    }

    HANDLE result = nullptr;
    HANDLE processHandle = GetCurrentProcess();

    if (!DuplicateHandle(processHandle, present_semaphore_, processHandle, &result, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
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

  HRESULT STDMETHODCALLTYPE
  CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport) override {
    if (!pColorSpaceSupport)
      return E_INVALIDARG;
    *pColorSpaceSupport = CGColorSpace_checkColorSpaceSupported(ConvertColorSpace(ColorSpace, false))
                              ? DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT
                              : 0;
    return S_OK;
  };

  UINT STDMETHODCALLTYPE
  GetCurrentBackBufferIndex() override {
    return presentation_count_ % backbuffers_.size();
  }

  HRESULT STDMETHODCALLTYPE
  ResizeBuffers1(
      UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT *pCreationNodeMask,
      IUnknown *const *ppPresentQueue
  ) override {
    WARN("DXGISwapChain3::ResizeBuffers1: ignoring d3d12 related parameters");
    return ResizeBuffers(BufferCount, Width, Height, Format, SwapChainFlags);
  }

  HRESULT STDMETHODCALLTYPE
  SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override {
    auto target_color_space = ConvertColorSpace(ColorSpace, LayerSupportEDR());
    if (presenter->changeLayerColorSpace(target_color_space)) {
      // TODO(d3d12): flush command queue
    }
    colorspace_ = ColorSpace;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData) override {
    return S_OK;
    if (Type == DXGI_HDR_METADATA_TYPE_NONE) {
      presenter->changeHDRMetadata(nullptr);
      return S_OK;
    }
    if (Type == DXGI_HDR_METADATA_TYPE_HDR10) {
      if (Size != sizeof(WMTHDRMetadata))
        return E_INVALIDARG;
      presenter->changeHDRMetadata(reinterpret_cast<const WMTHDRMetadata *>(pMetaData));
      return S_OK;
    }
    return DXGI_ERROR_UNSUPPORTED;
  }
};

HRESULT
CreateSwapChain(
    IDXGIFactory1 *pFactory, MTLD3D12Device *pDevice, MTLD3D12CommandQueue *pQueue, HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGISwapChain1 **ppSwapChain
) {
  auto swapchain = Com(new MTLD3D12SwapChain(pFactory, pDevice, pQueue, hWnd, pDesc, pFullscreenDesc));
  HRESULT hr = swapchain->ResizeBuffers(0, pDesc->Width, pDesc->Height, DXGI_FORMAT_UNKNOWN, 0);
  if (FAILED(hr))
    return hr;
  return swapchain->QueryInterface(IID_PPV_ARGS(ppSwapChain));
}

}; // namespace dxmt