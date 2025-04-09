#include "Metal/MTLPipeline.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "airconv_public.h"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_shader.hpp"
#include "log/log.hpp"

namespace dxmt {

class MTLCompiledGeometryPipeline
    : public ComObject<IMTLCompiledGeometryPipeline> {
public:
  MTLCompiledGeometryPipeline(MTLD3D11Device *pDevice,
                              const MTL_GRAPHICS_PIPELINE_DESC *pDesc)
      : ComObject<IMTLCompiledGeometryPipeline>(),
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
    VertexShader =
        pDesc->VertexShader->get_shader(ShaderVariantGeometryVertex{
            (uint64_t)pDesc->InputLayout, (uint64_t)pDesc->GeometryShader->handle(),
            pDesc->IndexBufferFormat, pDesc->GSStripTopology});
    GeometryShader = pDesc->GeometryShader->get_shader(
      ShaderVariantGeometry{(uint64_t)pDesc->VertexShader->handle(), pDesc->GSStripTopology});
   
    if (pDesc->PixelShader) {
      PixelShader = pDesc->PixelShader->get_shader(ShaderVariantPixel{
          pDesc->SampleMask, pDesc->BlendState->IsDualSourceBlending(),
          depth_stencil_format == WMTPixelFormatInvalid,
          unorm_output_reg_mask});
    }
  }

  void SubmitWork() { device_->SubmitThreadgroupWork(this); }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
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

  void GetPipeline(MTL_COMPILED_GRAPHICS_PIPELINE *pPipeline) final {
    ready_.wait(false, std::memory_order_acquire);
    *pPipeline = {state_mesh_.ptr()};
  }

  IMTLThreadpoolWork *RunThreadpoolWork() {

    Obj<NS::Error> err;
    MTL_COMPILED_SHADER vs, gs, ps;

    if (!VertexShader->GetShader(&vs)) {
      return VertexShader.ptr();
    }
    if (!GeometryShader->GetShader(&gs)) {
      return GeometryShader.ptr();
    }
    if (PixelShader && !PixelShader->GetShader(&ps)) {
      return PixelShader.ptr();
    }

    auto mesh_pipeline_desc =
        transfer(MTL::MeshRenderPipelineDescriptor::alloc()->init());

    mesh_pipeline_desc->setObjectFunction(vs.Function);
    mesh_pipeline_desc->setMeshFunction(gs.Function);
    mesh_pipeline_desc->setPayloadMemoryLength(16256);

    mesh_pipeline_desc->objectBuffers()->object(16)->setMutability(
        MTL::MutabilityImmutable);
    mesh_pipeline_desc->objectBuffers()->object(21)->setMutability(
        MTL::MutabilityImmutable);
    mesh_pipeline_desc->objectBuffers()->object(20)->setMutability(
        MTL::MutabilityMutable);

    if (PixelShader) {
      mesh_pipeline_desc->setFragmentFunction(ps.Function);
    }
    mesh_pipeline_desc->setRasterizationEnabled(RasterizationEnabled);

    for (unsigned i = 0; i < num_rtvs; i++) {
      if (rtv_formats[i] == WMTPixelFormatInvalid)
        continue;
      mesh_pipeline_desc->colorAttachments()->object(i)->setPixelFormat(
        (MTL::PixelFormat)rtv_formats[i]);
    }

    if (depth_stencil_format != WMTPixelFormatInvalid) {
      mesh_pipeline_desc->setDepthAttachmentPixelFormat((MTL::PixelFormat)depth_stencil_format);
    }
    // FIXME: don't hardcoding!
    if (depth_stencil_format == WMTPixelFormatDepth32Float_Stencil8 ||
        depth_stencil_format == WMTPixelFormatDepth24Unorm_Stencil8 ||
        depth_stencil_format == WMTPixelFormatStencil8) {
      mesh_pipeline_desc->setStencilAttachmentPixelFormat((MTL::PixelFormat)depth_stencil_format);
    }

    if (pBlendState) {
      pBlendState->SetupMetalPipelineDescriptor(mesh_pipeline_desc, num_rtvs);
    }

    mesh_pipeline_desc->setRasterSampleCount(SampleCount);

    state_mesh_ = transfer(device_->GetMTLDevice()->newRenderPipelineState(
        mesh_pipeline_desc.ptr(), MTL::PipelineOptionNone, nullptr, &err));

    if (state_mesh_ == nullptr) {
      ERR("Failed to create mesh PSO: ",
          err->localizedDescription()->utf8String());
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
  WMTPixelFormat rtv_formats[8];
  WMTPixelFormat depth_stencil_format;
  MTLD3D11Device *device_;
  std::atomic_bool ready_;
  IMTLD3D11BlendState *pBlendState;
  Obj<MTL::RenderPipelineState> state_mesh_;
  bool RasterizationEnabled;
  UINT SampleCount;

  Com<CompiledShader> VertexShader;
  Com<CompiledShader> PixelShader;
  Com<CompiledShader> GeometryShader;
};

Com<IMTLCompiledGeometryPipeline>
CreateGeometryPipeline(MTLD3D11Device *pDevice,
                       MTL_GRAPHICS_PIPELINE_DESC *pDesc) {
  Com<IMTLCompiledGeometryPipeline> pipeline =
      new MTLCompiledGeometryPipeline(pDevice, pDesc);
  pipeline->SubmitWork();
  return pipeline;
}

} // namespace dxmt