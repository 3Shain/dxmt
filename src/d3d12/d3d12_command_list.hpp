#pragma once

#include "d3d12_private.h"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <vector>

namespace dxmt::d3d12 {

class D3D12Device;
class D3D12CommandAllocator;
class D3D12PipelineState;

// ── Command tag IDs ──
enum class CommandTag : uint32_t {
    Nop                = 0,
    DrawInstanced      = 1,
    DrawIndexedInstanced = 2,
    Dispatch           = 3,
    CopyBufferRegion   = 4,
    CopyTextureRegion  = 5,
    CopyResource       = 6,
    ResourceBarrier    = 7,
    SetPipelineState   = 8,
    SetGraphicsRootSig = 9,
    SetComputeRootSig  = 10,
    IASetPrimTopology  = 11,
    IASetVertexBuffers = 12,
    IASetIndexBuffer   = 13,
    OMSetRenderTargets = 14,
    RSSetViewports     = 15,
    RSSetScissorRects  = 16,
    SetDescriptorHeaps = 17,
};

// ── Command buffer header ──
struct CommandHeader {
    CommandTag tag;
    uint32_t data_size;  // bytes following this header
};

class D3D12GraphicsCommandList final : public ID3D12GraphicsCommandList {
public:
    D3D12GraphicsCommandList(D3D12Device *device,
                              D3D12_COMMAND_LIST_TYPE type,
                              D3D12CommandAllocator *allocator);
    ~D3D12GraphicsCommandList();

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

    // ── ID3D12GraphicsCommandList ──

    // Lifecycle
    HRESULT STDMETHODCALLTYPE Close() final;
    HRESULT STDMETHODCALLTYPE Reset(
        ID3D12CommandAllocator *pAllocator,
        ID3D12PipelineState *pInitialState) final;

    // State
    void STDMETHODCALLTYPE SetPipelineState(ID3D12PipelineState *) final;
    void STDMETHODCALLTYPE SetGraphicsRootSignature(ID3D12RootSignature *) final;
    void STDMETHODCALLTYPE SetComputeRootSignature(ID3D12RootSignature *) final;
    void STDMETHODCALLTYPE IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY) final;
    void STDMETHODCALLTYPE IASetVertexBuffers(
        UINT StartSlot, UINT NumViews,
        const D3D12_VERTEX_BUFFER_VIEW *pViews) final;
    void STDMETHODCALLTYPE IASetIndexBuffer(
        const D3D12_INDEX_BUFFER_VIEW *pView) final;
    void STDMETHODCALLTYPE OMSetRenderTargets(
        UINT NumRenderTargetDescriptors,
        const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
        BOOL RTsSingleHandleToDescriptorRange,
        const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor) final;
    void STDMETHODCALLTYPE RSSetViewports(
        UINT NumViewports, const D3D12_VIEWPORT *pViewports) final;
    void STDMETHODCALLTYPE RSSetScissorRects(
        UINT NumRects, const D3D12_RECT *pRects) final;
    void STDMETHODCALLTYPE SetDescriptorHeaps(
        UINT NumHeaps, ID3D12DescriptorHeap *const *ppHeaps) final;

    // Draw / Dispatch
    void STDMETHODCALLTYPE DrawInstanced(
        UINT VertexCount, UINT InstanceCount,
        UINT StartVertex, UINT StartInstance) final;
    void STDMETHODCALLTYPE DrawIndexedInstanced(
        UINT IndexCount, UINT InstanceCount,
        UINT StartIndex, INT BaseVertex, UINT StartInstance) final;
    void STDMETHODCALLTYPE Dispatch(
        UINT X, UINT Y, UINT Z) final;

    // Copy
    void STDMETHODCALLTYPE CopyBufferRegion(
        ID3D12Resource *pDst, UINT64 DstOffset,
        ID3D12Resource *pSrc, UINT64 SrcOffset,
        UINT64 NumBytes) final;
    void STDMETHODCALLTYPE CopyTextureRegion(
        const D3D12_TEXTURE_COPY_LOCATION *pDst,
        UINT DstX, UINT DstY, UINT DstZ,
        const D3D12_TEXTURE_COPY_LOCATION *pSrc,
        const void *pSrcBox) final;
    void STDMETHODCALLTYPE CopyResource(
        ID3D12Resource *pDst, ID3D12Resource *pSrc) final;

    // Barriers
    void STDMETHODCALLTYPE ResourceBarrier(
        UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers) final;

    // ── Internal ──

    bool IsClosed() const { return closed_; }
    bool IsRecording() const { return recording_; }
    D3D12PipelineState *GetCurrentPSO() const { return static_cast<D3D12PipelineState *>(current_pso_); }
    UINT GetDescriptorHeapCount() const { return current_descriptor_heap_count_; }
    ID3D12DescriptorHeap *const *GetDescriptorHeaps() const { return current_descriptor_heaps_; }
    D3D12_COMMAND_LIST_TYPE GetType() const { return type_; }

    // Walk recorded commands during ExecuteCommandLists
    using CommandVisitor = void (*)(CommandTag tag, const void *data,
                                     uint32_t size, void *userdata);
    void WalkCommands(CommandVisitor visitor, void *userdata) const;

private:
    // Allocate space in the command buffer and write header
    void *EmitCommand(CommandTag tag, uint32_t data_size);

    // Validate we're in recording state
    bool CheckRecording();

    D3D12Device *device_;
    D3D12_COMMAND_LIST_TYPE type_;
    D3D12CommandAllocator *current_allocator_;
    std::atomic<ULONG> refcount_;

    // State
    bool closed_ = false;
    bool recording_ = false;

    // Command buffer: [header|data][header|data]...
    // Allocated from the command allocator
    uint8_t *cmd_buffer_ = nullptr;
    size_t cmd_buffer_offset_ = 0;
    size_t cmd_buffer_capacity_ = 0;

    // Tracked state (for validation, not applied until submit)
    ID3D12PipelineState *current_pso_ = nullptr;
    ID3D12RootSignature *current_graphics_rs_ = nullptr;
    ID3D12RootSignature *current_compute_rs_ = nullptr;
    ID3D12DescriptorHeap *current_descriptor_heaps_[2] = {};
    UINT current_descriptor_heap_count_ = 0;
};

} // namespace dxmt::d3d12
