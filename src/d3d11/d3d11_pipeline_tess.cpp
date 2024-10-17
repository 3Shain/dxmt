#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "Metal/MTLPipeline.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "airconv_public.h"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_shader.hpp"
#include "log/log.hpp"

namespace dxmt {

class MTLCompiledTessellationPipeline
    : public ComObject<IMTLCompiledTessellationPipeline> {
public:
  MTLCompiledTessellationPipeline(IMTLD3D11Device *pDevice,
                                  const MTL_GRAPHICS_PIPELINE_DESC *pDesc)
      : ComObject<IMTLCompiledTessellationPipeline>(),
        num_rtvs(pDesc->NumColorAttachments),
        depth_stencil_format(pDesc->DepthStencilFormat),
        topology_class(pDesc->TopologyClass), device_(pDevice),
        pBlendState(pDesc->BlendState),
        RasterizationEnabled(pDesc->RasterizationEnabled),
        SampleCount(pDesc->SampleCount) {
    for (unsigned i = 0; i < num_rtvs; i++) {
      rtv_formats[i] = pDesc->ColorAttachmentFormats[i];
    }
    VertexShader =
        pDesc->VertexShader->get_shader(ShaderVariantTessellationVertex{
            (uint64_t)pDesc->InputLayout, (uint64_t)pDesc->HullShader->handle(),
            pDesc->IndexBufferFormat});
    HullShader = pDesc->HullShader->get_shader(
        ShaderVariantTessellationHull{(uint64_t)pDesc->VertexShader->handle()});
    DomainShader =
        pDesc->DomainShader->get_shader(ShaderVariantTessellationDomain{
            (uint64_t)pDesc->HullShader->handle(), pDesc->GSPassthrough});
    if (pDesc->PixelShader) {
      PixelShader = pDesc->PixelShader->get_shader(ShaderVariantPixel{
          pDesc->SampleMask, pDesc->BlendState->IsDualSourceBlending(),
          depth_stencil_format == MTL::PixelFormatInvalid});
    }
    hull_reflection = pDesc->HullShader->reflection();
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

  void GetPipeline(MTL_COMPILED_TESSELLATION_PIPELINE *pPipeline) final {
    ready_.wait(false, std::memory_order_acquire);
    *pPipeline = {state_mesh_.ptr(), state_rasterization_.ptr(),
                  hull_reflection.NumOutputElement,
                  hull_reflection.NumPatchConstantOutputScalar,
                  hull_reflection.ThreadsPerPatch};
  }

  IMTLThreadpoolWork *RunThreadpoolWork() {

    Obj<NS::Error> err;
    MTL_COMPILED_SHADER vs, hs, ds, ps;

    if (!VertexShader->GetShader(&vs)) {
      return VertexShader.ptr();
    }
    if (!HullShader->GetShader(&hs)) {
      return HullShader.ptr();
    }
    if (!DomainShader->GetShader(&ds)) {
      return DomainShader.ptr();
    }
    if (PixelShader && !PixelShader->GetShader(&ps)) {
      return PixelShader.ptr();
    }

    auto mesh_pipeline_desc =
        transfer(MTL::MeshRenderPipelineDescriptor::alloc()->init());

    mesh_pipeline_desc->setObjectFunction(vs.Function);
    mesh_pipeline_desc->setMeshFunction(hs.Function);
    mesh_pipeline_desc->setRasterizationEnabled(false);
    mesh_pipeline_desc->setPayloadMemoryLength(16256);

    mesh_pipeline_desc->objectBuffers()->object(16)->setMutability(
        MTL::MutabilityImmutable);
    mesh_pipeline_desc->objectBuffers()->object(21)->setMutability(
        MTL::MutabilityImmutable);
    mesh_pipeline_desc->objectBuffers()->object(20)->setMutability(
        MTL::MutabilityMutable);

    state_mesh_ = transfer(device_->GetMTLDevice()->newRenderPipelineState(
        mesh_pipeline_desc.ptr(), MTL::PipelineOptionNone, nullptr, &err));

    if (state_mesh_ == nullptr) {
      ERR("Failed to create mesh PSO: ",
          err->localizedDescription()->utf8String());
      return this;
    }

    auto pipelineDescriptor =
        transfer(MTL::RenderPipelineDescriptor::alloc()->init());

    pipelineDescriptor->setVertexFunction(ds.Function);
    if (PixelShader) {
      pipelineDescriptor->setFragmentFunction(ps.Function);
    }
    pipelineDescriptor->setRasterizationEnabled(RasterizationEnabled);

    if (pBlendState) {
      pBlendState->SetupMetalPipelineDescriptor(pipelineDescriptor);
    }

    pipelineDescriptor->setMaxTessellationFactor(
        hull_reflection.Tessellator.MaxFactor);
    switch ((microsoft::D3D11_SB_TESSELLATOR_PARTITIONING)
                hull_reflection.Tessellator.Partition) {
    case microsoft::D3D11_SB_TESSELLATOR_PARTITIONING_INTEGER:
      pipelineDescriptor->setTessellationPartitionMode(
          MTL::TessellationPartitionModeInteger);
      break;
    case microsoft::D3D11_SB_TESSELLATOR_PARTITIONING_POW2:
      pipelineDescriptor->setTessellationPartitionMode(
          MTL::TessellationPartitionModePow2);
      break;
    case microsoft::D3D11_SB_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD:
      pipelineDescriptor->setTessellationPartitionMode(
          MTL::TessellationPartitionModeFractionalOdd);
      break;
    case microsoft::D3D11_SB_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN:
      pipelineDescriptor->setTessellationPartitionMode(
          MTL::TessellationPartitionModeFractionalEven);
      break;
    case microsoft::D3D11_SB_TESSELLATOR_PARTITIONING_UNDEFINED:
      break;
    }
    // TODO: figure out why it's inverted
    if (!hull_reflection.Tessellator.AntiClockwise) {
      pipelineDescriptor->setTessellationOutputWindingOrder(
          MTL::WindingCounterClockwise);
    } else {
      pipelineDescriptor->setTessellationOutputWindingOrder(
          MTL::WindingClockwise);
    }
    pipelineDescriptor->setTessellationFactorStepFunction(
        MTL::TessellationFactorStepFunctionPerPatch);

    for (unsigned i = 0; i < num_rtvs; i++) {
      if (rtv_formats[i] == MTL::PixelFormatInvalid)
        continue;
      pipelineDescriptor->colorAttachments()->object(i)->setPixelFormat(
          rtv_formats[i]);
    }

    if (depth_stencil_format != MTL::PixelFormatInvalid) {
      pipelineDescriptor->setDepthAttachmentPixelFormat(depth_stencil_format);
    }
    // FIXME: don't hardcoding!
    if (depth_stencil_format == MTL::PixelFormatDepth32Float_Stencil8 ||
        depth_stencil_format == MTL::PixelFormatDepth24Unorm_Stencil8 ||
        depth_stencil_format == MTL::PixelFormatStencil8) {
      pipelineDescriptor->setStencilAttachmentPixelFormat(depth_stencil_format);
    }

    pipelineDescriptor->setInputPrimitiveTopology(topology_class);
    pipelineDescriptor->setSampleCount(SampleCount);

    state_rasterization_ =
        transfer(device_->GetMTLDevice()->newRenderPipelineState(
            pipelineDescriptor, &err));

    return this;
  }

  bool GetIsDone() { return ready_; }

  void SetIsDone(bool state) {
    ready_.store(state);
    ready_.notify_all();
  }

private:
  UINT num_rtvs;
  MTL::PixelFormat rtv_formats[8];
  MTL::PixelFormat depth_stencil_format;
  MTL::PrimitiveTopologyClass topology_class;
  IMTLD3D11Device *device_;
  std::atomic_bool ready_;
  IMTLD3D11BlendState *pBlendState;
  Obj<MTL::RenderPipelineState> state_mesh_;
  Obj<MTL::RenderPipelineState> state_rasterization_;
  bool RasterizationEnabled;
  UINT SampleCount;

  MTL_SHADER_REFLECTION hull_reflection;

  Com<CompiledShader> VertexShader;
  Com<CompiledShader> PixelShader;
  Com<CompiledShader> HullShader;
  Com<CompiledShader> DomainShader;
};

Com<IMTLCompiledTessellationPipeline>
CreateTessellationPipeline(IMTLD3D11Device *pDevice,
                           MTL_GRAPHICS_PIPELINE_DESC *pDesc) {
  Com<IMTLCompiledTessellationPipeline> pipeline =
      new MTLCompiledTessellationPipeline(pDevice, pDesc);
  pipeline->SubmitWork();
  return pipeline;
}

} // namespace dxmt