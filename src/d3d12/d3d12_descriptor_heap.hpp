#pragma once

#include "d3d12_private.h"
#include "dxmt_buffer.hpp"
#include "rc/util_rc_ptr.hpp"
#include <atomic>
#include <vector>
#include <cstdint>

namespace dxmt::d3d12 {

class D3D12Device;

// Descriptor size constants (D3D12 spec: all 32 bytes for simplicity)
constexpr UINT kDescriptorSize = 32;

class D3D12DescriptorHeap final : public ID3D12DescriptorHeap {
public:
    D3D12DescriptorHeap(D3D12Device *device,
                         const D3D12_DESCRIPTOR_HEAP_DESC *pDesc);
    ~D3D12DescriptorHeap();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── ID3D12Object ──
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) final { return S_OK; }

    // ── ID3D12DescriptorHeap ──
    D3D12_DESCRIPTOR_HEAP_DESC STDMETHODCALLTYPE GetDesc() final;
    D3D12_DESCRIPTOR_HEAP_TYPE GetType() const { return desc_.Type; }
    D3D12_CPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart() final;
    D3D12_GPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart() final;

    // ── Internal descriptor management ──

    // Get raw pointer to a descriptor slot (for CPU-side writes)
    void *GetDescriptorCPU(UINT index);

    // Get GPU virtual address offset for a descriptor (shader-visible only)
    D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorGPU(UINT index);

    // Copy descriptors between heaps
    static void CopyDescriptors(
        ID3D12Device *pDevice,
        UINT NumDstRanges,
        const D3D12_CPU_DESCRIPTOR_HANDLE *pDstRangeStarts,
        const UINT *pDstRangeSizes,
        UINT NumSrcRanges,
        const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcRangeStarts,
        const UINT *pSrcRangeSizes,
        D3D12_DESCRIPTOR_HEAP_TYPE Type);

    // Access the GPU backing buffer (for root signature binding)
    WMT::Buffer GetGPUBackingBuffer() { return gpu_backing_; }
    bool IsShaderVisible() const { return is_shader_visible_; }

    // M5: Associate a resource with a descriptor slot
    void SetResourceForHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle, ID3D12Resource *res);
    ID3D12Resource *GetResourceFromHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle);
    UINT GetNumDescriptors() const { return desc_.NumDescriptors; }

private:
    // Compute byte offset from a CPU handle
    static size_t HandleToOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle);
    static size_t HandleToOffset(D3D12_GPU_DESCRIPTOR_HANDLE handle);

    D3D12Device *device_;
    D3D12_DESCRIPTOR_HEAP_DESC desc_;
    bool is_shader_visible_;
    UINT descriptor_size_;

    // CPU-side descriptor storage (always present)
    std::vector<uint8_t> cpu_descriptors_;

    // M5: Resource pointer storage for RTV/DSV descriptor resolution
    std::vector<ID3D12Resource *> descriptor_resources_;

    // GPU backing for shader-visible heaps
    Rc<Buffer> gpu_backing_buffer_;
    WMT::Buffer gpu_backing_;
    UINT64 gpu_base_va_;

    std::atomic<ULONG> refcount_;
};

} // namespace dxmt::d3d12
