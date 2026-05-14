#pragma once
#include "d3d12_private.h"
#include "Metal.hpp"
#include "thread.hpp"
#include <atomic>

namespace dxmt::d3d12 {

class D3D12Device;

class D3D12Fence final : public ID3D12Fence {
public:
    D3D12Fence(D3D12Device *device, UINT64 initial_value, D3D12_FENCE_FLAGS flags);
    ~D3D12Fence();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) final { return S_OK; }

    UINT64 STDMETHODCALLTYPE GetCompletedValue() final;
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 Value, HANDLE hEvent) final;
    HRESULT STDMETHODCALLTYPE Signal(UINT64 Value) final;

    WMT::SharedEvent GetMTLEvent() { return event_; }
    void SignalFromQueue(WMT::CommandBuffer cmdbuf, UINT64 value);
    void WaitFromQueue(WMT::CommandBuffer cmdbuf, UINT64 value);

private:
    D3D12Device *device_;
    WMT::Reference<WMT::SharedEvent> event_;
    D3D12_FENCE_FLAGS flags_;
    std::atomic<ULONG> refcount_;
    std::atomic<UINT64> completed_value_;
    dxmt::mutex event_mutex_;
    HANDLE pending_event_ = nullptr;
    UINT64 pending_value_ = 0;
};

} // namespace dxmt::d3d12
