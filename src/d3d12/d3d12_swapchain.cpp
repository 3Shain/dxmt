#include "d3d12_swapchain.hpp"
#include "d3d12_device.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_resource.hpp"
#include "dxmt_presenter.hpp"
#include "dxmt_command.hpp"
#include "log/log.hpp"
#include "wsi_window.hpp"
#include <cfloat>

namespace dxmt::d3d12 {

// ──────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────

static WMTPixelFormat ConvertSwapChainFormat(DXGI_FORMAT format) {
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

static WMTColorSpace ConvertColorSpace(DXGI_FORMAT format) {
    if (format == DXGI_FORMAT_R16G16B16A16_FLOAT)
        return WMTColorSpaceSRGBLinear;
    return WMTColorSpaceSRGB;
}

// ──────────────────────────────────────────────
// D3D12SwapChain
// ──────────────────────────────────────────────

D3D12SwapChain::D3D12SwapChain(
    D3D12Device *device,
    D3D12CommandQueue *queue,
    HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc)
    : device_(device)
    , queue_(queue)
    , hWnd_(hWnd)
    , desc_(*pDesc)
    , refcount_(1) {

    if (pFullscreenDesc) {
        fullscreen_desc_ = *pFullscreenDesc;
    } else {
        fullscreen_desc_.Windowed = TRUE;
    }

    // Get window size if zero
    if (desc_.Width == 0 || desc_.Height == 0) {
        wsi::getWindowSize(hWnd_, &desc_.Width, &desc_.Height);
    }

    // Create Metal view
    native_view_ = WMT::CreateMetalViewFromHWND(
        (intptr_t)hWnd_, device_->GetMTLDevice(), layer_);

    if (!native_view_) {
        Logger::err("D3D12SwapChain: Failed to create Metal view");
    }

    // Ensure minimum dimensions
    UINT width = desc_.Width > 0 ? desc_.Width : 1;
    UINT height = desc_.Height > 0 ? desc_.Height : 1;

    // Create Presenter
    auto &lib = device_->GetDXMTDevice().queue().cmd_library;
    float scale_factor = 1.0f;
    UINT sample_count = desc_.SampleDesc.Count > 0 ? desc_.SampleDesc.Count : 1;

    presenter_ = Rc<Presenter>(new Presenter(
        device_->GetMTLDevice(), layer_, lib,
        scale_factor, sample_count));

    // Initialize layer properties
    auto target_color_space = ConvertColorSpace(desc_.Format);
    presenter_->changeLayerProperties(
        ConvertSwapChainFormat(desc_.Format), target_color_space,
        width, height, sample_count);

    // Create back buffers
    CreateBackBuffers();

    TRACE("D3D12SwapChain created: ", width, "x", height,
          " buffers=", desc_.BufferCount);
}

D3D12SwapChain::~D3D12SwapChain() {
    DestroyBackBuffers();
    if (native_view_) {
        WMT::ReleaseMetalView(native_view_);
    }
    TRACE("D3D12SwapChain destroyed");
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12SwapChain::QueryInterface(
    REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
        return E_POINTER;

    *ppvObject = nullptr;

    if (riid == IID_IUnknown ||
        riid == IID_IDXGISwapChain ||
        riid == IID_IDXGISwapChain1 ||
        riid == IID_IDXGISwapChain2 ||
        riid == IID_IDXGISwapChain3) {
        *ppvObject = static_cast<IDXGISwapChain3 *>(this);
        AddRef();
        return S_OK;
    }

    WARN("D3D12SwapChain: Unknown interface query");
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12SwapChain::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE D3D12SwapChain::Release() {
    ULONG count = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (count == 0) {
        delete this;
    }
    return count;
}

// ── IDXGISwapChain ──

HRESULT STDMETHODCALLTYPE D3D12SwapChain::Present(
    UINT SyncInterval, UINT Flags) {
    return Present1(SyncInterval, Flags, nullptr);
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetBuffer(
    UINT Buffer, REFIID riid, void **ppSurface) {
    InitReturnPtr(ppSurface);

    if (Buffer >= backbuffers_.size()) {
        ERR("D3D12SwapChain::GetBuffer: index out of range: ", Buffer);
        return DXGI_ERROR_INVALID_CALL;
    }

    return backbuffers_[Buffer]->QueryInterface(riid, ppSurface);
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::SetFullscreenState(
    BOOL Fullscreen, void *pTarget) {
    WARN("D3D12SwapChain::SetFullscreenState: stub");
    fullscreen_desc_.Windowed = !Fullscreen;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetFullscreenState(
    BOOL *pFullscreen, void **ppTarget) {
    if (pFullscreen)
        *pFullscreen = !fullscreen_desc_.Windowed;
    if (ppTarget)
        *ppTarget = nullptr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetDesc(
    DXGI_SWAP_CHAIN_DESC *pDesc) {
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
    pDesc->OutputWindow = hWnd_;
    pDesc->Windowed = fullscreen_desc_.Windowed;
    pDesc->SwapEffect = desc_.SwapEffect;
    pDesc->Flags = desc_.Flags;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::ResizeBuffers(
    UINT BufferCount, UINT Width, UINT Height,
    DXGI_FORMAT NewFormat, UINT SwapChainFlags) {

    if (Width == 0 || Height == 0) {
        wsi::getWindowSize(hWnd_, &desc_.Width, &desc_.Height);
    } else {
        desc_.Width = Width;
        desc_.Height = Height;
    }

    if (NewFormat != DXGI_FORMAT_UNKNOWN) {
        desc_.Format = NewFormat;
    }
    if (SwapChainFlags)
        desc_.Flags = SwapChainFlags;
    if (BufferCount > 0)
        desc_.BufferCount = BufferCount;

    UINT w = desc_.Width > 0 ? desc_.Width : 1;
    UINT h = desc_.Height > 0 ? desc_.Height : 1;

    // Update Presenter layer properties
    auto target_color_space = ConvertColorSpace(desc_.Format);
    UINT sample_count = desc_.SampleDesc.Count > 0 ? desc_.SampleDesc.Count : 1;
    presenter_->changeLayerProperties(
        ConvertSwapChainFormat(desc_.Format), target_color_space,
        w, h, sample_count);

    // Recreate back buffers
    DestroyBackBuffers();
    CreateBackBuffers();

    TRACE("D3D12SwapChain::ResizeBuffers: ", w, "x", h,
          " count=", desc_.BufferCount);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::ResizeTarget(
    const DXGI_MODE_DESC *pNewTargetParameters) {
    if (!pNewTargetParameters)
        return DXGI_ERROR_INVALID_CALL;

    desc_.Width = pNewTargetParameters->Width;
    desc_.Height = pNewTargetParameters->Height;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetContainingOutput(
    void **ppOutput) {
    InitReturnPtr(ppOutput);
    WARN("D3D12SwapChain::GetContainingOutput: stub");
    return DXGI_ERROR_NOT_FOUND;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetFrameStatistics(
    DXGI_FRAME_STATISTICS *pStats) {
    if (!pStats)
        return E_POINTER;

    pStats->PresentCount = presentation_count_.load();
    pStats->PresentRefreshCount = presentation_count_.load();
    pStats->SyncRefreshCount = presentation_count_.load();
    pStats->SyncGPUTime = {};
    pStats->SyncQPCTime = {};
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetLastPresentCount(
    UINT *pLastPresentCount) {
    if (!pLastPresentCount)
        return E_POINTER;

    *pLastPresentCount = presentation_count_.load();
    return S_OK;
}

// ── IDXGISwapChain1 ──

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetDesc1(
    DXGI_SWAP_CHAIN_DESC1 *pDesc) {
    if (!pDesc)
        return E_POINTER;
    *pDesc = desc_;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetFullscreenDesc(
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) {
    if (!pDesc)
        return E_POINTER;
    *pDesc = fullscreen_desc_;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetHwnd(HWND *pHwnd) {
    if (!pHwnd)
        return E_POINTER;
    *pHwnd = hWnd_;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetCoreWindow(
    REFIID refiid, void **ppUnk) {
    InitReturnPtr(ppUnk);
    ERR("D3D12SwapChain::GetCoreWindow: not implemented");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::Present1(
    UINT SyncInterval, UINT PresentFlags,
    const void *pPresentParameters) {

    if (SyncInterval > 4)
        return DXGI_ERROR_INVALID_CALL;

    // Check for minimized/zero-size window
    bool minimized = wsi::isMinimized(hWnd_);
    HRESULT hr = S_OK;
    if (minimized || desc_.Width == 0 || desc_.Height == 0)
        hr = DXGI_STATUS_OCCLUDED;

    if (PresentFlags & DXGI_PRESENT_TEST)
        return hr;

    if (hr == DXGI_STATUS_OCCLUDED)
        return hr;

    // Synchronize layer properties
    auto state = presenter_->synchronizeLayerProperties();
    DXMTPresentMetadata metadata = state.metadata;

    // Get back buffer 0 (D3D12 swapchain typically uses a single back buffer
    // with the app managing the index)
    UINT current_index = 0;
    auto *backbuffer = static_cast<D3D12Resource *>(backbuffers_[current_index].ptr());

    // Create dedicated Metal command buffer for presentation
    auto mtl_queue = queue_->GetMTLCommandQueue();
    WMT::CommandBuffer cmd_buf = mtl_queue.commandBuffer();

    // Encode present commands
    WMT::Texture backbuffer_tex = backbuffer->GetTexture()->texture();
    auto drawable = presenter_->encodeCommands(
        cmd_buf, backbuffer_tex, metadata,
        [](WMT::RenderCommandEncoder) { /* no fences */ },
        [](WMT::RenderCommandEncoder) { /* no fences */ }
    );

    // Commit
    cmd_buf.commit();

    // Wait until complete (simple approach for M10; will be refined with
    // actual frame pacing / drawable scheduling in a later milestone)
    cmd_buf.waitUntilCompleted();

    presentation_count_.fetch_add(1);

    TRACE("D3D12SwapChain::Present1: frame ", presentation_count_.load());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::IsTemporaryMonoSupported() {
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetRestrictToOutput(
    void **ppRestrictToOutput) {
    InitReturnPtr(ppRestrictToOutput);
    WARN("D3D12SwapChain::GetRestrictToOutput: stub");
    return DXGI_ERROR_NOT_FOUND;
}

// ── IDXGISwapChain2 ──

HRESULT STDMETHODCALLTYPE D3D12SwapChain::SetSourceSize(
    UINT Width, UINT Height) {
    source_width_ = Width;
    source_height_ = Height;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetSourceSize(
    UINT *pWidth, UINT *pHeight) {
    if (pWidth) *pWidth = source_width_;
    if (pHeight) *pHeight = source_height_;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::SetMaximumFrameLatency(
    UINT MaxLatency) {
    frame_latency_ = MaxLatency;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetMaximumFrameLatency(
    UINT *pMaxLatency) {
    if (pMaxLatency) *pMaxLatency = frame_latency_;
    return S_OK;
}

HANDLE STDMETHODCALLTYPE D3D12SwapChain::GetFrameLatencyWaitableObject() {
    WARN("D3D12SwapChain::GetFrameLatencyWaitableObject: stub");
    return nullptr;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::SetMatrixTransform(
    const void *pMatrix) {
    WARN("D3D12SwapChain::SetMatrixTransform: stub");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::GetMatrixTransform(
    void *pMatrix) {
    if (pMatrix) memset(pMatrix, 0, 16 * sizeof(float));
    return S_OK;
}

// ── IDXGISwapChain3 ──

UINT STDMETHODCALLTYPE D3D12SwapChain::GetCurrentBackBufferIndex() {
    return 0; // simple single-buffer for M10
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::CheckColorSpaceSupport(
    UINT ColorSpace, UINT *pColorSpaceSupport) {
    if (!pColorSpaceSupport)
        return E_POINTER;
    *pColorSpaceSupport = 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::SetColorSpace1(UINT ColorSpace) {
    WARN("D3D12SwapChain::SetColorSpace1: stub");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12SwapChain::ResizeBuffers1(
    UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format,
    UINT SwapChainFlags, const UINT *pCreationNodeMask,
    void *const *ppPresentQueue) {
    return ResizeBuffers(BufferCount, Width, Height, Format, SwapChainFlags);
}

// ── Private helpers ──

void D3D12SwapChain::CreateBackBuffers() {
    UINT buffer_count = desc_.BufferCount;
    if (buffer_count == 0) buffer_count = 1;

    D3D12_RESOURCE_DESC res_desc = {};
    res_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    res_desc.Width = desc_.Width > 0 ? desc_.Width : 1;
    res_desc.Height = desc_.Height > 0 ? desc_.Height : 1;
    res_desc.DepthOrArraySize = 1;
    res_desc.MipLevels = 1;
    res_desc.Format = desc_.Format;
    res_desc.SampleDesc.Count = desc_.SampleDesc.Count > 0 ? desc_.SampleDesc.Count : 1;
    res_desc.SampleDesc.Quality = 0;
    res_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    res_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    if (desc_.BufferUsage & DXGI_USAGE_SHADER_INPUT)
        res_desc.Flags |= D3D12_RESOURCE_FLAG_NONE; // implied
    if (desc_.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
        res_desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    backbuffers_.resize(buffer_count);
    for (UINT i = 0; i < buffer_count; i++) {
        D3D12Resource *res = nullptr;
        HRESULT hr = D3D12Resource::CreateCommitted(
            device_, &heap_props, D3D12_HEAP_FLAG_NONE,
            &res_desc, D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr, &res);

        if (FAILED(hr)) {
            ERR("D3D12SwapChain: Failed to create back buffer ", i);
            backbuffers_[i] = nullptr;
        } else {
            backbuffers_[i] = Com<ID3D12Resource>(res);
        }
    }

    TRACE("Created ", buffer_count, " back buffers: ",
          res_desc.Width, "x", res_desc.Height);
}

void D3D12SwapChain::DestroyBackBuffers() {
    backbuffers_.clear();
}

// ──────────────────────────────────────────────
// Factory functions
// ──────────────────────────────────────────────

HRESULT CreateSwapChainForD3D12(
    IDXGIFactory1 *pFactory,
    D3D12Device *pDevice,
    D3D12CommandQueue *pQueue,
    HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGISwapChain1 **ppSwapChain) {

    InitReturnPtr(ppSwapChain);

    if (!pDevice || !hWnd || !pDesc) {
        return E_INVALIDARG;
    }

    try {
        auto *swapchain = new D3D12SwapChain(
            pDevice, pQueue, hWnd, pDesc, pFullscreenDesc);
        *ppSwapChain = static_cast<IDXGISwapChain1 *>(swapchain);
        return S_OK;
    } catch (const std::exception &e) {
        ERR("CreateSwapChainForD3D12 failed: ", e.what());
        return E_FAIL;
    }
}

HRESULT CreateSwapChainForD3D12(
    D3D12Device *pDevice,
    HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    IDXGISwapChain1 **ppSwapChain) {

    // For testing convenience: create a queue automatically
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.NodeMask = 0;

    Com<ID3D12CommandQueue> queue;
    HRESULT hr = pDevice->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue));
    if (FAILED(hr)) {
        ERR("CreateSwapChainForD3D12: Failed to create command queue");
        return hr;
    }

    auto *d3d12_queue = static_cast<D3D12CommandQueue *>(queue.ptr());
    return CreateSwapChainForD3D12(
        nullptr, pDevice, d3d12_queue, hWnd, pDesc, nullptr, ppSwapChain);
}

} // namespace dxmt::d3d12
