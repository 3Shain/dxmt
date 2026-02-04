#include "d3d11_shader.hpp"
#include "Metal.hpp"
#include "airconv_public.h"
#include "d3d11_input_layout.hpp"
#include "sha1/sha1_util.hpp"

namespace dxmt {

template <typename Proc>
class GeneralShaderCompileTask : public CompiledShader {
public:
  GeneralShaderCompileTask(MTLD3D11Device *pDevice, ManagedShader shader,
                           Proc &&proc, std::string func_name, const Sha1Digest& variant_digest)
      : CompiledShader(), proc(std::forward<Proc>(proc)), func_name(func_name), device_(pDevice),
        shader_(shader), variant_digest_(variant_digest) {
    sm50_common.type = SM50_SHADER_COMMON;
    sm50_common.metal_version = (SM50_SHADER_METAL_VERSION)pDevice->GetDXMTDevice().metalVersion();
    sm50_common.next = nullptr;
  }

  ~GeneralShaderCompileTask() {}

  bool GetShader(MTL_COMPILED_SHADER *pShaderData) final {
    bool ret = false;
    if ((ret = ready_.load(std::memory_order_acquire))) {
      *pShaderData = {function_};
    }
    return ret;
  }

  ThreadpoolWork *
  RunThreadpoolWork() {
    auto pool = WMT::MakeAutoreleasePool();
    WMT::Reference<WMT::Error> err;
    WMT::Reference<WMT::DispatchData> lib_data = shader_->find_cached_variant(variant_digest_);

    while (lib_data != nullptr) {
      auto library = device_->GetMTLDevice().newLibrary(lib_data, err);

      if (err || !library) {
        ERR("Failed to create MTLLibrary from cache: ", err.description().getUTF8String());
        lib_data = nullptr;
        break;
      }

      function_ = library.newFunction(func_name.c_str());

      if (function_ == nullptr) {
        ERR("Failed to create MTLFunction from cache: ", func_name);
        lib_data = nullptr;
        break;
      }
      break;
    }

    if (!lib_data) {
      SM50_COMPILED_BITCODE bitcode;
      sm50_bitcode_t compile_result = proc(func_name.c_str(), &sm50_common);

      if (!compile_result)
        return this;

      SM50GetCompiledBitcode(compile_result, &bitcode);
      lib_data = WMT::MakeDispatchData(bitcode.Data, bitcode.Size);
      auto library = device_->GetMTLDevice().newLibrary(lib_data, err);

      if (err) {
        ERR("Failed to create MTLLibrary: ", err.description().getUTF8String());
        shader_->dump();
        return this;
      }

      shader_->update_cached_variant(variant_digest_, lib_data);

      SM50DestroyBitcode(compile_result);
      function_ = library.newFunction(func_name.c_str());

      if (function_ == nullptr) {
        ERR("Failed to create MTLFunction: ", func_name);
        shader_->dump();
      }
    }

    return this;
  }

  bool GetIsDone() { return ready_; }

  void SetIsDone(bool state) { ready_.store(state); }

private:
  SM50_SHADER_COMMON_DATA sm50_common;
  Proc proc;
  std::string func_name;
  MTLD3D11Device *device_;
  ManagedShader shader_;
  Sha1Digest variant_digest_;
  std::atomic_bool ready_;
  WMT::Reference<WMT::Function> function_;
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantVertex variant) {
  Sha1HashState h;
  h.update(variant.gs_passthrough);
  h.update(variant.rasterization_disabled);
  if (variant.input_layout_handle)
    h.update(variant.input_layout_handle->sha1());
  auto variant_digest = h.final();
  std::string func_name = "vs_" + shader->sha1().string().substr(0, 8) + "_" + variant_digest.string();

  auto proc = [=](const char *func_name, SM50_SHADER_COMMON_DATA *common) -> sm50_bitcode_t  {
    SM50_SHADER_IA_INPUT_LAYOUT_DATA data_ia_layout;
    SM50_SHADER_GS_PASS_THROUGH_DATA data_gs_passthrough;
    data_gs_passthrough.type = SM50_SHADER_GS_PASS_THROUGH;
    data_gs_passthrough.DataEncoded = variant.gs_passthrough;
    data_gs_passthrough.RasterizationDisabled = variant.rasterization_disabled;
    data_gs_passthrough.next = common;
    if (variant.input_layout_handle) {
      data_gs_passthrough.next = &data_ia_layout;
      data_ia_layout.type = SM50_SHADER_IA_INPUT_LAYOUT;
      data_ia_layout.next = common;
      data_ia_layout.slot_mask = variant.input_layout_handle->input_slot_mask();
      data_ia_layout
          .num_elements = variant.input_layout_handle->input_layout_element(
          (MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **)&data_ia_layout.elements);
    }

    sm50_bitcode_t compile_result = nullptr;
    sm50_error_t sm50_err = nullptr;
    if (SM50Compile(
            shader->handle(),
            (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&data_gs_passthrough,
            func_name, &compile_result, &sm50_err)) {
      ERR("Failed to compile shader: ", SM50GetErrorMessageString(sm50_err));
      SM50FreeError(sm50_err);
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc), func_name, variant_digest);
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantPixel variant) {
  Sha1HashState h;
  h.update(variant.sample_mask);
  h.update(variant.unorm_output_reg_mask);
  h.update(variant.dual_source_blending);
  h.update(variant.disable_depth_output);
  auto variant_digest = h.final();
  std::string func_name = "ps_" + shader->sha1().string().substr(0, 8) + "_" + variant_digest.string();

  auto proc = [=](const char *func_name, SM50_SHADER_COMMON_DATA *common) -> sm50_bitcode_t  {
    SM50_SHADER_PSO_PIXEL_SHADER_DATA data;
    data.type = SM50_SHADER_PSO_PIXEL_SHADER;
    data.next = common;
    data.sample_mask = variant.sample_mask;
    data.dual_source_blending = variant.dual_source_blending;
    data.disable_depth_output = variant.disable_depth_output;
    data.unorm_output_reg_mask = variant.unorm_output_reg_mask;

    sm50_bitcode_t compile_result = nullptr;
    sm50_error_t sm50_err = nullptr;
    if (SM50Compile(shader->handle(),
                               (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&data,
                               func_name, &compile_result, &sm50_err)) {
      ERR("Failed to compile shader: ", SM50GetErrorMessageString(sm50_err));
      SM50FreeError(sm50_err);
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc), func_name, variant_digest);
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantDefault) {
  std::string func_name = "shader_" + shader->sha1().string().substr(0, 8);
  auto proc = [=](const char *func_name, SM50_SHADER_COMMON_DATA *common) -> sm50_bitcode_t  {
    sm50_bitcode_t compile_result = nullptr;
    sm50_error_t sm50_err = nullptr;
    if (SM50Compile(shader->handle(), (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)common, func_name,
                               &compile_result, &sm50_err)) {
      ERR("Failed to compile shader: ", SM50GetErrorMessageString(sm50_err));
      SM50FreeError(sm50_err);
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc), func_name, shader->sha1());
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantTessellationVertexHull variant) {
  Sha1HashState h;
  h.update(variant.vertex_shader_handle->sha1());
  h.update(variant.index_buffer_format);
  h.update(variant.max_potential_tess_factor);
  if (variant.input_layout_handle)
    h.update(variant.input_layout_handle->sha1());
  auto variant_digest = h.final();
  std::string func_name = "vshs_" + shader->sha1().string().substr(0, 8) + "_" +  variant_digest.string();
  auto proc = [=](const char *func_name, SM50_SHADER_COMMON_DATA *common) -> sm50_bitcode_t  {
    SM50_SHADER_IA_INPUT_LAYOUT_DATA ia_layout;
    SM50_SHADER_PSO_TESSELLATOR_DATA pso_tess;
    ia_layout.index_buffer_format = variant.index_buffer_format;
    if (variant.input_layout_handle) {
      ia_layout.slot_mask = variant.input_layout_handle->input_slot_mask();
      ia_layout.num_elements = variant.input_layout_handle->input_layout_element(
          reinterpret_cast<MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **>(&ia_layout.elements)
      );
    } else {
      ia_layout.slot_mask = 0;
      ia_layout.num_elements = 0;
    }
    ia_layout.type = SM50_SHADER_IA_INPUT_LAYOUT;
    ia_layout.next = &pso_tess;
    pso_tess.type = SM50_SHADER_PSO_TESSELLATOR;
    pso_tess.next = common;
    pso_tess.max_potential_tess_factor = variant.max_potential_tess_factor;

    sm50_bitcode_t compile_result = nullptr;
    sm50_error_t sm50_err = nullptr;
    if (SM50CompileTessellationPipelineHull(
            variant.vertex_shader_handle->handle(), shader->handle(), 
            (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&ia_layout, func_name,
            &compile_result, &sm50_err)) {
      ERR("Failed to compile shader: ", SM50GetErrorMessageString(sm50_err));
      SM50FreeError(sm50_err);
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc), func_name, variant_digest);
}

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantTessellationDomain variant) {
  Sha1HashState h;
  h.update(variant.hull_shader_handle->sha1());
  h.update(variant.gs_passthrough);
  h.update(variant.rasterization_disabled);
  h.update(variant.max_potential_tess_factor);
  auto variant_digest = h.final();
  std::string func_name = "ds_" + shader->sha1().string().substr(0, 8) + "_" + variant_digest.string();
  auto proc = [=](const char *func_name, SM50_SHADER_COMMON_DATA *common) -> sm50_bitcode_t  {
    SM50_SHADER_GS_PASS_THROUGH_DATA gs_passthrough;
    SM50_SHADER_PSO_TESSELLATOR_DATA pso_tess;
    gs_passthrough.DataEncoded = variant.gs_passthrough;
    gs_passthrough.RasterizationDisabled = variant.rasterization_disabled;
    gs_passthrough.next = &pso_tess;
    pso_tess.type = SM50_SHADER_PSO_TESSELLATOR;
    pso_tess.next = common;
    pso_tess.max_potential_tess_factor = variant.max_potential_tess_factor;

    sm50_bitcode_t compile_result = nullptr;
    sm50_error_t sm50_err = nullptr;
    if (SM50CompileTessellationPipelineDomain(
            variant.hull_shader_handle->handle(), shader->handle(),
            (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&gs_passthrough, func_name,
            &compile_result, &sm50_err)) {
      ERR("Failed to compile shader: ", SM50GetErrorMessageString(sm50_err));
      SM50FreeError(sm50_err);
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc), func_name, variant_digest);
}

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantVertexStreamOutput variant) {
  Sha1HashState h;
  h.update(((IMTLD3D11StreamOutputLayout *)variant.stream_output_layout_handle)->Digest());
  if (variant.input_layout_handle)
    h.update(variant.input_layout_handle->sha1());
  auto variant_digest = h.final();
  std::string func_name = "vsso_" + shader->sha1().string().substr(0, 8) + "_" + variant_digest.string();
  auto proc = [=](const char *func_name, SM50_SHADER_COMMON_DATA *common) -> sm50_bitcode_t  {
    SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA data_so;
    SM50_SHADER_IA_INPUT_LAYOUT_DATA data_vertex_pulling;
    data_so.type = SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT;
    data_so.next = common;
    data_so.num_output_slots = 0;
    data_so.num_elements =
        ((IMTLD3D11StreamOutputLayout *)variant.stream_output_layout_handle)
            ->GetStreamOutputElements(
                (MTL_SHADER_STREAM_OUTPUT_ELEMENT_DESC **)&data_so.elements,
                data_so.strides);

    if (variant.input_layout_handle) {
      data_so.next = &data_vertex_pulling;
      data_vertex_pulling.type = SM50_SHADER_IA_INPUT_LAYOUT;
      data_vertex_pulling.next = common;
      data_vertex_pulling.slot_mask =
          variant.input_layout_handle->input_slot_mask();
      data_vertex_pulling.num_elements =
          variant.input_layout_handle->input_layout_element(
              (MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **)&data_vertex_pulling
                  .elements);
    }

    sm50_bitcode_t compile_result = nullptr;
    sm50_error_t sm50_err = nullptr;
    if (SM50Compile(
            shader->handle(), (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&data_so,
            func_name, &compile_result, &sm50_err)) {
      ERR("Failed to compile shader: ", SM50GetErrorMessageString(sm50_err));
      SM50FreeError(sm50_err);
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc), func_name, variant_digest);
};

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantGeometryVertex variant) {
  Sha1HashState h;
  h.update(variant.geometry_shader_handle->sha1());
  h.update(variant.index_buffer_format);
  h.update(variant.strip_topology);
  if (variant.input_layout_handle)
    h.update(variant.input_layout_handle->sha1());
  auto variant_digest = h.final();
  std::string func_name = "vsgs_" + shader->sha1().string().substr(0, 8) + "_" + variant_digest.string();
  auto proc = [=](const char *func_name, SM50_SHADER_COMMON_DATA *common) -> sm50_bitcode_t  {
    SM50_SHADER_IA_INPUT_LAYOUT_DATA ia_layout;
    ia_layout.index_buffer_format = variant.index_buffer_format;
    if (variant.input_layout_handle) {
      ia_layout.slot_mask = variant.input_layout_handle->input_slot_mask();
      ia_layout.num_elements =
          variant.input_layout_handle->input_layout_element(
              (MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **)&ia_layout.elements);
    } else {
      ia_layout.slot_mask = 0;
      ia_layout.num_elements = 0;
    }

    ia_layout.type = SM50_SHADER_IA_INPUT_LAYOUT;
    ia_layout.next = common;

    SM50_SHADER_PSO_GEOMETRY_SHADER_DATA geometry;
    geometry.type = SM50_SHADER_PSO_GEOMETRY_SHADER;
    geometry.next = &ia_layout;
    geometry.strip_topology = variant.strip_topology;

    sm50_bitcode_t compile_result = nullptr;
    sm50_error_t sm50_err = nullptr;
    if (SM50CompileGeometryPipelineVertex(
            shader->handle(), variant.geometry_shader_handle->handle(),
            (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&geometry, func_name,
            &compile_result, &sm50_err)) {
      ERR("Failed to compile shader: ", SM50GetErrorMessageString(sm50_err));
      SM50FreeError(sm50_err);
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc), func_name, variant_digest);
}

template <>
std::unique_ptr<CompiledShader>
CreateVariantShader(MTLD3D11Device *pDevice, ManagedShader shader,
                    ShaderVariantGeometry variant) {
  Sha1HashState h;
  h.update(variant.vertex_shader_handle->sha1());
  h.update(variant.strip_topology);
  auto variant_digest = h.final();
  std::string func_name = "gs_" + shader->sha1().string().substr(0, 8) + "_" + variant_digest.string();
  auto proc = [=](const char *func_name, SM50_SHADER_COMMON_DATA *common) -> sm50_bitcode_t  {
    SM50_SHADER_PSO_GEOMETRY_SHADER_DATA geometry;
    geometry.type = SM50_SHADER_PSO_GEOMETRY_SHADER;
    geometry.next = common;
    geometry.strip_topology = variant.strip_topology;

    sm50_bitcode_t compile_result = nullptr;
    sm50_error_t sm50_err = nullptr;
    if (SM50CompileGeometryPipelineGeometry(
            variant.vertex_shader_handle->handle(), shader->handle(),
            (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&geometry, func_name,
            &compile_result, &sm50_err)) {
      ERR("Failed to compile shader: ", SM50GetErrorMessageString(sm50_err));
      SM50FreeError(sm50_err);
      return nullptr;
    }
    return compile_result;
  };
  return std::make_unique<GeneralShaderCompileTask<decltype(proc)>>(
      pDevice, shader, std::move(proc), func_name, variant_digest);
}

} // namespace dxmt