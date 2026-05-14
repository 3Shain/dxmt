#pragma once

#include "d3d12_private.h"
#include "dxmt_device.hpp"
#include "dxmt_command_queue.hpp"
#include "d3d12_shader_manager.hpp"
#include "log/log.hpp"
#include <memory>
#include <atomic>

namespace dxmt::d3d12 {

class D3D12CommandQueue;

class D3D12Device final : public ID3D12Device {
    friend class D3D12CommandQueue;

public:
    D3D12Device(std::unique_ptr<Device> dxmt_device,
                D3D_FEATURE_LEVEL feature_level);

    ~D3D12Device();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── ID3D12Object ──
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) final;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) final;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) final;
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) final;

    // ── ID3D12Device ──
    UINT STDMETHODCALLTYPE GetNodeCount() final;
    HRESULT STDMETHODCALLTYPE CreateCommandQueue(
        const D3D12_COMMAND_QUEUE_DESC *pDesc,
        REFIID riid,
        void **ppCommandQueue) final;
    HRESULT STDMETHODCALLTYPE CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE type,
        REFIID riid,
        void **ppCommandAllocator) final;
    HRESULT STDMETHODCALLTYPE CreateGraphicsPipelineState(
        const void *pDesc,
        REFIID riid,
        void **ppPipelineState) final;
    HRESULT STDMETHODCALLTYPE CreateComputePipelineState(
        const void *pDesc,
        REFIID riid,
        void **ppPipelineState) final;
    HRESULT STDMETHODCALLTYPE CreateCommandList(
        UINT nodeMask,
        D3D12_COMMAND_LIST_TYPE type,
        ID3D12CommandAllocator *pCommandAllocator,
        ID3D12PipelineState *pInitialState,
        REFIID riid,
        void **ppCommandList) final;
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
        D3D12_FEATURE_LEVEL Feature,
        void *pFeatureSupportData,
        UINT FeatureSupportDataSize) final;
    HRESULT STDMETHODCALLTYPE CreateDescriptorHeap(
        const void *pDescriptorHeapDesc,
        REFIID riid,
        void **ppDescriptorHeap) final;
    UINT STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) final;
    HRESULT STDMETHODCALLTYPE CreateRootSignature(
        UINT nodeMask,
        const void *pBlobWithRootSignature,
        SIZE_T blobLengthInBytes,
        REFIID riid,
        void **ppRootSignature) final;
    HRESULT STDMETHODCALLTYPE CreateCommittedResource(
        const void *pHeapProperties,
        D3D12_HEAP_FLAGS HeapFlags,
        const void *pDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const void *pOptimizedClearValue,
        REFIID riid,
        void **ppResource) final;
    HRESULT STDMETHODCALLTYPE CreateHeap(
        const void *pDesc,
        REFIID riid,
        void **ppHeap) final;
    HRESULT STDMETHODCALLTYPE CreatePlacedResource(
        ID3D12Heap *pHeap,
        UINT64 HeapOffset,
        const void *pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const void *pOptimizedClearValue,
        REFIID riid,
        void **ppResource) final;
    HRESULT STDMETHODCALLTYPE CreateReservedResource(
        const void *pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const void *pOptimizedClearValue,
        REFIID riid,
        void **ppResource) final;
    HRESULT STDMETHODCALLTYPE CreateFence(
        UINT64 InitialValue,
        D3D12_FENCE_FLAGS Flags,
        REFIID riid,
        void **ppFence) final;
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() final;
    HRESULT STDMETHODCALLTYPE GetCopyableFootprints(
        const void *pResourceDesc,
        UINT FirstSubresource,
        UINT NumSubresources,
        UINT64 BaseOffset,
        void *pLayouts,
        UINT *pNumRows,
        UINT64 *pRowSizeInBytes,
        UINT64 *pTotalBytes) final;
    HRESULT STDMETHODCALLTYPE CreateQueryHeap(
        const void *pDesc,
        REFIID riid,
        void **ppQueryHeap) final;
    HRESULT STDMETHODCALLTYPE SetStablePowerState(BOOL Enable) final;
    HRESULT STDMETHODCALLTYPE CreateCommandSignature(
        const void *pDesc,
        ID3D12RootSignature *pRootSignature,
        REFIID riid,
        void **ppCommandSignature) final;
    HRESULT STDMETHODCALLTYPE GetResourceTiling(
        ID3D12Resource *pTiledResource,
        UINT *pNumTilesForEntireResource,
        void *pPackedMipDesc,
        void *pStandardTileShapeForNonPackedMips,
        UINT *pNumSubresourceTilings,
        UINT FirstSubresourceTilingToGet,
        void *pSubresourceTilingsForNonPackedMips) final;
    LUID STDMETHODCALLTYPE GetAdapterLuid() final;

    // ── M5: View creation ──
    HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
        ID3D12Resource *pResource,
        const void *pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) final;
    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
        ID3D12Resource *pResource,
        const void *pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) final;

    // ── Internal accessors ──
    Device &GetDXMTDevice() { return *dxmt_device_; }
    WMT::Device GetMTLDevice() { return dxmt_device_->device(); }
    ShaderManager &GetShaderManager() { return *shader_manager_; }
    void RegisterDescriptorHeap(class D3D12DescriptorHeap *heap);
    class D3D12DescriptorHeap *FindDescriptorHeapByHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle);

private:
    std::unique_ptr<Device> dxmt_device_;
    D3D_FEATURE_LEVEL feature_level_;
    std::unique_ptr<ShaderManager> shader_manager_;
    std::vector<class D3D12DescriptorHeap *> descriptor_heaps_;
    std::atomic<ULONG> refcount_;
};

} // namespace dxmt::d3d12
