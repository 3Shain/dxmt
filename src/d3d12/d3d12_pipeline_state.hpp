#pragma once

#include "d3d12_private.h"
#include "d3d12_root_signature.hpp"
#include "d3d12_shader_manager.hpp"
#include "Metal.hpp"
#include <atomic>
#include <vector>
#include <memory>

namespace dxmt::d3d12 {

class D3D12Device;

class D3D12PipelineState final : public ID3D12PipelineState {
public:
    // Factory: graphics PSO
    static HRESULT CreateGraphics(
        D3D12Device *device,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc,
        D3D12PipelineState **out);

    // Factory: compute PSO
    static HRESULT CreateCompute(
        D3D12Device *device,
        const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc,
        D3D12PipelineState **out);

    ~D3D12PipelineState();

    // ── IUnknown ──
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;
    ULONG STDMETHODCALLTYPE AddRef() final;
    ULONG STDMETHODCALLTYPE Release() final;

    // ── ID3D12Object ──
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) final { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) final { return S_OK; }

    // ── ID3D12PipelineState ──
    HRESULT STDMETHODCALLTYPE GetCachedBlob(void **ppBlob) final;

    // ── Internal ──
    bool IsGraphics() const { return is_graphics_; }
    bool IsCompute() const { return !is_graphics_; }
    bool IsCompiled() const { return compiled_; }
    D3D12RootSignature *GetRootSignature() const { return root_sig_; }
    D3D12_PRIMITIVE_TOPOLOGY_TYPE GetTopologyType() const { return topology_type_; }

    // Metal PSO access
    WMT::RenderPipelineState GetMTLRenderPipelineState() const { return mtl_render_pso_; }
    WMT::ComputePipelineState GetMTLComputePipelineState() const { return mtl_compute_pso_; }
    WMT::PrimitiveType GetMTLPrimitiveType() const { return mtl_primitive_type_; }

private:
    D3D12PipelineState(D3D12Device *device, bool is_graphics);

    D3D12Device *device_;
    bool is_graphics_;
    bool compiled_ = false;
    std::atomic<ULONG> refcount_;

    // Root signature reference
    D3D12RootSignature *root_sig_ = nullptr;

    // PSO state
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    UINT num_render_targets_ = 0;
    DXGI_FORMAT rtv_formats_[8] = {};
    DXGI_FORMAT dsv_format_ = DXGI_FORMAT_UNKNOWN;

    // Metal pipeline state
    WMT::Reference<WMT::RenderPipelineState> mtl_render_pso_;
    WMT::Reference<WMT::ComputePipelineState> mtl_compute_pso_;
    WMT::PrimitiveType mtl_primitive_type_ = WMT::PrimitiveTypeTriangle;

    // Cached blob
    std::vector<uint8_t> cached_blob_;
};

} // namespace dxmt::d3d12
