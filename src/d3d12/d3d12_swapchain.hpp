#pragma once

#include "d3d12_private.h"
#include "dxmt_presenter.hpp"
#include "Metal.hpp"
#include "com/com_pointer.hpp"
#include "rc/util_rc_ptr.hpp"
#include "log/log.hpp"
#include <atomic>
#include <vector>

namespace dxmt::d3d12 {

class D3D12Device;
class D3D12CommandQueue;

class D3D12SwapChain final : public IDXGISwapChain3 {
public:
    D3D12SwapChain(
        D3D12Device *device,
        D3D12CommandQueue *queue,
        HWND hWnd,
        const DXGI_SWAP_CHAIN_DESC1 *pDesc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc);

    ~D3D12SwapChain();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── IDXGISwapChain ──
    HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) final;
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void **ppSurface) final;
    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, void *pTarget) final;
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL *pFullscreen, void **ppTarget) final;
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc) final;
    HRESULT STDMETHODCALLTYPE ResizeBuffers(
        UINT BufferCount, UINT Width, UINT Height,
        DXGI_FORMAT NewFormat, UINT SwapChainFlags) final;
    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters) final;
    HRESULT STDMETHODCALLTYPE GetContainingOutput(void **ppOutput) final;
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) final;
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) final;

    // ── IDXGISwapChain1 ──
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) final;
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) final;
    HRESULT STDMETHODCALLTYPE GetHwnd(HWND *pHwnd) final;
    HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void **ppUnk) final;
    HRESULT STDMETHODCALLTYPE Present1(
        UINT SyncInterval, UINT PresentFlags,
        const void *pPresentParameters) final;
    HRESULT STDMETHODCALLTYPE IsTemporaryMonoSupported() final;
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(void **ppRestrictToOutput) final;

    // ── IDXGISwapChain2 ──
    HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) final;
    HRESULT STDMETHODCALLTYPE GetSourceSize(UINT *pWidth, UINT *pHeight) final;
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) final;
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) final;
    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() final;
    HRESULT STDMETHODCALLTYPE SetMatrixTransform(const void *pMatrix) final;
    HRESULT STDMETHODCALLTYPE GetMatrixTransform(void *pMatrix) final;

    // ── IDXGISwapChain3 ──
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() final;
    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(
        UINT ColorSpace, UINT *pColorSpaceSupport) final;
    HRESULT STDMETHODCALLTYPE SetColorSpace1(UINT ColorSpace) final;
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(
        UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format,
        UINT SwapChainFlags, const UINT *pCreationNodeMask,
        void *const *ppPresentQueue) final;

private:
    void CreateBackBuffers();
    void DestroyBackBuffers();

    D3D12Device *device_;
    D3D12CommandQueue *queue_;
    HWND hWnd_;
    DXGI_SWAP_CHAIN_DESC1 desc_;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc_;
    std::atomic<ULONG> refcount_;
    std::atomic<UINT> presentation_count_{0};
    UINT frame_latency_{1};

    // WSI / Metal
    obj_handle_t native_view_{};
    WMT::MetalLayer layer_;

    // Presenter
    Rc<Presenter> presenter_;

    // Back buffers
    std::vector<Com<ID3D12Resource>> backbuffers_;

    // Source size (IDXGISwapChain2)
    UINT source_width_{0};
    UINT source_height_{0};
};

// ── Factory (called by DXGI factory) ──

HRESULT CreateSwapChainForD3D12(
    IDXGIFactory1 *pFactory,
    D3D12Device *pDevice,
    D3D12CommandQueue *pQueue,
    HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGISwapChain1 **ppSwapChain);

// ── Convenience: create swapchain from device (for testing) ──

HRESULT CreateSwapChainForD3D12(
    D3D12Device *pDevice,
    HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    IDXGISwapChain1 **ppSwapChain);

} // namespace dxmt::d3d12
