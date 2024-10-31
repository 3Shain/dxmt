#include "d3d11_shader.hpp"
#include "Metal/MTLLibrary.hpp"
#include "airconv_public.h"
#include "d3d11_input_layout.hpp"

namespace dxmt {

template <typename Proc>
class GeneralShaderCompileTask : public CompiledShader {
public:
  GeneralShaderCompileTask(MTLD3D11Device *pDevice, ManagedShader shader,
                           Proc &&proc)
      : CompiledShader(), proc(std::forward<Proc>(proc)), device_(pDevice),
        shader_(shader) {}

  ~GeneralShaderCompileTask() {}

  ULONG STDMETHODCALLTYPE AddRef() {
    uint32_t refCount = m_refCount++;
    return refCount + 1;
  }

  ULONG STDMETHODCALLTYPE Release() {
    uint32_t refCount = --m_refCount;
    return refCount;
  }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLThreadpoolWork)) {
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

  IMTLThreadpoolWork *RunThreadpoolWork() {
    auto pool = transfer(NS::AutoreleasePool::alloc()->init());
    Obj<NS::Error> err;
    std::string func_name = "shader_main_" + std::to_string(shader_->id());
    SM50CompiledBitcode *compile_result = proc(func_name.c_str());

    if (!compile_result)
      return this;

    MTL_SHADER_BITCODE bitcode;
    SM50GetCompiledBitcode(compile_result, &bitcode);
    hash_.compute(bitcode.Data, bitcode.Size);
    auto dispatch_data =
        dispatch_data_create(bitcode.Data, bitcode.Size, nullptr, nullptr);
    D3D11_ASSERT(dispatch_data);
    Obj<MTL::Library> library =
        transfer(device_->GetMTLDevice()->newLibrary(dispatch_data, &err));

    if (err) {
      ERR("Failed to create MTLLibrary: ",
          err->localizedDescription()->utf8String());
      return this;
    }

    dispatch_release(dispatch_data);
    SM50DestroyBitcode(compile_result);
    function_ = transfer(library->newFunction(
        NS::String::string(func_name.c_str(), NS::UTF8StringEncoding)));
    if (function_ == nullptr) {
      ERR("Failed to create MTLFunction: ", func_name);
    }

    return this;
  }

  bool GetIsDone() { return ready_; }

  void SetIsDone(bool state) { ready_.store(state); }

private:
  Proc proc;
  MTLD3D11Device *device_;
  ManagedShader shader_;
  std::atomic_bool ready_;
  Sha1Hash hash_;
  Obj<MTL::Function> function_;
  std::atomic<uint32_t> m_refCount = {0ul};
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantVertex variant) {

  auto proc = [=](const char *func_name) -> SM50CompiledBitcode * {
    SM50_SHADER_IA_INPUT_LAYOUT_DATA data_ia_layout;
    SM50_SHADER_GS_PASS_THROUGH_DATA data_gs_passthrough;
    data_gs_passthrough.type = SM50_SHADER_GS_PASS_THROUGH;
    data_gs_passthrough.DataEncoded = variant.gs_passthrough;
    data_gs_passthrough.RasterizationDisabled = variant.rasterization_disabled;
    data_gs_passthrough.next = nullptr;
    if (variant.input_layout_handle) {
      data_gs_passthrough.next = &data_ia_layout;
      data_ia_layout.type = SM50_SHADER_IA_INPUT_LAYOUT;
      data_ia_layout.next = nullptr;
      data_ia_layout.slot_mask =
          ((ManagedInputLayout)variant.input_layout_handle)->input_slot_mask();
      data_ia_layout.num_elements =
          ((ManagedInputLayout)variant.input_layout_handle)
              ->input_layout_element((MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC *
                                          *)&data_ia_layout.elements);
    }

    SM50CompiledBitcode *compile_result = nullptr;
    SM50Error *sm50_err = nullptr;
    if (auto ret = SM50Compile(
            shader->handle(),
            (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&data_gs_passthrough,
            func_name, &compile_result, &sm50_err)) {
      if (ret == 42) {
        ERR("Failed to compile shader due to failed assertion");
      } else {
        ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
        SM50FreeError(sm50_err);
      }
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc));
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantPixel variant) {
  auto proc = [=](const char *func_name) -> SM50CompiledBitcode * {
    SM50_SHADER_PSO_PIXEL_SHADER_DATA data;
    data.type = SM50_SHADER_PSO_PIXEL_SHADER;
    data.next = nullptr;
    data.sample_mask = variant.sample_mask;
    data.dual_source_blending = variant.dual_source_blending;
    data.disable_depth_output = variant.disable_depth_output;

    SM50CompiledBitcode *compile_result = nullptr;
    SM50Error *sm50_err = nullptr;
    if (auto ret = SM50Compile(shader->handle(),
                               (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&data,
                               func_name, &compile_result, &sm50_err)) {
      if (ret == 42) {
        ERR("Failed to compile shader due to failed assertion");
      } else {
        ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
        SM50FreeError(sm50_err);
      }
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc));
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantDefault) {
  auto proc = [=](const char *func_name) -> SM50CompiledBitcode * {
    SM50CompiledBitcode *compile_result = nullptr;
    SM50Error *sm50_err = nullptr;
    if (auto ret = SM50Compile(shader->handle(), nullptr, func_name,
                               &compile_result, &sm50_err)) {
      if (ret == 42) {
        ERR("Failed to compile shader due to failed assertion");
      } else {
        ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
        SM50FreeError(sm50_err);
      }
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc));
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantTessellationVertex variant) {
  auto proc = [=](const char *func_name) -> SM50CompiledBitcode * {
    SM50_SHADER_IA_INPUT_LAYOUT_DATA ia_layout;
    ia_layout.index_buffer_format = variant.index_buffer_format;
    ia_layout.slot_mask =
        ((ManagedInputLayout)variant.input_layout_handle)->input_slot_mask();
    ia_layout.num_elements =
        ((ManagedInputLayout)variant.input_layout_handle)
            ->input_layout_element(
                (MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **)&ia_layout.elements);
    ia_layout.type = SM50_SHADER_IA_INPUT_LAYOUT;
    ia_layout.next = nullptr;

    SM50CompiledBitcode *compile_result = nullptr;
    SM50Error *sm50_err = nullptr;
    if (auto ret = SM50CompileTessellationPipelineVertex(
            shader->handle(), (SM50Shader *)variant.hull_shader_handle,
            (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&ia_layout, func_name,
            &compile_result, &sm50_err)) {
      if (ret == 42) {
        ERR("Failed to compile shader due to failed assertion");
      } else {
        ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
        SM50FreeError(sm50_err);
      }
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc));
}

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantTessellationHull variant) {
  auto proc = [=](const char *func_name) -> SM50CompiledBitcode * {
    SM50CompiledBitcode *compile_result = nullptr;
    SM50Error *sm50_err = nullptr;
    if (auto ret = SM50CompileTessellationPipelineHull(
            (SM50Shader *)variant.vertex_shader_handle, shader->handle(),
            nullptr, func_name, &compile_result, &sm50_err)) {
      if (ret == 42) {
        ERR("Failed to compile shader due to failed assertion");
      } else {
        ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
        SM50FreeError(sm50_err);
      }
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc));
}

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantTessellationDomain variant) {
  auto proc = [=](const char *func_name) -> SM50CompiledBitcode * {
    SM50_SHADER_GS_PASS_THROUGH_DATA gs_passthrough;
    gs_passthrough.DataEncoded = variant.gs_passthrough;
    gs_passthrough.RasterizationDisabled = variant.rasterization_disabled;
    gs_passthrough.next = nullptr;

    SM50CompiledBitcode *compile_result = nullptr;
    SM50Error *sm50_err = nullptr;
    if (auto ret = SM50CompileTessellationPipelineDomain(
            (SM50Shader *)variant.hull_shader_handle, shader->handle(),
            (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&gs_passthrough, func_name,
            &compile_result, &sm50_err)) {
      if (ret == 42) {
        ERR("Failed to compile shader due to failed assertion");
      } else {
        ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
        SM50FreeError(sm50_err);
      }
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc));
}

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantVertexStreamOutput variant) {

  auto proc = [=](const char *func_name) -> SM50CompiledBitcode * {
    SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA data_so;
    SM50_SHADER_IA_INPUT_LAYOUT_DATA data_vertex_pulling;
    data_so.type = SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT;
    data_so.next = nullptr;
    data_so.num_output_slots = 0;
    data_so.num_elements =
        ((IMTLD3D11StreamOutputLayout *)variant.stream_output_layout_handle)
            ->GetStreamOutputElements(
                (MTL_SHADER_STREAM_OUTPUT_ELEMENT_DESC **)&data_so.elements,
                data_so.strides);

    if (variant.input_layout_handle) {
      data_so.next = &data_vertex_pulling;
      data_vertex_pulling.type = SM50_SHADER_IA_INPUT_LAYOUT;
      data_vertex_pulling.next = nullptr;
      data_vertex_pulling.slot_mask =
          ((ManagedInputLayout)variant.input_layout_handle)->input_slot_mask();
      data_vertex_pulling.num_elements =
          ((ManagedInputLayout)variant.input_layout_handle)
              ->input_layout_element((MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC *
                                          *)&data_vertex_pulling.elements);
    }

    SM50CompiledBitcode *compile_result = nullptr;
    SM50Error *sm50_err = nullptr;
    if (auto ret = SM50Compile(
            shader->handle(), (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&data_so,
            func_name, &compile_result, &sm50_err)) {
      if (ret == 42) {
        ERR("Failed to compile shader due to failed assertion");
      } else {
        ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
        SM50FreeError(sm50_err);
      }
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc));
};

} // namespace dxmt