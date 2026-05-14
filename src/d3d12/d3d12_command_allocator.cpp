#include "d3d12_command_allocator.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"

namespace dxmt::d3d12 {

D3D12CommandAllocator::D3D12CommandAllocator(D3D12Device *device,
                                              D3D12_COMMAND_LIST_TYPE type)
    : device_(device)
    , type_(type)
    , refcount_(1)
    , pool_(kDefaultPoolSize, 0) {
    TRACE("D3D12CommandAllocator created. Type: ",
          type == D3D12_COMMAND_LIST_TYPE_DIRECT  ? "DIRECT" :
          type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? "COMPUTE" :
          type == D3D12_COMMAND_LIST_TYPE_COPY    ? "COPY" : "BUNDLE");
}

D3D12CommandAllocator::~D3D12CommandAllocator() {
    TRACE("D3D12CommandAllocator destroyed");
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12CommandAllocator::QueryInterface(
    REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr) return E_POINTER;
    *ppvObject = nullptr;

    if (riid == IID_IUnknown ||
        riid == IID_ID3D12Object ||
        riid == IID_ID3D12CommandAllocator) {
        *ppvObject = static_cast<ID3D12CommandAllocator *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12CommandAllocator::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE D3D12CommandAllocator::Release() {
    ULONG count = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (count == 0) {
        delete this;
    }
    return count;
}

// ── ID3D12CommandAllocator ──

HRESULT STDMETHODCALLTYPE D3D12CommandAllocator::Reset() {
    // Reset the ring-bump: zero offset and zero out the pool
    offset_ = 0;
    // Zero the first page for safety
    if (pool_.size() >= 4096) {
        memset(pool_.data(), 0, 4096);
    }
    TRACE("D3D12CommandAllocator::Reset - pool recycled");
    return S_OK;
}

// ── Internal ──

void *D3D12CommandAllocator::Allocate(size_t size, size_t alignment) {
    // Align offset
    size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);

    if (aligned + size > pool_.size()) {
        // Out of memory in this pool — grow
        size_t new_size = pool_.size() * 2;
        while (aligned + size > new_size) {
            new_size *= 2;
        }
        pool_.resize(new_size, 0);
        ERR("D3D12CommandAllocator: pool exhausted, grown to ", new_size, " bytes");
    }

    void *ptr = pool_.data() + aligned;
    offset_ = aligned + size;
    return ptr;
}

} // namespace dxmt::d3d12
