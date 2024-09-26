#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "Metal/MTLLibrary.hpp"
#include "Metal/MTLPipeline.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "airconv_public.h"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_shader.hpp"
#include "log/log.hpp"

namespace dxmt {

template <typename Proc>
class GeneralShaderCompileTask : public ComObject<IMTLCompiledShader> {
public:
  GeneralShaderCompileTask(IMTLD3D11Device *pDevice, Proc &&proc)
      : ComObject<IMTLCompiledShader>(), proc(std::forward<Proc>(proc)),
        device_(pDevice) {}

  ~GeneralShaderCompileTask() {}

  void SubmitWork() { device_->SubmitThreadgroupWork(this); }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLThreadpoolWork) ||
        riid == __uuidof(IMTLCompiledShader)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  bool GetShader(MTL_COMPILED_SHADER *pShaderData) final {
    bool ret = false;
    if ((ret = ready_.load(std::memory_order_acquire))) {
      *pShaderData = {function_.ptr(), &hash_};
    }
    return ret;
  }

  void Dump() {}

  IMTLThreadpoolWork *RunThreadpoolWork() {
    function_ = transfer(proc(&hash_));
    return this;
  }

  bool GetIsDone() {
    return ready_;
  }

  void SetIsDone(bool state) {
    ready_.store(state);
  }

private:
  Proc proc;
  IMTLD3D11Device *device_;
  std::atomic_bool ready_;
  Sha1Hash hash_;
  Obj<MTL::Function> function_;
  void *compilation_args;
  SM50_SHADER_DEBUG_IDENTITY_DATA identity_data;
  uint64_t variant_id;
};

class MTLCompiledTessellationPipeline
    : public ComObject<IMTLCompiledTessellationPipeline> {
public:
  MTLCompiledTessellationPipeline(IMTLD3D11Device *pDevice,
                                  const MTL_GRAPHICS_PIPELINE_DESC *pDesc)
      : ComObject<IMTLCompiledTessellationPipeline>(),
        num_rtvs(pDesc->NumColorAttachments),
        depth_stencil_format(pDesc->DepthStencilFormat), device_(pDevice),
        pBlendState(pDesc->BlendState),
        RasterizationEnabled(pDesc->RasterizationEnabled) {
    for (unsigned i = 0; i < num_rtvs; i++) {
      rtv_formats[i] = pDesc->ColorAttachmentFormats[i];
    }
    VertexShader = new GeneralShaderCompileTask(
        pDevice,
        [vs = Com(pDesc->VertexShader), hs = Com(pDesc->HullShader), pDevice,
         index_buffer_format = pDesc->IndexBufferFormat,
         ia = pDesc->InputLayout](auto hash_) -> MTL::Function * {
          SM50_SHADER_IA_INPUT_LAYOUT_DATA ia_layout;
          ia_layout.index_buffer_format = index_buffer_format;
          ia_layout.num_elements = ia->GetInputLayoutElements(
              (MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **)&ia_layout.elements);
          ia_layout.type = SM50_SHADER_IA_INPUT_LAYOUT;
          ia_layout.next = nullptr;

          auto pool = transfer(NS::AutoreleasePool::alloc()->init());
          Obj<NS::Error> err;

          SM50CompiledBitcode *compile_result = nullptr;
          SM50Error *sm50_err = nullptr;
          std::string func_name =
              "shader_main_" + std::to_string(vs->GetUniqueId());
          if (auto ret = SM50CompileTessellationPipelineVertex(
                  (SM50Shader *)vs->GetAirconvHandle(),
                  (SM50Shader *)hs->GetAirconvHandle(),
                  (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&ia_layout,
                  func_name.c_str(), &compile_result, &sm50_err)) {
            if (ret == 42) {
              ERR("Failed to compile shader due to failed assertation");
            } else {
              ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
              SM50FreeError(sm50_err);
            }
            return nullptr;
          }
          MTL_SHADER_BITCODE bitcode;
          SM50GetCompiledBitcode(compile_result, &bitcode);
          hash_->compute(bitcode.Data, bitcode.Size);
          auto dispatch_data = dispatch_data_create(bitcode.Data, bitcode.Size,
                                                    nullptr, nullptr);
          D3D11_ASSERT(dispatch_data);
          Obj<MTL::Library> library = transfer(
              pDevice->GetMTLDevice()->newLibrary(dispatch_data, &err));

          if (err) {
            ERR("Failed to create MTLLibrary: ",
                err->localizedDescription()->utf8String());
            return nullptr;
          }

          dispatch_release(dispatch_data);
          SM50DestroyBitcode(compile_result);
          auto function_ = (library->newFunction(
              NS::String::string(func_name.c_str(), NS::UTF8StringEncoding)));
          if (function_ == nullptr) {
            ERR("Failed to create MTLFunction: ", func_name);
            return nullptr;
          }

          return function_;
        });
    HullShader = new GeneralShaderCompileTask(
        pDevice,
        [hs = Com(pDesc->HullShader), vs = Com(pDesc->VertexShader),
         pDevice](auto hash_) -> MTL::Function * {
          auto pool = transfer(NS::AutoreleasePool::alloc()->init());
          Obj<NS::Error> err;

          SM50CompiledBitcode *compile_result = nullptr;
          SM50Error *sm50_err = nullptr;
          std::string func_name =
              "shader_main_" + std::to_string(hs->GetUniqueId());
          if (auto ret = SM50CompileTessellationPipelineHull(
                  (SM50Shader *)vs->GetAirconvHandle(),
                  (SM50Shader *)hs->GetAirconvHandle(), nullptr,
                  func_name.c_str(), &compile_result, &sm50_err)) {
            if (ret == 42) {
              ERR("Failed to compile shader due to failed assertation");
            } else {
              ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
              SM50FreeError(sm50_err);
            }
            return nullptr;
          }
          MTL_SHADER_BITCODE bitcode;
          SM50GetCompiledBitcode(compile_result, &bitcode);
          hash_->compute(bitcode.Data, bitcode.Size);
          auto dispatch_data = dispatch_data_create(bitcode.Data, bitcode.Size,
                                                    nullptr, nullptr);
          D3D11_ASSERT(dispatch_data);
          Obj<MTL::Library> library = transfer(
              pDevice->GetMTLDevice()->newLibrary(dispatch_data, &err));

          if (err) {
            ERR("Failed to create MTLLibrary: ",
                err->localizedDescription()->utf8String());
            return nullptr;
          }

          dispatch_release(dispatch_data);
          SM50DestroyBitcode(compile_result);
          auto function_ = (library->newFunction(
              NS::String::string(func_name.c_str(), NS::UTF8StringEncoding)));
          if (function_ == nullptr) {
            ERR("Failed to create MTLFunction: ", func_name);
            return nullptr;
          }

          return function_;
        });
    DomainShader = new GeneralShaderCompileTask(
        pDevice,
        [hs = Com(pDesc->HullShader), ds = Com(pDesc->DomainShader),
         pDevice](auto hash_) -> MTL::Function * {
          auto pool = transfer(NS::AutoreleasePool::alloc()->init());
          Obj<NS::Error> err;

          SM50CompiledBitcode *compile_result = nullptr;
          SM50Error *sm50_err = nullptr;
          std::string func_name =
              "shader_main_" + std::to_string(ds->GetUniqueId());
          if (auto ret = SM50CompileTessellationPipelineDomain(
                  (SM50Shader *)hs->GetAirconvHandle(),
                  (SM50Shader *)ds->GetAirconvHandle(), nullptr,
                  func_name.c_str(), &compile_result, &sm50_err)) {
            if (ret == 42) {
              ERR("Failed to compile shader due to failed assertation");
            } else {
              ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
              SM50FreeError(sm50_err);
            }
            return nullptr;
          }
          MTL_SHADER_BITCODE bitcode;
          SM50GetCompiledBitcode(compile_result, &bitcode);
          hash_->compute(bitcode.Data, bitcode.Size);
          auto dispatch_data = dispatch_data_create(bitcode.Data, bitcode.Size,
                                                    nullptr, nullptr);
          D3D11_ASSERT(dispatch_data);
          Obj<MTL::Library> library = transfer(
              pDevice->GetMTLDevice()->newLibrary(dispatch_data, &err));

          if (err) {
            ERR("Failed to create MTLLibrary: ",
                err->localizedDescription()->utf8String());
            return nullptr;
          }

          dispatch_release(dispatch_data);
          SM50DestroyBitcode(compile_result);
          auto function_ = (library->newFunction(
              NS::String::string(func_name.c_str(), NS::UTF8StringEncoding)));
          if (function_ == nullptr) {
            ERR("Failed to create MTLFunction: ", func_name);
            return nullptr;
          }

          return function_;
        });
    if (pDesc->PixelShader) {
      pDesc->PixelShader->GetCompiledPixelShader(
          pDesc->SampleMask, pDesc->BlendState->IsDualSourceBlending(),
          &PixelShader);
    }
    hull_reflection = *pDesc->HullShader->GetReflection();
    VertexShader->SubmitWork();
    HullShader->SubmitWork();
    DomainShader->SubmitWork();
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

  IMTLThreadpoolWork* RunThreadpoolWork() {

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

    for (unsigned i = 0; i < num_rtvs; i++) {
      if (rtv_formats[i] == MTL::PixelFormatInvalid)
        continue;
      mesh_pipeline_desc->colorAttachments()->object(i)->setPixelFormat(
          rtv_formats[i]);
    }

    if (depth_stencil_format != MTL::PixelFormatInvalid) {
      mesh_pipeline_desc->setDepthAttachmentPixelFormat(depth_stencil_format);
    }
    // FIXME: don't hardcoding!
    if (depth_stencil_format == MTL::PixelFormatDepth32Float_Stencil8 ||
        depth_stencil_format == MTL::PixelFormatDepth24Unorm_Stencil8 ||
        depth_stencil_format == MTL::PixelFormatStencil8) {
      mesh_pipeline_desc->setStencilAttachmentPixelFormat(depth_stencil_format);
    }

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

    state_rasterization_ =
        transfer(device_->GetMTLDevice()->newRenderPipelineState(
            pipelineDescriptor, &err));

    return this;
  }

  bool GetIsDone() {
    return ready_;
  }

  void SetIsDone(bool state) {
    ready_.store(state);
    ready_.notify_all();
  }

private:
  UINT num_rtvs;
  MTL::PixelFormat rtv_formats[8];
  MTL::PixelFormat depth_stencil_format;
  IMTLD3D11Device *device_;
  std::atomic_bool ready_;
  IMTLD3D11BlendState *pBlendState;
  Obj<MTL::RenderPipelineState> state_mesh_;
  Obj<MTL::RenderPipelineState> state_rasterization_;
  bool RasterizationEnabled;

  MTL_SHADER_REFLECTION hull_reflection;

  Com<IMTLCompiledShader> VertexShader;
  Com<IMTLCompiledShader> PixelShader;
  Com<IMTLCompiledShader> HullShader;
  Com<IMTLCompiledShader> DomainShader;
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