#pragma once

#include "d3d12_private.h"
#include "dxgi_interfaces.h"
#include "Metal.hpp"
#include <atomic>
#include <memory>
#include <vector>

namespace dxmt {
class Device;
} // namespace dxmt

namespace dxmt::d3d12 {

class D3D12Device;

class D3D12CommandQueue final : public ID3D12CommandQueue, public IMTLDXGIDevice12 {
public:
    D3D12CommandQueue(D3D12Device *device,
                       const D3D12_COMMAND_QUEUE_DESC *pDesc);

    ~D3D12CommandQueue();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── ID3D12Object ──
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) final;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) final;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) final;
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) final;

    // ── ID3D12CommandQueue ──
    void STDMETHODCALLTYPE UpdateTileMappings(
        ID3D12Resource*, UINT, const void*, const void*,
        ID3D12Heap*, UINT, const void*, const UINT*,
        const UINT*, D3D12_TILE_MAPPING_FLAGS) final;
    HRESULT STDMETHODCALLTYPE CopyTileMappings(
        ID3D12Resource*, const void*, ID3D12Resource*,
        const void*, const void*, D3D12_TILE_MAPPING_FLAGS) final;
    HRESULT STDMETHODCALLTYPE ExecuteCommandLists(
        UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists) final;
    void STDMETHODCALLTYPE SetMarker(UINT, const void*, UINT) final;
    void STDMETHODCALLTYPE BeginEvent(UINT, const void*, UINT) final;
    void STDMETHODCALLTYPE EndEvent() final;
    HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence*, UINT64) final;
    HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence*, UINT64) final;
    HRESULT STDMETHODCALLTYPE GetTimestampFrequency(UINT64*) final;
    HRESULT STDMETHODCALLTYPE GetClockCalibration(UINT64*, UINT64*) final;
    void STDMETHODCALLTYPE GetDesc(D3D12_COMMAND_QUEUE_DESC*) final;

    // ── IMTLDXGIDevice12 (M12 DXGI factory integration) ──
    WMT::Device GetMTLDevice();
    HRESULT CreateSwapChain(
        IDXGIFactory1 *pFactory, HWND hWnd,
        const DXGI_SWAP_CHAIN_DESC1 *pDesc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
        IDXGISwapChain1 **ppSwapChain);

    // ── Internal ──
    WMT::CommandQueue GetMTLCommandQueue() { return mtl_queue_; }
    WMT::CommandBuffer CreateCommandBuffer();
    D3D12Device *GetDevice() { return device_; }

private:
    D3D12Device *device_;
    D3D12_COMMAND_QUEUE_DESC desc_;
    WMT::Reference<WMT::CommandQueue> mtl_queue_;
    std::atomic<ULONG> refcount_;
};

} // namespace dxmt::d3d12
