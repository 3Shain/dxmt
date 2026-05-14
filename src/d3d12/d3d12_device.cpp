#include "d3d12_device.hpp"
#include "d3d12_command_queue.hpp"
#include "d3d12_command_allocator.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_resource.hpp"
#include "d3d12_heap.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_root_signature.hpp"
#include "d3d12_pipeline_state.hpp"
#include "d3d12_fence.hpp"
#include "dxmt_device.hpp"
#include "log/log.hpp"

namespace dxmt::d3d12 {

Logger Logger::s_instance("d3d12.log");

// ═══════════════════════════════════════════════════════════════
// D3D12Device
// ═══════════════════════════════════════════════════════════════

D3D12Device::D3D12Device(std::unique_ptr<Device> dxmt_device,
                         D3D_FEATURE_LEVEL feature_level)
    : dxmt_device_(std::move(dxmt_device))
    , feature_level_(feature_level)
    , shader_manager_(std::make_unique<ShaderManager>(this))
    , refcount_(1) {
    TRACE("D3D12Device created. Feature level: ",
          feature_level_ == D3D_FEATURE_LEVEL_12_0 ? "12.0" :
          feature_level_ == D3D_FEATURE_LEVEL_12_1 ? "12.1" : "12.2");
    TRACE("Metal device: ", dxmt_device_->device().name().getUTF8String());
}

D3D12Device::~D3D12Device() {
    TRACE("D3D12Device destroyed");
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12Device::QueryInterface(
    REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
        riid == IID_ID3D12Device) {
        *ppvObject = static_cast<ID3D12Device *>(this);
        AddRef(); return S_OK;
    }
    WARN("D3D12Device: Unknown interface query");
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12Device::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}
ULONG STDMETHODCALLTYPE D3D12Device::Release() {
    ULONG c = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (c == 0) delete this;
    return c;
}

// ── ID3D12Object ──
HRESULT STDMETHODCALLTYPE D3D12Device::GetPrivateData(
    REFGUID, UINT *, void *) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE D3D12Device::SetPrivateData(
    REFGUID, UINT, const void *) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE D3D12Device::SetPrivateDataInterface(
    REFGUID, const IUnknown *) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE D3D12Device::SetName(LPCWSTR) { return S_OK; }

// ── ID3D12Device ──

UINT STDMETHODCALLTYPE D3D12Device::GetNodeCount() { return 1; }

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandQueue(
    const D3D12_COMMAND_QUEUE_DESC *pDesc,
    REFIID riid, void **ppCommandQueue) {
    if (!pDesc || !ppCommandQueue) return E_INVALIDARG;
    *ppCommandQueue = nullptr;
    auto *queue = new D3D12CommandQueue(this, pDesc);
    HRESULT hr = queue->QueryInterface(riid, ppCommandQueue);
    queue->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE type, REFIID riid, void **ppCommandAllocator) {
    if (!ppCommandAllocator) return E_INVALIDARG;
    *ppCommandAllocator = nullptr;
    auto *allocator = new D3D12CommandAllocator(this, type);
    HRESULT hr = allocator->QueryInterface(riid, ppCommandAllocator);
    allocator->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateGraphicsPipelineState(
    const void *pDesc, REFIID riid, void **ppPipelineState) {
    if (!pDesc || !ppPipelineState) return E_INVALIDARG;
    *ppPipelineState = nullptr;
    D3D12PipelineState *pso = nullptr;
    HRESULT hr = D3D12PipelineState::CreateGraphics(
        this, static_cast<const D3D12_GRAPHICS_PIPELINE_STATE_DESC *>(pDesc), &pso);
    if (FAILED(hr)) return hr;
    hr = pso->QueryInterface(riid, ppPipelineState);
    pso->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateComputePipelineState(
    const void *pDesc, REFIID riid, void **ppPipelineState) {
    if (!pDesc || !ppPipelineState) return E_INVALIDARG;
    *ppPipelineState = nullptr;
    D3D12PipelineState *pso = nullptr;
    HRESULT hr = D3D12PipelineState::CreateCompute(
        this, static_cast<const D3D12_COMPUTE_PIPELINE_STATE_DESC *>(pDesc), &pso);
    if (FAILED(hr)) return hr;
    hr = pso->QueryInterface(riid, ppPipelineState);
    pso->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList(
    UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator *pCommandAllocator,
    ID3D12PipelineState *pInitialState,
    REFIID riid, void **ppCommandList) {
    if (!pCommandAllocator || !ppCommandList) return E_INVALIDARG;
    *ppCommandList = nullptr;
    auto *list = new D3D12GraphicsCommandList(
        this, type, static_cast<D3D12CommandAllocator *>(pCommandAllocator));
    if (pInitialState)
        list->SetPipelineState(pInitialState);
    HRESULT hr = list->QueryInterface(riid, ppCommandList);
    list->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CheckFeatureSupport(
    D3D12_FEATURE_LEVEL Feature, void *pData, UINT Size) {
    return S_OK; // M1: basic support
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateDescriptorHeap(
    const void *pDesc, REFIID riid, void **ppDescriptorHeap) {
    if (!pDesc || !ppDescriptorHeap) return E_INVALIDARG;
    *ppDescriptorHeap = nullptr;
    auto *heap = new D3D12DescriptorHeap(
        this, static_cast<const D3D12_DESCRIPTOR_HEAP_DESC *>(pDesc));
    RegisterDescriptorHeap(heap); // M5
    HRESULT hr = heap->QueryInterface(riid, ppDescriptorHeap);
    heap->Release();
    return hr;
}

UINT STDMETHODCALLTYPE D3D12Device::GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE Type) {
    return 8; // All descriptors are 8 bytes
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateRootSignature(
    UINT nodeMask, const void *pBlob, SIZE_T blobSize,
    REFIID riid, void **ppRootSignature) {
    if (!ppRootSignature) return E_INVALIDARG;
    *ppRootSignature = nullptr;
    auto *rs = new D3D12RootSignature(
        this, static_cast<const D3D12_ROOT_SIGNATURE_DESC *>(pBlob));
    HRESULT hr = rs->QueryInterface(riid, ppRootSignature);
    rs->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource(
    const void *pHeapProps, D3D12_HEAP_FLAGS HeapFlags,
    const void *pDesc, D3D12_RESOURCE_STATES InitialState,
    const void *pClearValue, REFIID riid, void **ppResource) {
    if (!pHeapProps || !pDesc || !ppResource) return E_INVALIDARG;
    *ppResource = nullptr;
    D3D12Resource *res = nullptr;
    HRESULT hr = D3D12Resource::CreateCommitted(
        this,
        static_cast<const D3D12_HEAP_PROPERTIES *>(pHeapProps),
        HeapFlags,
        static_cast<const D3D12_RESOURCE_DESC *>(pDesc),
        InitialState,
        static_cast<const D3D12_CLEAR_VALUE *>(pClearValue),
        &res);
    if (FAILED(hr)) return hr;
    hr = res->QueryInterface(riid, ppResource);
    res->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap(
    const void *pDesc, REFIID riid, void **ppHeap) {
    if (!pDesc || !ppHeap) return E_INVALIDARG;
    *ppHeap = nullptr;
    auto *heap = new D3D12Heap(
        this, static_cast<const D3D12_HEAP_DESC *>(pDesc));
    HRESULT hr = heap->QueryInterface(riid, ppHeap);
    heap->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource(
    ID3D12Heap *pHeap, UINT64 HeapOffset,
    const void *pDesc, D3D12_RESOURCE_STATES InitialState,
    const void *pClearValue, REFIID riid, void **ppResource) {
    if (!pHeap || !pDesc || !ppResource) return E_INVALIDARG;
    *ppResource = nullptr;
    D3D12Resource *res = nullptr;
    HRESULT hr = D3D12Resource::CreatePlaced(
        this, static_cast<D3D12Heap *>(pHeap), HeapOffset,
        static_cast<const D3D12_RESOURCE_DESC *>(pDesc),
        InitialState, &res);
    if (FAILED(hr)) return hr;
    hr = res->QueryInterface(riid, ppResource);
    res->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource(
    const void *pDesc, D3D12_RESOURCE_STATES InitialState,
    const void *pClearValue, REFIID riid, void **ppResource) {
    if (ppResource) *ppResource = nullptr;
    WARN("CreateReservedResource: not implemented");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateFence(
    UINT64 InitialValue, D3D12_FENCE_FLAGS Flags,
    REFIID riid, void **ppFence) {
    if (!ppFence) return E_INVALIDARG;
    *ppFence = nullptr;
    auto *fence = new D3D12Fence(this, InitialValue, Flags);
    HRESULT hr = fence->QueryInterface(riid, ppFence);
    fence->Release();
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::GetDeviceRemovedReason() {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12Device::GetCopyableFootprints(
    const void *pResourceDesc, UINT FirstSubresource, UINT NumSubresources,
    UINT64 BaseOffset, void *pLayouts, UINT *pNumRows,
    UINT64 *pRowSizeInBytes, UINT64 *pTotalBytes) {
    if (!pResourceDesc || NumSubresources == 0) return E_INVALIDARG;
    auto *desc = static_cast<const D3D12_RESOURCE_DESC *>(pResourceDesc);
    UINT64 total = 0;
    auto *layouts = static_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT *>(pLayouts);
    for (UINT i = 0; i < NumSubresources; i++) {
        UINT sub = FirstSubresource + i;
        UINT mip = sub % std::max(desc->MipLevels, (UINT16)1);
        UINT64 w = std::max(desc->Width >> mip, 1ULL);
        UINT h = std::max((UINT64)desc->Height >> mip, 1ULL);
        UINT row_pitch = (UINT)(w * 4);
        UINT64 offset = BaseOffset + total;
        total += row_pitch * h;
        if (layouts) {
            layouts[i].Offset = offset;
            layouts[i].Footprint.Format = desc->Format;
            layouts[i].Footprint.Width = (UINT)w;
            layouts[i].Footprint.Height = (UINT)h;
            layouts[i].Footprint.Depth = 1;
            layouts[i].Footprint.RowPitch = row_pitch;
        }
    }
    if (pNumRows) *pNumRows = 1;
    if (pRowSizeInBytes) *pRowSizeInBytes = total > 0
        ? total / (std::max(desc->Height, (UINT)1) * NumSubresources) : 0;
    if (pTotalBytes) *pTotalBytes = total;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateQueryHeap(
    const void *, REFIID, void **ppQueryHeap) {
    if (ppQueryHeap) *ppQueryHeap = nullptr;
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D12Device::SetStablePowerState(BOOL Enable) {
    return Enable ? E_NOTIMPL : S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandSignature(
    const void *, ID3D12RootSignature *, REFIID, void **ppSig) {
    if (ppSig) *ppSig = nullptr;
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D12Device::GetResourceTiling(
    ID3D12Resource *, UINT *, void *, void *,
    UINT *, UINT, void *) {
    return E_NOTIMPL;
}

LUID STDMETHODCALLTYPE D3D12Device::GetAdapterLuid() {
    LUID luid = {};
    return luid;
}

// ── M5: CreateRenderTargetView / CreateDepthStencilView ──

HRESULT STDMETHODCALLTYPE D3D12Device::CreateRenderTargetView(
    ID3D12Resource *pResource, const void *,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    if (!pResource || DestDescriptor.ptr == 0) return E_INVALIDARG;
    auto *heap = FindDescriptorHeapByHandle(DestDescriptor);
    if (!heap) {
        ERR("CreateRenderTargetView: cannot find descriptor heap");
        return E_INVALIDARG;
    }
    heap->SetResourceForHandle(DestDescriptor, pResource);
    TRACE("CreateRenderTargetView: res=", (void*)pResource);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateDepthStencilView(
    ID3D12Resource *pResource, const void *,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    if (!pResource || DestDescriptor.ptr == 0) return E_INVALIDARG;
    auto *heap = FindDescriptorHeapByHandle(DestDescriptor);
    if (!heap) {
        ERR("CreateDepthStencilView: cannot find descriptor heap");
        return E_INVALIDARG;
    }
    heap->SetResourceForHandle(DestDescriptor, pResource);
    TRACE("CreateDepthStencilView: res=", (void*)pResource);
    return S_OK;
}

// ── M5: Descriptor heap registry ──

void D3D12Device::RegisterDescriptorHeap(D3D12DescriptorHeap *heap) {
    descriptor_heaps_.push_back(heap);
}

D3D12DescriptorHeap *D3D12Device::FindDescriptorHeapByHandle(
    D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    for (auto *heap : descriptor_heaps_) {
        D3D12_CPU_DESCRIPTOR_HANDLE start =
            heap->GetCPUDescriptorHandleForHeapStart();
        UINT size = heap->GetNumDescriptors() * kDescriptorSize;
        if (handle.ptr >= start.ptr && handle.ptr < start.ptr + size) {
            return heap;
        }
    }
    return nullptr;
}

} // namespace dxmt::d3d12
