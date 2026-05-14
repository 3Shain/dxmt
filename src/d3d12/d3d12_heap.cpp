#include "d3d12_heap.hpp"
#include "d3d12_device.hpp"
#include "dxmt_device.hpp"
#include "log/log.hpp"

namespace dxmt::d3d12 {

D3D12Heap::D3D12Heap(D3D12Device *device, const D3D12_HEAP_DESC *pDesc)
    : device_(device)
    , desc_(*pDesc)
    , next_free_offset_(0)
    , refcount_(1) {

    WMT::Device mtl_device = device->GetMTLDevice();

    // Create a backing MTLBuffer for the heap
    buffer_ = Rc<Buffer>(new Buffer(pDesc->SizeInBytes, mtl_device));

    WMTResourceOptions storage = WMTResourceStorageModePrivate;
    if (pDesc->Properties.Type == D3D12_HEAP_TYPE_UPLOAD ||
        pDesc->Properties.Type == D3D12_HEAP_TYPE_READBACK) {
        storage = WMTResourceStorageModeShared;
    }

    Flags<BufferAllocationFlag> flags;
    if (storage == WMTResourceStorageModePrivate) {
        flags.set(BufferAllocationFlag::GpuPrivate);
    }

    backing_allocation_ = buffer_->allocate(flags);
    TRACE("D3D12Heap created: ", pDesc->SizeInBytes, " bytes, type=",
          pDesc->Properties.Type == D3D12_HEAP_TYPE_DEFAULT  ? "DEFAULT" :
          pDesc->Properties.Type == D3D12_HEAP_TYPE_UPLOAD   ? "UPLOAD" :
          pDesc->Properties.Type == D3D12_HEAP_TYPE_READBACK ? "READBACK" : "CUSTOM");
}

D3D12Heap::~D3D12Heap() {
    TRACE("D3D12Heap destroyed");
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12Heap::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12Heap) {
        *ppvObject = static_cast<ID3D12Heap *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12Heap::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE D3D12Heap::Release() {
    ULONG c = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (c == 0) delete this;
    return c;
}

HRESULT STDMETHODCALLTYPE D3D12Heap::SetName(LPCWSTR Name) {
    return S_OK;
}

// ── ID3D12Heap ──

D3D12_HEAP_DESC STDMETHODCALLTYPE D3D12Heap::GetDesc() {
    return desc_;
}

// ── Internal: sub-allocate ──

HRESULT D3D12Heap::Suballocate(UINT64 size, UINT64 alignment,
                                Rc<BufferAllocation> *out, UINT64 *out_offset) {
    std::lock_guard<std::mutex> lock(alloc_mutex_);

    // Align the offset
    UINT64 aligned = (next_free_offset_ + alignment - 1) & ~(alignment - 1);

    if (aligned + size > desc_.SizeInBytes) {
        WARN("D3D12Heap::Suballocate: out of space (",
             aligned + size, " > ", desc_.SizeInBytes, ")");
        return E_OUTOFMEMORY;
    }

    // The allocation references the same backing buffer
    // For placed resources, the allocation is just a view into our buffer
    // We return the backing allocation with the offset
    *out = backing_allocation_;
    if (out_offset) *out_offset = aligned;
    next_free_offset_ = aligned + size;

    TRACE("Heap sub-allocated ", size, " bytes at offset ", aligned,
          " (", next_free_offset_, "/", desc_.SizeInBytes, " used)");
    return S_OK;
}

} // namespace dxmt::d3d12
