#include "d3d12_command_list.hpp"
#include "d3d12_command_allocator.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include <cstring>

namespace dxmt::d3d12 {

D3D12GraphicsCommandList::D3D12GraphicsCommandList(
    D3D12Device *device,
    D3D12_COMMAND_LIST_TYPE type,
    D3D12CommandAllocator *allocator)
    : device_(device)
    , type_(type)
    , current_allocator_(allocator)
    , refcount_(1) {

    // Allocate initial command buffer from the allocator
    cmd_buffer_capacity_ = 64 * 1024; // 64 KB initial
    cmd_buffer_ = static_cast<uint8_t *>(
        allocator->Allocate(cmd_buffer_capacity_, 16));
    cmd_buffer_offset_ = 0;
    recording_ = true;

    TRACE("D3D12GraphicsCommandList created. Type: ",
          type == D3D12_COMMAND_LIST_TYPE_DIRECT  ? "DIRECT" :
          type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? "COMPUTE" :
          type == D3D12_COMMAND_LIST_TYPE_COPY    ? "COPY" : "BUNDLE");
}

D3D12GraphicsCommandList::~D3D12GraphicsCommandList() {
    TRACE("D3D12GraphicsCommandList destroyed");
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::QueryInterface(
    REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr) return E_POINTER;
    *ppvObject = nullptr;

    if (riid == IID_IUnknown ||
        riid == IID_ID3D12Object ||
        riid == IID_ID3D12GraphicsCommandList) {
        *ppvObject = static_cast<ID3D12GraphicsCommandList *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12GraphicsCommandList::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE D3D12GraphicsCommandList::Release() {
    ULONG count = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (count == 0) {
        delete this;
    }
    return count;
}

// ── Lifecycle ──

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::Close() {
    if (!recording_) {
        WARN("Close: list is not in recording state");
        return E_FAIL;
    }
    recording_ = false;
    closed_ = true;
    TRACE("D3D12GraphicsCommandList::Close - ", cmd_buffer_offset_,
          " bytes of commands recorded");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::Reset(
    ID3D12CommandAllocator *pAllocator,
    ID3D12PipelineState *pInitialState) {

    if (pAllocator == nullptr) {
        return E_INVALIDARG;
    }

    if (recording_) {
        WARN("Reset: list is still recording, close it first");
        return E_FAIL;
    }

    auto *alloc = static_cast<D3D12CommandAllocator *>(pAllocator);
    current_allocator_ = alloc;
    current_pso_ = static_cast<D3D12PipelineState *>(pInitialState);

    // Allocate new command buffer
    cmd_buffer_capacity_ = 64 * 1024;
    cmd_buffer_ = static_cast<uint8_t *>(
        alloc->Allocate(cmd_buffer_capacity_, 16));
    cmd_buffer_offset_ = 0;

    // Reset tracked state
    current_graphics_rs_ = nullptr;
    current_compute_rs_ = nullptr;
    current_descriptor_heap_count_ = 0;

    recording_ = true;
    closed_ = false;

    TRACE("D3D12GraphicsCommandList::Reset");
    return S_OK;
}

// ── Internal command emission ──

void *D3D12GraphicsCommandList::EmitCommand(CommandTag tag,
                                              uint32_t data_size) {
    size_t total = sizeof(CommandHeader) + data_size;
    if (cmd_buffer_offset_ + total > cmd_buffer_capacity_) {
        // Grow buffer
        size_t new_cap = cmd_buffer_capacity_ * 2;
        while (cmd_buffer_offset_ + total > new_cap)
            new_cap *= 2;
        uint8_t *new_buf = static_cast<uint8_t *>(
            current_allocator_->Allocate(new_cap, 16));
        memcpy(new_buf, cmd_buffer_, cmd_buffer_offset_);
        cmd_buffer_ = new_buf;
        cmd_buffer_capacity_ = new_cap;
        TRACE("Command buffer grown to ", new_cap, " bytes");
    }

    auto *hdr = reinterpret_cast<CommandHeader *>(
        cmd_buffer_ + cmd_buffer_offset_);
    hdr->tag = tag;
    hdr->data_size = data_size;
    cmd_buffer_offset_ += sizeof(CommandHeader);
    void *data = cmd_buffer_ + cmd_buffer_offset_;
    cmd_buffer_offset_ += data_size;
    return data;
}

bool D3D12GraphicsCommandList::CheckRecording() {
    if (!recording_) {
        WARN("Command emitted on non-recording command list");
        return false;
    }
    return true;
}

// ── Walk recorded commands ──

void D3D12GraphicsCommandList::WalkCommands(
    CommandVisitor visitor, void *userdata) const {
    size_t offset = 0;
    while (offset < cmd_buffer_offset_) {
        auto *hdr = reinterpret_cast<const CommandHeader *>(
            cmd_buffer_ + offset);
        offset += sizeof(CommandHeader);
        const void *data = cmd_buffer_ + offset;
        visitor(hdr->tag, data, hdr->data_size, userdata);
        offset += hdr->data_size;
    }
}

// ── State commands ──

void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPipelineState(
    ID3D12PipelineState *pPSO) {
    if (!CheckRecording()) return;
    current_pso_ = static_cast<D3D12PipelineState *>(pPSO);
    EmitCommand(CommandTag::SetPipelineState, 0);
    TRACE("  SetPipelineState recorded");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootSignature(
    ID3D12RootSignature *pRS) {
    if (!CheckRecording()) return;
    current_graphics_rs_ = pRS;
    EmitCommand(CommandTag::SetGraphicsRootSig, 0);
    TRACE("  SetGraphicsRootSignature recorded");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootSignature(
    ID3D12RootSignature *pRS) {
    if (!CheckRecording()) return;
    current_compute_rs_ = pRS;
    EmitCommand(CommandTag::SetComputeRootSig, 0);
    TRACE("  SetComputeRootSignature recorded");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetPrimitiveTopology(
    D3D12_PRIMITIVE_TOPOLOGY Topology) {
    if (!CheckRecording()) return;
    struct Data { D3D12_PRIMITIVE_TOPOLOGY topo; };
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::IASetPrimTopology, sizeof(Data)));
    d->topo = Topology;
    TRACE("  IASetPrimitiveTopology recorded (", Topology, ")");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetVertexBuffers(
    UINT StartSlot, UINT NumViews,
    const D3D12_VERTEX_BUFFER_VIEW *pViews) {
    if (!CheckRecording()) return;
    UINT n = NumViews < 32 ? NumViews : 32;
    struct Data { UINT start; UINT count;
                  D3D12_VERTEX_BUFFER_VIEW views[32];
                  ID3D12Resource *resources[32]; };
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::IASetVertexBuffers,
                     offsetof(Data, views) + n * sizeof(D3D12_VERTEX_BUFFER_VIEW)
                     + n * sizeof(ID3D12Resource*)));
    d->start = StartSlot;
    d->count = n;
    for (UINT i = 0; i < n && pViews; i++) {
        d->views[i] = pViews[i];
        // Resolve GPU VA to resource (M5: store in command data)
        d->resources[i] = nullptr; // M5: resolved at dispatch via device VA map
    }
    TRACE("  IASetVertexBuffers recorded (", n, " views)");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetIndexBuffer(
    const D3D12_INDEX_BUFFER_VIEW *pView) {
    if (!CheckRecording()) return;
    struct Data { D3D12_INDEX_BUFFER_VIEW view; };
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::IASetIndexBuffer, sizeof(Data)));
    if (pView) d->view = *pView;
    TRACE("  IASetIndexBuffer recorded");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetRenderTargets(
    UINT NumRT, const D3D12_CPU_DESCRIPTOR_HANDLE *pRTs,
    BOOL RTSingleHandle, const D3D12_CPU_DESCRIPTOR_HANDLE *pDSV) {
    if (!CheckRecording()) return;
    UINT n = NumRT < 8 ? NumRT : 8;
    struct Data { UINT num_rt; BOOL single_handle;
                  D3D12_CPU_DESCRIPTOR_HANDLE rt_handles[8];
                  D3D12_CPU_DESCRIPTOR_HANDLE ds; };
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::OMSetRenderTargets,
                     offsetof(Data, rt_handles) + n * sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)));
    d->num_rt = n;
    d->single_handle = RTSingleHandle;
    for (UINT i = 0; i < n && pRTs; i++)
        d->rt_handles[i] = RTSingleHandle ? pRTs[0] : pRTs[i];
    if (pDSV) d->ds = *pDSV;
    TRACE("  OMSetRenderTargets recorded (", n, " RTs)");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetViewports(
    UINT NumViewports, const D3D12_VIEWPORT *pViewports) {
    if (!CheckRecording()) return;
    struct Data { UINT count; D3D12_VIEWPORT vps[16]; };
    UINT n = NumViewports < 16 ? NumViewports : 16;
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::RSSetViewports,
                     offsetof(Data, vps) + n * sizeof(D3D12_VIEWPORT)));
    d->count = n;
    if (pViewports) memcpy(d->vps, pViewports, n * sizeof(D3D12_VIEWPORT));
    TRACE("  RSSetViewports recorded (", n, " viewports)");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetScissorRects(
    UINT NumRects, const D3D12_RECT *pRects) {
    if (!CheckRecording()) return;
    struct Data { UINT count; D3D12_RECT rects[16]; };
    UINT n = NumRects < 16 ? NumRects : 16;
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::RSSetScissorRects,
                     offsetof(Data, rects) + n * sizeof(D3D12_RECT)));
    d->count = n;
    if (pRects) memcpy(d->rects, pRects, n * sizeof(D3D12_RECT));
    TRACE("  RSSetScissorRects recorded (", n, " rects)");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetDescriptorHeaps(
    UINT NumHeaps, ID3D12DescriptorHeap *const *ppHeaps) {
    if (!CheckRecording()) return;
    current_descriptor_heap_count_ = NumHeaps < 2 ? NumHeaps : 2;
    for (UINT i = 0; i < current_descriptor_heap_count_; i++) {
        current_descriptor_heaps_[i] =
            static_cast<ID3D12DescriptorHeap *>(ppHeaps[i]);
    }
    EmitCommand(CommandTag::SetDescriptorHeaps, 0);
    TRACE("  SetDescriptorHeaps recorded (", NumHeaps, " heaps)");
}

// ── Draw / Dispatch ──

void STDMETHODCALLTYPE D3D12GraphicsCommandList::DrawInstanced(
    UINT VertexCount, UINT InstanceCount,
    UINT StartVertex, UINT StartInstance) {
    if (!CheckRecording()) return;
    struct Data { UINT vc, ic, sv, si; };
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::DrawInstanced, sizeof(Data)));
    d->vc = VertexCount; d->ic = InstanceCount;
    d->sv = StartVertex; d->si = StartInstance;
    TRACE("  DrawInstanced recorded (", VertexCount, " verts)");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::DrawIndexedInstanced(
    UINT IndexCount, UINT InstanceCount,
    UINT StartIndex, INT BaseVertex, UINT StartInstance) {
    if (!CheckRecording()) return;
    struct Data { UINT ic, instc, si; INT bv; UINT si2; };
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::DrawIndexedInstanced, sizeof(Data)));
    d->ic = IndexCount; d->instc = InstanceCount;
    d->si = StartIndex; d->bv = BaseVertex; d->si2 = StartInstance;
    TRACE("  DrawIndexedInstanced recorded (", IndexCount, " indices)");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::Dispatch(
    UINT X, UINT Y, UINT Z) {
    if (!CheckRecording()) return;
    struct Data { UINT x, y, z; };
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::Dispatch, sizeof(Data)));
    d->x = X; d->y = Y; d->z = Z;
    TRACE("  Dispatch recorded (", X, ", ", Y, ", ", Z, ")");
}

// ── Copy ──

void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyBufferRegion(
    ID3D12Resource *pDst, UINT64 DstOffset,
    ID3D12Resource *pSrc, UINT64 SrcOffset, UINT64 NumBytes) {
    if (!CheckRecording()) return;
    struct Data { UINT64 dst_off, src_off, num_bytes; };
    auto *d = static_cast<Data *>(
        EmitCommand(CommandTag::CopyBufferRegion, sizeof(Data)));
    d->dst_off = DstOffset; d->src_off = SrcOffset;
    d->num_bytes = NumBytes;
    // Track resources via AddRef/Release on the COM objects
    if (pDst) pDst->AddRef();
    if (pSrc) pSrc->AddRef();
    TRACE("  CopyBufferRegion recorded");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyTextureRegion(
    const D3D12_TEXTURE_COPY_LOCATION *pDst,
    UINT DstX, UINT DstY, UINT DstZ,
    const D3D12_TEXTURE_COPY_LOCATION *pSrc,
    const void *pSrcBox) {
    if (!CheckRecording()) return;
    EmitCommand(CommandTag::CopyTextureRegion, 0);
    TRACE("  CopyTextureRegion recorded (stub)");
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyResource(
    ID3D12Resource *pDst, ID3D12Resource *pSrc) {
    if (!CheckRecording()) return;
    EmitCommand(CommandTag::CopyResource, 0);
    if (pDst) pDst->AddRef();
    if (pSrc) pSrc->AddRef();
    TRACE("  CopyResource recorded (stub)");
}

// ── Barriers ──

void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResourceBarrier(
    UINT NumBarriers, const D3D12_RESOURCE_BARRIER *pBarriers) {
    if (!CheckRecording()) return;
    EmitCommand(CommandTag::ResourceBarrier, 0);
    TRACE("  ResourceBarrier recorded (", NumBarriers, " barriers, stub)");
}

} // namespace dxmt::d3d12
