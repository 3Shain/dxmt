#pragma once

#include "d3d12_private.h"
#include <atomic>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace dxmt::d3d12 {

class D3D12Device;

class D3D12CommandAllocator final : public ID3D12CommandAllocator {
public:
    D3D12CommandAllocator(D3D12Device *device, D3D12_COMMAND_LIST_TYPE type);
    ~D3D12CommandAllocator();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── ID3D12Object ──
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) final {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) final {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) final {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) final { return S_OK; }

    // ── ID3D12CommandAllocator ──
    HRESULT STDMETHODCALLTYPE Reset() final;

    // ── Internal: allocate memory for command recording ──
    void *Allocate(size_t size, size_t alignment);

    D3D12_COMMAND_LIST_TYPE GetType() const { return type_; }

private:
    D3D12Device *device_;
    D3D12_COMMAND_LIST_TYPE type_;
    std::atomic<ULONG> refcount_;

    // Simple ring-bump allocator
    static constexpr size_t kDefaultPoolSize = 4 * 1024 * 1024; // 4 MB
    std::vector<uint8_t> pool_;
    size_t offset_ = 0;
};

} // namespace dxmt::d3d12
