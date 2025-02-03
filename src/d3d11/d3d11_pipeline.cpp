#include "Metal/MTLComputePipeline.hpp"
#include "d3d11_private.h"
#include "d3d11_pipeline.hpp"
#include "com/com_object.hpp"
#include "d3d11_device.hpp"
#include "d3d11_shader.hpp"
#include "log/log.hpp"
#include "objc_pointer.hpp"
#include <atomic>
#include <cassert>

namespace dxmt {

class MTLCompiledGraphicsPipeline
    : public ComObject<IMTLCompiledGraphicsPipeline> {
public:
  MTLCompiledGraphicsPipeline(MTLD3D11Device *pDevice,
                              MTL_GRAPHICS_PIPELINE_DESC *pDesc)
      : ComObject<IMTLCompiledGraphicsPipeline>(),
        num_rtvs(pDesc->NumColorAttachments),
        depth_stencil_format(pDesc->DepthStencilFormat),
        topology_class(pDesc->TopologyClass), device_(pDevice),
        pBlendState(pDesc->BlendState),
        RasterizationEnabled(pDesc->RasterizationEnabled),
        SampleCount(pDesc->SampleCount) {
    uint32_t unorm_output_reg_mask = 0;
    for (unsigned i = 0; i < num_rtvs; i++) {
      rtv_formats[i] = pDesc->ColorAttachmentFormats[i];
      unorm_output_reg_mask |= (uint32_t(IsUnorm8RenderTargetFormat(pDesc->ColorAttachmentFormats[i])) << i);
    }

    if (pDesc->SOLayout) {
      VertexShader =
          pDesc->VertexShader->get_shader(ShaderVariantVertexStreamOutput{
              (uint64_t)pDesc->InputLayout, (uint64_t)pDesc->SOLayout});
    } else {
      VertexShader = pDesc->VertexShader->get_shader(ShaderVariantVertex{
          (uint64_t)pDesc->InputLayout, pDesc->GSPassthrough, !pDesc->RasterizationEnabled});
    }

    if (pDesc->PixelShader) {
      PixelShader = pDesc->PixelShader->get_shader(ShaderVariantPixel{
          pDesc->SampleMask, pDesc->BlendState->IsDualSourceBlending(),
          depth_stencil_format == MTL::PixelFormatInvalid,
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
    *pPipeline = {state_.ptr()};
  }

  IMTLThreadpoolWork *RunThreadpoolWork() {

    TRACE("Start compiling 1 PSO");

    Obj<NS::Error> err;
    MTL_COMPILED_SHADER vs, ps;
    if (!VertexShader->GetShader(&vs)) {
      return VertexShader.ptr();
    }
    if (PixelShader && !PixelShader->GetShader(&ps)) {
      return PixelShader.ptr();
    }

    auto pipelineDescriptor =
        transfer(MTL::RenderPipelineDescriptor::alloc()->init());

    pipelineDescriptor->setVertexFunction(vs.Function);

    if (PixelShader) {
      pipelineDescriptor->setFragmentFunction(ps.Function);
    }
    pipelineDescriptor->setRasterizationEnabled(RasterizationEnabled);

    if (pBlendState) {
      pBlendState->SetupMetalPipelineDescriptor(pipelineDescriptor, num_rtvs);
    }

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
    pipelineDescriptor->setRasterSampleCount(SampleCount);

    state_ = transfer(device_->GetMTLDevice()->newRenderPipelineState(
        pipelineDescriptor, &err));

    if (state_ == nullptr) {
      ERR("Failed to create PSO: ", err->localizedDescription()->utf8String());
      return this;
    }

    TRACE("Compiled 1 PSO");

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
  MTLD3D11Device *device_;
  std::atomic_bool ready_;
  Com<CompiledShader> VertexShader;
  Com<CompiledShader> PixelShader;
  IMTLD3D11BlendState *pBlendState;
  Obj<MTL::RenderPipelineState> state_;
  bool RasterizationEnabled;
  UINT SampleCount;
};

Com<IMTLCompiledGraphicsPipeline>
CreateGraphicsPipeline(MTLD3D11Device *pDevice,
                       MTL_GRAPHICS_PIPELINE_DESC *pDesc) {
  Com<IMTLCompiledGraphicsPipeline> pipeline =
      new MTLCompiledGraphicsPipeline(pDevice, pDesc);
  pipeline->SubmitWork();
  return pipeline;
}

class MTLCompiledComputePipeline
    : public ComObject<IMTLCompiledComputePipeline> {
public:
  MTLCompiledComputePipeline(MTLD3D11Device *pDevice, ManagedShader shader)
      : ComObject<IMTLCompiledComputePipeline>(), device_(pDevice) {
    ComputeShader = shader->get_shader(ShaderVariantDefault{});
  }

  void SubmitWork() final { device_->SubmitThreadgroupWork(this); }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLThreadpoolWork) ||
        riid == __uuidof(IMTLCompiledComputePipeline)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  bool IsReady() final { return ready_.load(std::memory_order_relaxed); }

  void GetPipeline(MTL_COMPILED_COMPUTE_PIPELINE *pPipeline) final {
    ready_.wait(false, std::memory_order_acquire);
    *pPipeline = {state_.ptr()};
  }

  IMTLThreadpoolWork *RunThreadpoolWork() {
    D3D11_ASSERT(!ready_ && "?wtf"); // TODO: should use a lock?

    TRACE("Start compiling 1 PSO");

    Obj<NS::Error> err;
    MTL_COMPILED_SHADER cs;
    if (!ComputeShader->GetShader(&cs)) {
      return ComputeShader.ptr();
    }

    auto desc = transfer(MTL::ComputePipelineDescriptor::alloc()->init());
    desc->setComputeFunction(cs.Function);

    state_ = transfer(device_->GetMTLDevice()->newComputePipelineState(
        desc, 0, nullptr, &err));

    if (state_ == nullptr) {
      ERR("Failed to create compute PSO: ",
          err->localizedDescription()->utf8String());
      return this;
    }

    TRACE("Compiled 1 PSO");

    return this;
  }

  bool GetIsDone() { return ready_; }

  void SetIsDone(bool state) {
    ready_.store(state);
    ready_.notify_all();
  }

private:
  MTLD3D11Device *device_;
  std::atomic_bool ready_;
  Com<CompiledShader> ComputeShader;
  Obj<MTL::ComputePipelineState> state_;
};

Com<IMTLCompiledComputePipeline>
CreateComputePipeline(MTLD3D11Device *pDevice, ManagedShader ComputeShader) {
  Com<IMTLCompiledComputePipeline> pipeline =
      new MTLCompiledComputePipeline(pDevice, ComputeShader);
  pipeline->SubmitWork();
  return pipeline;
}

} // namespace dxmt