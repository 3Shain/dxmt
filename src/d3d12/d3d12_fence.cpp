#include "d3d12_fence.hpp"
#include "d3d12_device.hpp"
#include "dxmt_device.hpp"
#include "log/log.hpp"
#include "util_win32_compat.h"

namespace dxmt::d3d12 {

D3D12Fence::D3D12Fence(D3D12Device *device, UINT64 initial_value,
                        D3D12_FENCE_FLAGS flags)
    : device_(device), flags_(flags), refcount_(1),
      completed_value_(initial_value) {
    event_ = device->GetMTLDevice().newSharedEvent();
    event_.signalValue(initial_value);
    TRACE("D3D12Fence created, initial=", initial_value);
}

D3D12Fence::~D3D12Fence() {
    if (pending_event_) CloseHandle(pending_event_);
    TRACE("D3D12Fence destroyed");
}

HRESULT STDMETHODCALLTYPE D3D12Fence::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER; *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12Fence) {
        *ppvObject = static_cast<ID3D12Fence *>(this); AddRef(); return S_OK;
    }
    return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE D3D12Fence::AddRef() { return refcount_.fetch_add(1, std::memory_order_relaxed) + 1; }
ULONG STDMETHODCALLTYPE D3D12Fence::Release() {
    ULONG c = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (c == 0) delete this; return c;
}

UINT64 STDMETHODCALLTYPE D3D12Fence::GetCompletedValue() {
    UINT64 val = event_.signaledValue();
    completed_value_.store(val, std::memory_order_release);
    return val;
}

HRESULT STDMETHODCALLTYPE D3D12Fence::SetEventOnCompletion(UINT64 Value, HANDLE hEvent) {
    std::lock_guard<dxmt::mutex> lock(event_mutex_);
    if (pending_event_) CloseHandle(pending_event_);
    pending_event_ = hEvent;
    pending_value_ = Value;

    // Poll immediately — if already signaled, fire the event
    GetCompletedValue();
    if (completed_value_.load() >= Value && pending_event_) {
        SetEvent(pending_event_);
        pending_event_ = nullptr;
    }

    // For async completion, we'd need a listener thread.
    // M8: fire synchronously if already ready; full async needs MTLSharedEventListener
    TRACE("SetEventOnCompletion: value=", Value, ", immediate=",
          completed_value_.load() >= Value ? "yes" : "deferred");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12Fence::Signal(UINT64 Value) {
    event_.signalValue(Value);
    completed_value_.store(Value, std::memory_order_release);

    // Fire pending event if threshold met
    if (pending_event_ && Value >= pending_value_) {
        SetEvent(pending_event_);
        pending_event_ = nullptr;
    }
    TRACE("Fence::Signal(", Value, ")");
    return S_OK;
}

void D3D12Fence::SignalFromQueue(WMT::CommandBuffer cmdbuf, UINT64 value) {
    cmdbuf.encodeSignalEvent(event_, value);
    completed_value_.store(value, std::memory_order_release);
    TRACE("Fence::SignalFromQueue(", value, ")");
}

void D3D12Fence::WaitFromQueue(WMT::CommandBuffer cmdbuf, UINT64 value) {
    cmdbuf.encodeWaitForEvent(event_, value);
    TRACE("Fence::WaitFromQueue(", value, ")");
}

} // namespace dxmt::d3d12
