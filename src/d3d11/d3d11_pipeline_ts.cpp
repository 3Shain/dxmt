#include "Metal.hpp"
#include "airconv_public.h"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_shader.hpp"
#include "log/log.hpp"

namespace dxmt {

class MTLCompiledTessellationPipeline
    : public ComObject<IMTLCompiledTessellationMeshPipeline> {
public:
  MTLCompiledTessellationPipeline(MTLD3D11Device *pDevice,
                                  const MTL_GRAPHICS_PIPELINE_DESC *pDesc)
      : ComObject<IMTLCompiledTessellationMeshPipeline>(),
        num_rtvs(pDesc->NumColorAttachments),
        depth_stencil_format(pDesc->DepthStencilFormat), device_(pDevice),
        pBlendState(pDesc->BlendState),
        RasterizationEnabled(pDesc->RasterizationEnabled),
        SampleCount(pDesc->SampleCount) {
    uint32_t unorm_output_reg_mask = 0;
    for (unsigned i = 0; i < num_rtvs; i++) {
      rtv_formats[i] = pDesc->ColorAttachmentFormats[i];
      unorm_output_reg_mask |= (uint32_t(IsUnorm8RenderTargetFormat(
                                    pDesc->ColorAttachmentFormats[i]))
                                << i);
    }

    VertexHullShader =
        pDesc->HullShader->get_shader(ShaderVariantTessellationVertexHull{
            (uint64_t)pDesc->InputLayout,
            (uint64_t)pDesc->VertexShader->handle(), pDesc->IndexBufferFormat});
    DomainShader =
        pDesc->DomainShader->get_shader(ShaderVariantTessellationDomain{
            (uint64_t)pDesc->HullShader->handle(), pDesc->GSPassthrough,
            !pDesc->RasterizationEnabled});
    if (pDesc->PixelShader) {
      PixelShader = pDesc->PixelShader->get_shader(ShaderVariantPixel{
          pDesc->SampleMask, pDesc->BlendState->IsDualSourceBlending(),
          depth_stencil_format == WMTPixelFormatInvalid,
          unorm_output_reg_mask});
    }
    hull_reflection = pDesc->HullShader->reflection();
  }

  void SubmitWork() { device_->SubmitThreadgroupWork(this); }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLThreadpoolWork) ||
        riid == __uuidof(IMTLCompiledGraphicsPipeline)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  bool IsReady() final { return ready_.load(std::memory_order_relaxed); }

  void GetPipeline(MTL_COMPILED_TESSELLATION_MESH_PIPELINE *pPipeline) final {
    ready_.wait(false, std::memory_order_acquire);
    *pPipeline = {state_rasterization_, hull_reflection.NumOutputElement,
                  hull_reflection.ThreadsPerPatch};
  }

  IMTLThreadpoolWork *RunThreadpoolWork() {

    WMT::Reference<WMT::Error> err;
    MTL_COMPILED_SHADER vhs, ds, ps;

    if (!VertexHullShader->GetShader(&vhs)) {
      return VertexHullShader.ptr();
    }
    if (!vhs.Function) {
      ERR("Failed to create tess-mesh PSO: Invalid vertex-hull shader.");
      return this;
    }
    if (!DomainShader->GetShader(&ds)) {
      return DomainShader.ptr();
    }
    if (!ds.Function) {
      ERR("Failed to create tess-mesh PSO: Invalid domain shader.");
      return this;
    }
    if (PixelShader) {
      if (!PixelShader->GetShader(&ps)) {
        return PixelShader.ptr();
      }
      if (!ps.Function) {
        ERR("Failed to create mesh PSO: Invalid pixel shader.");
        return this;
      }
    }

    WMTMeshRenderPipelineInfo info;
    WMT::InitializeMeshRenderPipelineInfo(info);

    info.object_function = vhs.Function;
    info.mesh_function = ds.Function;
    info.payload_memory_length = 0;

    info.immutable_object_buffers =
        (1 << 16) | (1 << 21) | (1 << 27) | (1 << 28) | (1 << 29) | (1 << 30);
    info.immutable_mesh_buffers = (1 << 29) | (1 << 30);
    info.immutable_fragment_buffers = (1 << 29) | (1 << 30);

    if (PixelShader) {
      info.fragment_function = ps.Function;
    }
    info.rasterization_enabled = RasterizationEnabled;

    for (unsigned i = 0; i < num_rtvs; i++) {
      if (rtv_formats[i] == WMTPixelFormatInvalid)
        continue;
      info.colors[i].pixel_format = rtv_formats[i];
    }

    if (depth_stencil_format != WMTPixelFormatInvalid) {
      info.depth_pixel_format = depth_stencil_format;
    }
    // FIXME: don't hardcoding!
    if (depth_stencil_format == WMTPixelFormatDepth32Float_Stencil8 ||
        depth_stencil_format == WMTPixelFormatDepth24Unorm_Stencil8 ||
        depth_stencil_format == WMTPixelFormatStencil8) {
      info.stencil_pixel_format = depth_stencil_format;
    }

    if (pBlendState) {
      pBlendState->SetupMetalPipelineDescriptor(
          (WMTRenderPipelineBlendInfo *)&info, num_rtvs);
    }

    info.raster_sample_count = SampleCount;

    state_rasterization_ =
        device_->GetMTLDevice().newRenderPipelineState(info, err);
    if (state_rasterization_ == nullptr) {
      ERR("Failed to create tessellation raster PSO: ",
          err.description().getUTF8String());
    }

    return this;
  }

  bool GetIsDone() { return ready_; }

  void SetIsDone(bool state) {
    ready_.store(state);
    ready_.notify_all();
  }

private:
  UINT num_rtvs;
  WMTPixelFormat rtv_formats[8];
  WMTPixelFormat depth_stencil_format;
  MTLD3D11Device *device_;
  std::atomic_bool ready_;
  IMTLD3D11BlendState *pBlendState;
  WMT::Reference<WMT::RenderPipelineState> state_rasterization_;
  bool RasterizationEnabled;
  UINT SampleCount;

  MTL_SHADER_REFLECTION hull_reflection;

  Com<CompiledShader> VertexHullShader;
  Com<CompiledShader> PixelShader;
  Com<CompiledShader> DomainShader;
};

Com<IMTLCompiledTessellationMeshPipeline>
CreateTessellationMeshPipeline(MTLD3D11Device *pDevice,
                               MTL_GRAPHICS_PIPELINE_DESC *pDesc) {
  Com<IMTLCompiledTessellationMeshPipeline> pipeline =
      new MTLCompiledTessellationPipeline(pDevice, pDesc);
  pipeline->SubmitWork();
  return pipeline;
}

} // namespace dxmt