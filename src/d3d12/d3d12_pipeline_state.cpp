#include "d3d12_pipeline_state.hpp"
#include "d3d12_device.hpp"
#include "d3d12_shader_manager.hpp"
#include "log/log.hpp"
#include <cstring>

namespace dxmt::d3d12 {

// ── Helpers ──

static WMTPixelFormat MapDXGIFormatToWMT(DXGI_FORMAT fmt) {
    switch (fmt) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:       return WMTPixelFormatRGBA8Unorm;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:  return WMTPixelFormatRGBA8Unorm_sRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM:       return WMTPixelFormatBGRA8Unorm;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:  return WMTPixelFormatBGRA8Unorm_sRGB;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:   return WMTPixelFormatRGBA16Float;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:   return WMTPixelFormatRGBA32Float;
    case DXGI_FORMAT_R10G10B10A2_UNORM:    return WMTPixelFormatRGB10A2Unorm;
    case DXGI_FORMAT_R11G11B10_FLOAT:      return WMTPixelFormatRG11B10Float;
    case DXGI_FORMAT_R32_FLOAT:            return WMTPixelFormatR32Float;
    case DXGI_FORMAT_D32_FLOAT:            return WMTPixelFormatDepth32Float;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:    return WMTPixelFormatDepth24Unorm_Stencil8;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return WMTPixelFormatDepth32Float_Stencil8;
    default: return WMTPixelFormatInvalid;
    }
}

static WMTPrimitiveTopologyClass MapTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE t) {
    switch (t) {
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT:    return WMTPrimitiveTopologyClassPoint;
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:     return WMTPrimitiveTopologyClassLine;
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE:
    default:                                     return WMTPrimitiveTopologyClassTriangle;
    }
}

static WMT::PrimitiveType MapPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY_TYPE t) {
    switch (t) {
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT:    return WMT::PrimitiveTypePoint;
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:     return WMT::PrimitiveTypeLine;
    case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE:
    default:                                     return WMT::PrimitiveTypeTriangle;
    }
}

D3D12PipelineState::D3D12PipelineState(D3D12Device *device, bool is_graphics)
    : device_(device)
    , is_graphics_(is_graphics)
    , refcount_(1) {
}

D3D12PipelineState::~D3D12PipelineState() {
    TRACE("D3D12PipelineState destroyed (",
          is_graphics_ ? "graphics" : "compute",
          compiled_ ? ", compiled)" : ")");
}

// ── Factory: graphics PSO ──

HRESULT D3D12PipelineState::CreateGraphics(
    D3D12Device *device,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc,
    D3D12PipelineState **out) {

    if (!device || !pDesc || !out) return E_INVALIDARG;
    *out = nullptr;

    auto *pso = new D3D12PipelineState(device, true);

    // Store root signature reference
    pso->root_sig_ = static_cast<D3D12RootSignature *>(pDesc->pRootSignature);
    if (pso->root_sig_) pso->root_sig_->AddRef();

    // Store PSO state
    pso->topology_type_ = pDesc->PrimitiveTopologyType;
    pso->mtl_primitive_type_ = MapPrimitiveType(pDesc->PrimitiveTopologyType);
    pso->num_render_targets_ = pDesc->NumRenderTargets;
    for (UINT i = 0; i < pDesc->NumRenderTargets && i < 8; i++)
        pso->rtv_formats_[i] = pDesc->RTVFormats[i];
    pso->dsv_format_ = pDesc->DSVFormat;

    // Store cached blob
    if (pDesc->CachedPSO && pDesc->CachedPSOLength > 0) {
        auto *data = static_cast<const uint8_t *>(pDesc->CachedPSO);
        pso->cached_blob_.assign(data, data + pDesc->CachedPSOLength);
    }

    // ── M11: Compile shaders and create Metal PSO ──
    auto &shader_mgr = device->GetShaderManager();

    // Initialize shaders
    CompiledShader *vs = nullptr;
    CompiledShader *ps = nullptr;

    if (pDesc->VS.pShaderBytecode && pDesc->VS.BytecodeLength > 0) {
        vs = shader_mgr.initializeShader(
            pDesc->VS.pShaderBytecode, pDesc->VS.BytecodeLength);
    }
    if (pDesc->PS.pShaderBytecode && pDesc->PS.BytecodeLength > 0) {
        ps = shader_mgr.initializeShader(
            pDesc->PS.pShaderBytecode, pDesc->PS.BytecodeLength);
    }

    // If we have both VS and PS, compile a full graphics pipeline
    if (vs && ps) {
        MetalShaderPair pair;
        if (shader_mgr.compileGraphicsPipeline(vs, ps, &pair)) {
            // Build Metal render pipeline descriptor
            WMTRenderPipelineInfo info = {};
            InitializeRenderPipelineInfo(info);

            info.vertex_function = pair.vertex.handle;
            info.fragment_function = pair.fragment.handle;

            // Color attachments
            for (UINT i = 0; i < pDesc->NumRenderTargets && i < 8; i++) {
                info.colors[i].pixel_format = MapDXGIFormatToWMT(pso->rtv_formats_[i]);
            }

            // Depth/stencil
            if (pDesc->DSVFormat != DXGI_FORMAT_UNKNOWN) {
                WMTPixelFormat ds_fmt = MapDXGIFormatToWMT(pDesc->DSVFormat);
                if (ds_fmt == WMTPixelFormatDepth32Float ||
                    ds_fmt == WMTPixelFormatDepth32Float_Stencil8) {
                    info.depth_pixel_format = ds_fmt;
                } else if (ds_fmt == WMTPixelFormatDepth24Unorm_Stencil8) {
                    info.depth_pixel_format = WMTPixelFormatDepth32Float;
                }
            }

            info.raster_sample_count = pDesc->SampleDesc.Count > 0
                ? pDesc->SampleDesc.Count : 1;
            info.input_primitive_topology = MapTopology(pDesc->PrimitiveTopologyType);
            info.rasterization_enabled = true;

            // Rasterizer state
            if (pDesc->RasterizerState.FillMode == D3D12_FILL_MODE_WIREFRAME) {
                // Metal doesn't support wireframe — fill instead
                TRACE("PSO: wireframe requested, using fill");
            }

            // Blend state: use defaults from InitializeRenderPipelineInfo
            // Custom blend settings would be mapped here

            WMT::Error error;
            pso->mtl_render_pso_ = device->GetMTLDevice()
                .newRenderPipelineState(info, error);

            if (!pso->mtl_render_pso_) {
                WARN("PSO: Metal render pipeline compilation failed");
            } else {
                pso->compiled_ = true;
                TRACE("Graphics PSO: Metal render pipeline compiled successfully (",
                      pDesc->NumRenderTargets, " RTs, samples=",
                      info.raster_sample_count, ")");
            }
        } else {
            WARN("PSO: shader compilation failed — PSO will be un-compiled");
        }
    } else if (vs && !ps) {
        // VS-only pipeline (stream output, transform feedback)
        WARN("PSO: VS-only pipeline not fully supported");
    } else {
        WARN("PSO: No shaders provided — empty PSO");
    }

    *out = pso;
    return S_OK;
}

// ── Factory: compute PSO ──

HRESULT D3D12PipelineState::CreateCompute(
    D3D12Device *device,
    const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc,
    D3D12PipelineState **out) {

    if (!device || !pDesc || !out) return E_INVALIDARG;
    *out = nullptr;

    auto *pso = new D3D12PipelineState(device, false);

    pso->root_sig_ = static_cast<D3D12RootSignature *>(pDesc->pRootSignature);
    if (pso->root_sig_) pso->root_sig_->AddRef();

    if (pDesc->CachedPSO && pDesc->CachedPSOLength > 0) {
        auto *data = static_cast<const uint8_t *>(pDesc->CachedPSO);
        pso->cached_blob_.assign(data, data + pDesc->CachedPSOLength);
    }

    // ── M11: Compile compute shader ──
    auto &shader_mgr = device->GetShaderManager();

    if (pDesc->CS.pShaderBytecode && pDesc->CS.BytecodeLength > 0) {
        CompiledShader *cs = shader_mgr.initializeShader(
            pDesc->CS.pShaderBytecode, pDesc->CS.BytecodeLength);

        if (cs) {
            MetalComputeKernel kernel;
            if (shader_mgr.compileComputePipeline(cs, &kernel)) {
                WMTComputePipelineInfo info = {};
                info.compute_function = kernel.kernel.handle;

                WMT::Error error;
                pso->mtl_compute_pso_ = device->GetMTLDevice()
                    .newComputePipelineState(info, error);

                if (!pso->mtl_compute_pso_) {
                    WARN("PSO: Metal compute pipeline compilation failed");
                } else {
                    pso->compiled_ = true;
                    TRACE("Compute PSO: Metal compute pipeline compiled successfully");
                }
            } else {
                WARN("PSO: compute shader compilation failed");
            }
        }
    } else {
        WARN("PSO: No compute shader provided");
    }

    *out = pso;
    return S_OK;
}

// ── IUnknown ──

HRESULT STDMETHODCALLTYPE D3D12PipelineState::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_ID3D12Object || riid == IID_ID3D12PipelineState) {
        *ppvObject = static_cast<ID3D12PipelineState *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D12PipelineState::AddRef() {
    return refcount_.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE D3D12PipelineState::Release() {
    ULONG c = refcount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (c == 0) {
        if (root_sig_) root_sig_->Release();
        delete this;
    }
    return c;
}

// ── ID3D12PipelineState ──

HRESULT STDMETHODCALLTYPE D3D12PipelineState::GetCachedBlob(void **ppBlob) {
    if (!ppBlob) return E_INVALIDARG;
    if (cached_blob_.empty()) {
        *ppBlob = nullptr;
        return S_FALSE;
    }
    size_t size = cached_blob_.size();
    void *blob = CoTaskMemAlloc(size);
    if (!blob) return E_OUTOFMEMORY;
    memcpy(blob, cached_blob_.data(), size);
    *ppBlob = blob;
    return S_OK;
}

} // namespace dxmt::d3d12
