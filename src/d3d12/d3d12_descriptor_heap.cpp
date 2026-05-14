#include "d3d12_descriptor_heap.hpp"
#include "d3d12_device.hpp"
#include "dxmt_device.hpp"
#include "log/log.hpp"
#include <cstring>

namespace dxmt::d3d12 {

D3D12DescriptorHeap::D3D12DescriptorHeap(
    D3D12Device *device,
    const D3D12_DESCRIPTOR_HEAP_DESC *pDesc)
    : device_(device)
    , desc_(*pDesc)
    , is_shader_visible_((pDesc->Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0)
    , descriptor_size_(kDescriptorSize)
    , refcount_(1) {

    size_t total_size = static_cast<size_t>(desc_.NumDescriptors) * descriptor_size_;
    cpu_descriptors_.resize(total_size, 0);

    // M5: Initialize resource pointers (for RTV/DSV)
    if (desc_.Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV ||
        desc_.Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
        descriptor_resources_.resize(desc_.NumDescriptors, nullptr);
    }

    if (is_shader_visible_) {
        // Allocate GPU backing buffer for shader-visible descriptors
        WMT::Device mtl_device = device->GetMTLDevice();
        gpu_backing_buffer_ = Rc<Buffer>(new Buffer(total_size, mtl_device));
        auto alloc = gpu_backing_buffer_->allocate(BufferAllocationFlag::GpuPrivate);
        gpu_backing_ = alloc->buffer();
        gpu_base_va_ = alloc->gpuAddress();
        TRACE("Shader-visible heap: GPU backing ",
              total_size, " bytes, VA=0x", (void*)gpu_base_va_);
    }

    TRACE("D3D12DescriptorHeap created: type=",
          desc_.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? "CBV_SRV_UAV" :
          desc_.Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER     ? "SAMPLER" :
          desc_.Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV         ? "RTV" : "DSV",
          ", count=", desc_.NumDescriptors,
          is_shader_visible_ ? ", SHADER_VISIBLE" : "");
}

D3D12DescriptorHeap::~D3D12DescriptorHeap() {
    TRACE("D3D12DescriptorHeap destroyed");
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12DescriptorHeap::QueryInterface(
    REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
        riid == IID_ID3D12DescriptorHeap) {
        *ppvObject = static_cast<ID3D12DescriptorHeap *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12DescriptorHeap::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE D3D12DescriptorHeap::Release() {
    ULONG c = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (c == 0) delete this;
    return c;
}

// ── ID3D12DescriptorHeap ──

D3D12_DESCRIPTOR_HEAP_DESC STDMETHODCALLTYPE D3D12DescriptorHeap::GetDesc() {
    return desc_;
}

D3D12_CPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE
D3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart() {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = {};
    handle.ptr = reinterpret_cast<SIZE_T>(cpu_descriptors_.data());
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE STDMETHODCALLTYPE
D3D12DescriptorHeap::GetGPUDescriptorHandleForHeapStart() {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
    if (is_shader_visible_) {
        handle.ptr = gpu_base_va_;
    }
    return handle;
}

// ── Internal ──

void *D3D12DescriptorHeap::GetDescriptorCPU(UINT index) {
    if (index >= desc_.NumDescriptors) return nullptr;
    return cpu_descriptors_.data() + (static_cast<size_t>(index) * descriptor_size_);
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetDescriptorGPU(UINT index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
    if (is_shader_visible_ && index < desc_.NumDescriptors) {
        handle.ptr = gpu_base_va_ + (static_cast<UINT64>(index) * descriptor_size_);
    }
    return handle;
}

size_t D3D12DescriptorHeap::HandleToOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    return handle.ptr;
}

size_t D3D12DescriptorHeap::HandleToOffset(D3D12_GPU_DESCRIPTOR_HANDLE handle) {
    return static_cast<size_t>(handle.ptr);
}

// ── CopyDescriptors ──

void D3D12DescriptorHeap::CopyDescriptors(
    ID3D12Device *pDevice,
    UINT NumDstRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE *pDstRangeStarts,
    const UINT *pDstRangeSizes,
    UINT NumSrcRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcRangeStarts,
    const UINT *pSrcRangeSizes,
    D3D12_DESCRIPTOR_HEAP_TYPE Type) {

    if (!pDstRangeStarts || !pSrcRangeStarts) return;

    UINT src_idx = 0;
    UINT dst_idx = 0;
    UINT src_range_idx = 0;
    UINT dst_range_idx = 0;
    UINT src_copied = 0;
    UINT dst_copied = 0;

    UINT src_range_size = NumSrcRanges > 0 && pSrcRangeSizes
        ? pSrcRangeSizes[0] : UINT(-1);
    UINT dst_range_size = NumDstRanges > 0 && pDstRangeSizes
        ? pDstRangeSizes[0] : UINT(-1);

    while (src_range_idx < NumSrcRanges && dst_range_idx < NumDstRanges) {
        UINT copy_count = std::min(src_range_size - src_copied,
                                    dst_range_size - dst_copied);

        if (copy_count == 0) break;

        SIZE_T src_base = pSrcRangeStarts[src_range_idx].ptr +
                          src_copied * kDescriptorSize;
        SIZE_T dst_base = pDstRangeStarts[dst_range_idx].ptr +
                          dst_copied * kDescriptorSize;

        memcpy(reinterpret_cast<void *>(dst_base),
               reinterpret_cast<const void *>(src_base),
               copy_count * kDescriptorSize);

        TRACE("CopyDescriptors: ", copy_count, " descriptors");

        src_copied += copy_count;
        dst_copied += copy_count;

        // Advance ranges
        while (src_range_idx < NumSrcRanges && src_copied >= src_range_size) {
            src_range_idx++;
            src_copied = 0;
            if (src_range_idx < NumSrcRanges && pSrcRangeSizes)
                src_range_size = pSrcRangeSizes[src_range_idx];
        }

        while (dst_range_idx < NumDstRanges && dst_copied >= dst_range_size) {
            dst_range_idx++;
            dst_copied = 0;
            if (dst_range_idx < NumDstRanges && pDstRangeSizes)
                dst_range_size = pDstRangeSizes[dst_range_idx];
        }
    }
}

    // ── M5: Resource pointer tracking ──

    void D3D12DescriptorHeap::SetResourceForHandle(
        D3D12_CPU_DESCRIPTOR_HANDLE handle, ID3D12Resource *res) {
        SIZE_T base = reinterpret_cast<SIZE_T>(cpu_descriptors_.data());
        size_t offset = handle.ptr - base;
        size_t index = offset / descriptor_size_;
        if (index < descriptor_resources_.size()) {
            descriptor_resources_[index] = res;
            TRACE("SetResourceForHandle: idx=", index, " res=", (void*)res);
        } else {
            WARN("SetResourceForHandle: index ", index,
                 " out of range (max ", descriptor_resources_.size(), ")");
        }
    }

    ID3D12Resource *D3D12DescriptorHeap::GetResourceFromHandle(
        D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        SIZE_T base = reinterpret_cast<SIZE_T>(cpu_descriptors_.data());
        size_t offset = handle.ptr - base;
        size_t index = offset / descriptor_size_;
        if (index < descriptor_resources_.size()) {
            return descriptor_resources_[index];
        }
        return nullptr;
    }

} // namespace dxmt::d3d12
