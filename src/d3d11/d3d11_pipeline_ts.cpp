#include "Metal.hpp"
#include "airconv_public.h"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_shader.hpp"
#include "log/log.hpp"
#include "thread.hpp"

namespace dxmt {

static dxmt::mutex ts_global_mutex;

class MTLCompiledTessellationPipelineImpl: public MTLCompiledTessellationMeshPipeline {
public:
  MTLCompiledTessellationPipelineImpl(MTLD3D11Device *pDevice,
                                  const MTL_GRAPHICS_PIPELINE_DESC *pDesc)
      : num_rtvs(pDesc->NumColorAttachments),
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

    hull_reflection = pDesc->HullShader->reflection();
    auto &domain_reflection = pDesc->DomainShader->reflection();
    uint32_t max_potential_factor = domain_reflection.PostTessellator.MaxPotentialTessFactor;

    if (!device_->GetMTLDevice().supportsFamily(WMTGPUFamilyApple9)) {
      // indeed this value might be too conservative
      max_potential_factor = std::min(8u, max_potential_factor);
    }
    if ((float)max_potential_factor < hull_reflection.Tessellator.MaxFactor) {
      WARN("maxtessfactor(", hull_reflection.Tessellator.MaxFactor,
           ") is too large for a mesh pipeline. Clamping to ",
           max_potential_factor);
    }

    VertexHullShader =
        pDesc->HullShader->get_shader(ShaderVariantTessellationVertexHull{
            pDesc->InputLayout,
            pDesc->VertexShader, pDesc->IndexBufferFormat,
            max_potential_factor});
    DomainShader =
        pDesc->DomainShader->get_shader(ShaderVariantTessellationDomain{
            pDesc->HullShader, pDesc->GSPassthrough,
            max_potential_factor, !pDesc->RasterizationEnabled});
    if (pDesc->PixelShader) {
      PixelShader = pDesc->PixelShader->get_shader(ShaderVariantPixel{
          pDesc->SampleMask, pDesc->BlendState->IsDualSourceBlending(),
          depth_stencil_format == WMTPixelFormatInvalid,
          unorm_output_reg_mask});
      ps_valid_render_targets = pDesc->PixelShader->reflection().PSValidRenderTargets;
    } else {
      PixelShader = nullptr;
      ps_valid_render_targets = 0;
    }
  }

  void GetPipeline(MTL_COMPILED_TESSELLATION_MESH_PIPELINE *pPipeline) final {
    ready_.wait(false, std::memory_order_acquire);
    *pPipeline = {state_rasterization_, hull_reflection.NumOutputElement,
                  hull_reflection.ThreadsPerPatch};
  }

  ThreadpoolWork *RunThreadpoolWork() {

    WMT::Reference<WMT::Error> err;
    MTL_COMPILED_SHADER vhs, ds, ps;

    if (!VertexHullShader->GetShader(&vhs)) {
      return VertexHullShader;
    }
    if (!vhs.Function) {
      ERR("Failed to create tess-mesh PSO: Invalid vertex-hull shader.");
      return this;
    }
    if (!DomainShader->GetShader(&ds)) {
      return DomainShader;
    }
    if (!ds.Function) {
      ERR("Failed to create tess-mesh PSO: Invalid domain shader.");
      return this;
    }
    if (PixelShader) {
      if (!PixelShader->GetShader(&ps)) {
        return PixelShader;
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

    // HACK for AMDGPU: always reserve all payload capability
    if (!device_->GetMTLDevice().supportsFamily(WMTGPUFamilyApple7))
      info.payload_memory_length = 16384;

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
    if (DepthStencilPlanarFlags(depth_stencil_format) & 2) {
      info.stencil_pixel_format = depth_stencil_format;
    }

    if (pBlendState) {
      pBlendState->SetupMetalPipelineDescriptor(
          (WMTRenderPipelineBlendInfo *)&info, num_rtvs, ps_valid_render_targets);
    }

    info.raster_sample_count = SampleCount;
    // so far it's a fixed constant
    info.mesh_tgsize_is_multiple_of_sgwidth = true;
    // total threads is 32
    // FIXME: might be different on AMD GPU, if it's ever supported
    info.object_tgsize_is_multiple_of_sgwidth = true;

    {
      std::lock_guard<dxmt::mutex> lock(ts_global_mutex);
      state_rasterization_ =
          device_->GetMTLDevice().newRenderPipelineState(info, err);
    }
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
  UINT ps_valid_render_targets;
  WMTPixelFormat rtv_formats[8];
  WMTPixelFormat depth_stencil_format;
  MTLD3D11Device *device_;
  std::atomic_bool ready_;
  IMTLD3D11BlendState *pBlendState;
  WMT::Reference<WMT::RenderPipelineState> state_rasterization_;
  bool RasterizationEnabled;
  UINT SampleCount;

  MTL_SHADER_REFLECTION hull_reflection;

  CompiledShader *VertexHullShader;
  CompiledShader *PixelShader;
  CompiledShader *DomainShader;
};

std::unique_ptr<MTLCompiledTessellationMeshPipeline>
CreateTessellationMeshPipeline(MTLD3D11Device *pDevice,
                               MTL_GRAPHICS_PIPELINE_DESC *pDesc) {
  return std::make_unique<MTLCompiledTessellationPipelineImpl>(pDevice, pDesc);
}

} // namespace dxmt