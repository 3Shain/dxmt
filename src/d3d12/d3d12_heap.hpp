#pragma once

#include "d3d12_private.h"
#include "dxmt_buffer.hpp"
#include "rc/util_rc_ptr.hpp"
#include <atomic>
#include <mutex>

namespace dxmt::d3d12 {

class D3D12Device;

class D3D12Heap final : public ID3D12Heap {
public:
    D3D12Heap(D3D12Device *device, const D3D12_HEAP_DESC *pDesc);
    ~D3D12Heap();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── ID3D12Object ──
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) final;

    // ── ID3D12Heap ──
    D3D12_HEAP_DESC STDMETHODCALLTYPE GetDesc() final;

    // ── Internal: sub-allocate a region ──
    HRESULT Suballocate(UINT64 size, UINT64 alignment,
                         Rc<BufferAllocation> *out, UINT64 *out_offset);

    Rc<Buffer> GetBackingBuffer() { return buffer_; }
    D3D12_HEAP_PROPERTIES GetProperties() const { return desc_.Properties; }

private:
    D3D12Device *device_;
    D3D12_HEAP_DESC desc_;
    Rc<Buffer> buffer_;
    Rc<BufferAllocation> backing_allocation_;
    UINT64 next_free_offset_;
    std::mutex alloc_mutex_;
    std::atomic<ULONG> refcount_;
};

} // namespace dxmt::d3d12
