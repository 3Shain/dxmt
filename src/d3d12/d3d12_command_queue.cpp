#include "d3d12_command_queue.hpp"
#include "d3d12_command_list.hpp"
#include "d3d12_device.hpp"
#include "d3d12_pipeline_state.hpp"
#include "d3d12_resource.hpp"
#include "d3d12_fence.hpp"
#include "d3d12_descriptor_heap.hpp"
#include "d3d12_swapchain.hpp"
#include "log/log.hpp"
#include <cstring>
#include <unordered_map>

namespace dxmt::d3d12 {

// ── Execution context (M5: extended for render pass + vertex buffers) ──

struct ExecuteContext {
    WMT::CommandBuffer cmd_buf;
    WMT::Reference<WMT::RenderCommandEncoder> render_enc;
    WMT::Reference<WMT::ComputeCommandEncoder> compute_enc;
    WMT::Reference<WMT::BlitCommandEncoder> blit_enc;
    D3D12PipelineState *current_pso = nullptr;
    D3D12Device *device = nullptr;

    // M5: Descriptor heaps from command list
    ID3D12DescriptorHeap *descriptor_heaps_[2] = {};
    UINT descriptor_heap_count_ = 0;

    // Resource state tracking
    std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> resource_states_;

    void endRenderEncoder() {
        if (render_enc) { render_enc->endEncoding(); render_enc = nullptr; }
    }
    void endComputeEncoder() {
        if (compute_enc) { compute_enc->endEncoding(); compute_enc = nullptr; }
    }
    void endBlitEncoder() {
        if (blit_enc) { blit_enc->endEncoding(); blit_enc = nullptr; }
    }
    void endAllEncoders() {
        endRenderEncoder(); endComputeEncoder(); endBlitEncoder();
    }

    WMT::RenderCommandEncoder ensureRenderEncoder(
        ID3D12Resource *const *rt_resources, UINT num_rt,
        ID3D12Resource *ds_resource = nullptr) {
        endBlitEncoder(); endComputeEncoder();
        if (render_enc) return render_enc;

        WMTRenderPassInfo rp_info = {};
        InitializeRenderPassInfo(rp_info);

        // M5: Build color attachments from resolved resources
        for (UINT i = 0; i < num_rt && i < 8 && rt_resources; i++) {
            if (rt_resources[i]) {
                auto *res = static_cast<D3D12Resource *>(rt_resources[i]);
                if (res->GetTexture()) {
                    rp_info.colors[i].texture = res->GetTexture()->texture().handle;
                    rp_info.colors[i].load_action = WMTLoadActionClear;
                    rp_info.colors[i].store_action = WMTStoreActionStore;
                    rp_info.colors[i].clear_color[0] = 0.0f;
                    rp_info.colors[i].clear_color[1] = 0.0f;
                    rp_info.colors[i].clear_color[2] = 0.0f;
                    rp_info.colors[i].clear_color[3] = 1.0f;
                }
            }
        }

        // Depth/stencil
        if (ds_resource) {
            auto *res = static_cast<D3D12Resource *>(ds_resource);
            if (res->GetTexture()) {
                rp_info.depth.texture = res->GetTexture()->texture().handle;
                rp_info.depth.load_action = WMTLoadActionClear;
                rp_info.depth.store_action = WMTStoreActionStore;
                rp_info.depth.clear_depth = 1.0f;
            }
        }

        render_enc = cmd_buf.renderCommandEncoder(rp_info);
        return render_enc;
    }

    WMT::ComputeCommandEncoder ensureComputeEncoder() {
        endRenderEncoder(); endBlitEncoder();
        if (!compute_enc)
            compute_enc = cmd_buf.computeCommandEncoder(false);
        return compute_enc;
    }

    void applyBarrier(ID3D12Resource *res, D3D12_RESOURCE_STATES after) {
        if (!res) return;
        auto it = resource_states_.find(res);
        D3D12_RESOURCE_STATES before = (it != resource_states_.end())
            ? it->second : D3D12_RESOURCE_STATE_COMMON;
        if (before == after) return;
        endAllEncoders();
        resource_states_[res] = after;
        TRACE("  Barrier: ", (void*)res, " ", (uint32_t)before, " -> ", (uint32_t)after);
    }
};

// ── Primitive type mapping ──

static WMTPrimitiveType MapPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY_TYPE t) {
    switch (t) {
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT:  return WMTPrimitiveTypePoint;
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:   return WMTPrimitiveTypeLine;
    default:                                   return WMTPrimitiveTypeTriangle;
    }
}

// ── M5: Resolve descriptor handle to resource ──

static ID3D12Resource *ResolveRTVHandle(
    ID3D12DescriptorHeap *heap,
    D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    if (!heap || handle.ptr == 0) return nullptr;
    auto *dh = static_cast<D3D12DescriptorHeap *>(heap);
    return dh->GetResourceFromHandle(handle);
}

// ── ExecuteVisitor ──

static void ExecuteVisitor(CommandTag tag, const void *data,
                             uint32_t size, void *userdata) {
    auto *ctx = static_cast<ExecuteContext *>(userdata);

    switch (tag) {
    case CommandTag::SetPipelineState:
        if (ctx->current_pso && ctx->current_pso->IsCompiled()) {
            if (ctx->current_pso->IsGraphics()) {
                auto pso = ctx->current_pso->GetMTLRenderPipelineState();
                if (pso) {
                    // Encoder created lazily — render pass set up on first draw
                }
            }
        }
        break;

    case CommandTag::SetGraphicsRootSig:
    case CommandTag::SetComputeRootSig:
    case CommandTag::IASetPrimTopology:
    case CommandTag::IASetIndexBuffer:
        break;

    // ── M5: Vertex buffer binding ──
    case CommandTag::IASetVertexBuffers: {
        auto *d = (const struct {
            UINT start; UINT count;
            D3D12_VERTEX_BUFFER_VIEW views[32];
            ID3D12Resource *resources[32];
        } *)data;

        for (UINT i = 0; i < d->count && d->resources[i]; i++) {
            auto *res = static_cast<D3D12Resource *>(d->resources[i]);
            if (res && res->GetBuffer()) {
                WMT::Buffer mtl_buf = res->GetBuffer()->buffer();
                if (mtl_buf) {
                    ctx->ensureRenderEncoder(nullptr, 0)->setVertexBuffer(
                        mtl_buf, 0, d->start + i);
                }
            }
        }
        TRACE("  Execute: IASetVertexBuffers (", d->count, " bound)");
        break;
    }

    // ── M5: Render target binding → creates render pass ──
    case CommandTag::OMSetRenderTargets: {
        auto *d = (const struct {
            UINT num_rt; BOOL single_handle;
            D3D12_CPU_DESCRIPTOR_HANDLE rt_handles[8];
            D3D12_CPU_DESCRIPTOR_HANDLE ds;
        } *)data;

        TRACE("  Execute: OMSetRenderTargets (", d->num_rt, " RTs)");

        // M5: Resolve RT resources from descriptor heaps
        ID3D12Resource *rt_resources[8] = {};
        for (UINT r = 0; r < d->num_rt && r < 8; r++) {
            for (UINT h = 0; h < ctx->descriptor_heap_count_; h++) {
                auto *heap = static_cast<D3D12DescriptorHeap *>(
                    ctx->descriptor_heaps_[h]);
                if (heap && heap->GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
                    rt_resources[r] = heap->GetResourceFromHandle(
                        d->rt_handles[r]);
                    if (rt_resources[r]) break;
                }
            }
        }

        // Resolve DSV resource
        ID3D12Resource *ds_resource = nullptr;
        if (d->ds.ptr) {
            for (UINT h = 0; h < ctx->descriptor_heap_count_; h++) {
                auto *heap = static_cast<D3D12DescriptorHeap *>(
                    ctx->descriptor_heaps_[h]);
                if (heap && heap->GetType() == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
                    ds_resource = heap->GetResourceFromHandle(d->ds);
                    if (ds_resource) break;
                }
            }
        }

        // Create render pass from resolved resources
        if (d->num_rt > 0 && rt_resources[0]) {
            ctx->ensureRenderEncoder(rt_resources, d->num_rt, ds_resource);
        }
        break;
    }

    case CommandTag::RSSetViewports: {
        auto *d = (const struct { UINT count; D3D12_VIEWPORT vps[16]; } *)data;
        if (d->count > 0 && ctx->render_enc) {
            WMTViewport vp = {d->vps[0].TopLeftX, d->vps[0].TopLeftY,
                              d->vps[0].Width, d->vps[0].Height,
                              d->vps[0].MinDepth, d->vps[0].MaxDepth};
            ctx->render_enc->setViewport(vp);
        }
        break;
    }

    case CommandTag::RSSetScissorRects:
    case CommandTag::SetDescriptorHeaps:
        break;

    // ── Draw ──
    case CommandTag::DrawInstanced: {
        auto *d = (const struct { UINT vc, ic, sv, si; } *)data;
        // M5: Create render pass on first draw if not already active
        if (!ctx->render_enc) {
            ctx->ensureRenderEncoder(nullptr, 0);
        }
        if (ctx->render_enc && ctx->current_pso) {
            auto pso = ctx->current_pso->GetMTLRenderPipelineState();
            if (pso) ctx->render_enc->setRenderPipelineState(pso);
        }
        WMTPrimitiveType prim = ctx->current_pso
            ? MapPrimitiveType(ctx->current_pso->GetTopologyType())
            : WMTPrimitiveTypeTriangle;
        ctx->render_enc->drawPrimitives(prim, d->sv, d->vc);
        TRACE("  Execute: DrawInstanced (v=", d->vc, ")");
        break;
    }

    case CommandTag::DrawIndexedInstanced: {
        auto *d = (const struct { UINT ic, instc, si; INT bv; UINT si2; } *)data;
        if (!ctx->render_enc) ctx->ensureRenderEncoder(nullptr, 0);
        if (ctx->render_enc && ctx->current_pso) {
            auto pso = ctx->current_pso->GetMTLRenderPipelineState();
            if (pso) ctx->render_enc->setRenderPipelineState(pso);
        }
        ctx->render_enc->drawPrimitives(WMTPrimitiveTypeTriangle, 0, d->ic);
        break;
    }

    case CommandTag::Dispatch:
        TRACE("  Execute: Dispatch (compute deferred)");
        break;

    case CommandTag::CopyBufferRegion:
    case CommandTag::CopyTextureRegion:
    case CommandTag::CopyResource:
        TRACE("  Execute: Copy (deferred)");
        break;

    case CommandTag::ResourceBarrier: {
        auto *d = (const struct {
            D3D12_RESOURCE_BARRIER_TYPE type; D3D12_RESOURCE_BARRIER_FLAGS flags;
            union {
                struct { ID3D12Resource *res; UINT sub;
                         D3D12_RESOURCE_STATES before, after; } trans;
                struct { ID3D12Resource *res; } alias;
                struct { ID3D12Resource *res; UINT64 base, sz; } uav;
            };
        } *)data;
        if (d->type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
            ctx->applyBarrier(d->trans.res, d->trans.after);
        break;
    }

    case CommandTag::Nop:
    default:
        break;
    }
}

// ═══════════════════════════════════════════════════════════════
// D3D12CommandQueue
// ═══════════════════════════════════════════════════════════════

D3D12CommandQueue::D3D12CommandQueue(D3D12Device *device,
                                      const D3D12_COMMAND_QUEUE_DESC *pDesc)
    : device_(device), desc_(*pDesc), refcount_(1) {
    mtl_queue_ = device_->GetMTLDevice().newCommandQueue(0);
    TRACE("D3D12CommandQueue created");
}

D3D12CommandQueue::~D3D12CommandQueue() {
    TRACE("D3D12CommandQueue destroyed");
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::QueryInterface(
    REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown) {
        *ppvObject = static_cast<ID3D12CommandQueue *>(this);
        AddRef(); return S_OK;
    }
    if (riid == IID_ID3D12Object || riid == IID_ID3D12CommandQueue) {
        *ppvObject = static_cast<ID3D12CommandQueue *>(this);
        AddRef(); return S_OK;
    }
    if (riid == __uuidof(IMTLDXGIDevice12)) {
        *ppvObject = static_cast<IMTLDXGIDevice12 *>(this);
        AddRef(); return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12CommandQueue::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}
ULONG STDMETHODCALLTYPE D3D12CommandQueue::Release() {
    ULONG c = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (c == 0) delete this;
    return c;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetPrivateData(
    REFGUID, UINT*, void*) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetPrivateData(
    REFGUID, UINT, const void*) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetPrivateDataInterface(
    REFGUID, const IUnknown*) { return E_NOTIMPL; }
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetName(LPCWSTR) { return S_OK; }

void STDMETHODCALLTYPE D3D12CommandQueue::UpdateTileMappings(
    ID3D12Resource*, UINT, const void*, const void*,
    ID3D12Heap*, UINT, const void*, const UINT*,
    const UINT*, D3D12_TILE_MAPPING_FLAGS) {}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::CopyTileMappings(
    ID3D12Resource*, const void*, ID3D12Resource*,
    const void*, const void*, D3D12_TILE_MAPPING_FLAGS) {
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::ExecuteCommandLists(
    UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists) {
    TRACE("ExecuteCommandLists: ", NumCommandLists, " lists");
    for (UINT i = 0; i < NumCommandLists; i++) {
        if (!ppCommandLists[i])
            { WARN("null list at ", i); return E_INVALIDARG; }
        auto *list = static_cast<D3D12GraphicsCommandList *>(ppCommandLists[i]);
        if (!list->IsClosed())
            { WARN("list ", i, " not closed"); return E_FAIL; }
    }

    WMT::CommandBuffer cmd_buf = mtl_queue_.commandBuffer();
    ExecuteContext exec_ctx;
    exec_ctx.cmd_buf = cmd_buf;
    exec_ctx.device = device_;

    for (UINT i = 0; i < NumCommandLists; i++) {
        auto *list = static_cast<D3D12GraphicsCommandList *>(ppCommandLists[i]);
        exec_ctx.current_pso = list->GetCurrentPSO();
        // Copy descriptor heaps from this command list for handle resolution
        exec_ctx.descriptor_heap_count_ = list->GetDescriptorHeapCount();
        auto *heaps = list->GetDescriptorHeaps();
        for (UINT h = 0; h < exec_ctx.descriptor_heap_count_; h++)
            exec_ctx.descriptor_heaps_[h] = heaps[h];
        list->WalkCommands(ExecuteVisitor, &exec_ctx);
    }

    exec_ctx.endAllEncoders();
    cmd_buf.commit();
    TRACE("  Metal command buffer committed");
    return S_OK;
}

void STDMETHODCALLTYPE D3D12CommandQueue::SetMarker(UINT, const void*, UINT) {}
void STDMETHODCALLTYPE D3D12CommandQueue::BeginEvent(UINT, const void*, UINT) {}
void STDMETHODCALLTYPE D3D12CommandQueue::EndEvent() {}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::Signal(
    ID3D12Fence *f, UINT64 v) {
    if (!f) return E_INVALIDARG;
    auto *fence = static_cast<D3D12Fence *>(f);
    WMT::CommandBuffer cb = mtl_queue_.commandBuffer();
    fence->SignalFromQueue(cb, v);
    cb.commit();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::Wait(
    ID3D12Fence *f, UINT64 v) {
    if (!f) return E_INVALIDARG;
    auto *fence = static_cast<D3D12Fence *>(f);
    WMT::CommandBuffer cb = mtl_queue_.commandBuffer();
    fence->WaitFromQueue(cb, v);
    cb.commit();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetTimestampFrequency(UINT64 *p) {
    if (p) *p = 1'000'000'000;
    return S_OK;
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetClockCalibration(UINT64 *g, UINT64 *c) {
    if (g) *g = 0; if (c) *c = 0;
    return S_OK;
}
void STDMETHODCALLTYPE D3D12CommandQueue::GetDesc(D3D12_COMMAND_QUEUE_DESC *d) {
    if (d) *d = desc_;
}

WMT::Device D3D12CommandQueue::GetMTLDevice() {
    return device_->GetMTLDevice();
}

HRESULT D3D12CommandQueue::CreateSwapChain(
    IDXGIFactory1 *pFactory, HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1 *pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
    IDXGISwapChain1 **ppSwapChain) {
    return CreateSwapChainForD3D12(
        pFactory, device_, this, hWnd, pDesc, pFullscreenDesc, ppSwapChain);
}

WMT::CommandBuffer D3D12CommandQueue::CreateCommandBuffer() {
    return mtl_queue_.commandBuffer();
}

} // namespace dxmt::d3d12
