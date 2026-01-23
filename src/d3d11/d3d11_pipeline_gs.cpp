#include "Metal.hpp"
#include "airconv_public.h"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "log/log.hpp"

namespace dxmt {

class MTLCompiledGeometryPipelineImpl
    : public MTLCompiledGeometryPipeline {
public:
  MTLCompiledGeometryPipelineImpl(MTLD3D11Device *pDevice,
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
    VertexShader =
        pDesc->VertexShader->get_shader(ShaderVariantGeometryVertex{
            pDesc->InputLayout, pDesc->GeometryShader,
            pDesc->IndexBufferFormat, pDesc->GSStripTopology});
    GeometryShader = pDesc->GeometryShader->get_shader(
      ShaderVariantGeometry{pDesc->VertexShader, pDesc->GSStripTopology});
   
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

  void GetPipeline(MTL_COMPILED_GRAPHICS_PIPELINE *pPipeline) final {
    ready_.wait(false, std::memory_order_acquire);
    *pPipeline = {state_mesh_};
  }

  ThreadpoolWork *RunThreadpoolWork() {

    WMT::Reference<WMT::Error> err;
    MTL_COMPILED_SHADER vs, gs, ps;

    if (!VertexShader->GetShader(&vs)) {
      return VertexShader;
    }
    if (!vs.Function) {
      ERR("Failed to create mesh PSO: Invalid vertex shader.");
      return this;
    }
    if (!GeometryShader->GetShader(&gs)) {
      return GeometryShader;
    }
    if (!gs.Function) {
      ERR("Failed to create mesh PSO: Invalid geometry shader.");
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

    info.object_function = vs.Function;
    info.mesh_function = gs.Function;
    info.payload_memory_length = 16256;

    info.immutable_object_buffers = (1 << 16)  | (1 << 21) | (1 << 29) | (1 << 30);
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
      pBlendState->SetupMetalPipelineDescriptor((WMTRenderPipelineBlendInfo *)&info, num_rtvs, ps_valid_render_targets);
    }

    info.raster_sample_count = SampleCount;

    state_mesh_ = device_->GetMTLDevice().newRenderPipelineState(info, err);

    if (state_mesh_ == nullptr) {
      ERR("Failed to create mesh PSO: ", err.description().getUTF8String());
      return this;
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
  WMT::Reference<WMT::RenderPipelineState> state_mesh_;
  bool RasterizationEnabled;
  UINT SampleCount;

  CompiledShader *VertexShader;
  CompiledShader *PixelShader;
  CompiledShader *GeometryShader;
};

std::unique_ptr<MTLCompiledGeometryPipeline>
CreateGeometryPipeline(MTLD3D11Device *pDevice,
                       MTL_GRAPHICS_PIPELINE_DESC *pDesc) {
  return std::make_unique<MTLCompiledGeometryPipelineImpl>(pDevice, pDesc);
}

} // namespace dxmt