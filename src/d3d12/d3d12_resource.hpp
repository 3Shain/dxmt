#pragma once

#include "d3d12_private.h"
#include "dxmt_buffer.hpp"
#include "dxmt_texture.hpp"
#include "rc/util_rc_ptr.hpp"
#include <atomic>
#include <memory>

namespace dxmt::d3d12 {

class D3D12Device;
class D3D12Heap;

class D3D12Resource final : public ID3D12Resource {
public:
    // Factory: committed resource (device-owns the backing memory)
    static HRESULT CreateCommitted(
        D3D12Device *device,
        const D3D12_HEAP_PROPERTIES *pHeapProperties,
        D3D12_HEAP_FLAGS HeapFlags,
        const D3D12_RESOURCE_DESC *pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const D3D12_CLEAR_VALUE *pOptimizedClearValue,
        D3D12Resource **out);

    // Factory: placed resource (sub-allocated from a heap)
    static HRESULT CreatePlaced(
        D3D12Device *device,
        D3D12Heap *heap,
        UINT64 HeapOffset,
        const D3D12_RESOURCE_DESC *pDesc,
        D3D12_RESOURCE_STATES InitialState,
        D3D12Resource **out);

    ~D3D12Resource();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── ID3D12Object ──
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) final;

    // ── ID3D12Resource ──
    HRESULT STDMETHODCALLTYPE Map(UINT Subresource, const D3D12_RANGE *pReadRange,
                                   void **ppData) final;
    void STDMETHODCALLTYPE Unmap(UINT Subresource,
                                  const D3D12_RANGE *pWrittenRange) final;
    D3D12_RESOURCE_DESC STDMETHODCALLTYPE GetDesc() final;
    D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress() final;
    HRESULT STDMETHODCALLTYPE WriteToSubresource(
        UINT DstSubresource, const D3D12_BOX *pDstBox,
        const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch) final;
    HRESULT STDMETHODCALLTYPE ReadFromSubresource(
        void *pDstData, UINT DstRowPitch, UINT DstDepthPitch,
        UINT SrcSubresource, const D3D12_BOX *pSrcBox) final;
    HRESULT STDMETHODCALLTYPE GetHeapProperties(
        D3D12_HEAP_PROPERTIES *pHeapProperties,
        D3D12_HEAP_FLAGS *pHeapFlags) final;

    // ── Internal accessors ──
    bool IsBuffer() const { return is_buffer_; }
    Rc<Buffer> GetBuffer() { return buffer_; }
    Rc<Texture> GetTexture() { return texture_; }
    Rc<BufferAllocation> GetAllocation() { return allocation_; }
    D3D12_RESOURCE_STATES GetCurrentState() const { return current_state_; }

private:
    D3D12Resource(D3D12Device *device, const D3D12_RESOURCE_DESC &desc);

    D3D12Device *device_;
    D3D12_RESOURCE_DESC desc_;
    D3D12_RESOURCE_STATES current_state_;
    D3D12_HEAP_PROPERTIES heap_properties_;
    D3D12_HEAP_FLAGS heap_flags_;
    bool is_buffer_;

    // One of these is populated:
    Rc<Buffer> buffer_;
    Rc<Texture> texture_;
    Rc<BufferAllocation> allocation_;  // the actual allocation

    std::atomic<ULONG> refcount_;

    // Map tracking
    bool mapped_ = false;
    UINT mapped_subresource_ = 0;
    void *mapped_data_ = nullptr;
};

} // namespace dxmt::d3d12
