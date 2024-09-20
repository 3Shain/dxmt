#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "air_operations.hpp"
#include "air_signature.hpp"
#include "airconv_public.h"
#include "dxbc_constants.hpp"
#include "dxbc_instructions.hpp"
#include "shader_common.hpp"

namespace dxmt::dxbc {

struct ResourceRange {
  uint32_t range_id;
  uint32_t lower_bound;
  uint32_t size;
  uint32_t space;
};

struct ShaderResourceViewInfo {
  ResourceRange range;
  shader::common::ScalerDataType scaler_type;
  shader::common::ResourceType resource_type;
  bool read = false;
  bool sampled = false;
  bool compared = false; // therefore we use depth texture!

  uint32_t strucure_stride = 0;
  uint32_t arg_index;
  uint32_t arg_size_index;
};
struct UnorderedAccessViewInfo {
  ResourceRange range;
  shader::common::ScalerDataType scaler_type;
  shader::common::ResourceType resource_type;
  bool read = false;
  bool written = false;
  bool global_coherent = false;
  bool rasterizer_order = false;
  bool with_counter = false;

  uint32_t strucure_stride = 0;
  uint32_t arg_index;
  uint32_t arg_size_index;
  uint32_t arg_counter_index;
};
struct ConstantBufferInfo {
  ResourceRange range;
  uint32_t size_in_vec4;
  uint32_t arg_index;
};
struct SamplerInfo {
  ResourceRange range;
  uint32_t arg_index;
};

struct ThreadgroupBufferInfo {
  uint32_t size_in_uint;
  uint32_t size;
  uint32_t stride;
  bool structured;
};

struct PhaseInfo {
  uint32_t tempRegisterCount = 0;
  std::unordered_map<
    uint32_t, std::pair<uint32_t /* count */, uint32_t /* mask */>>
    indexableTempRegisterCounts;
};

class ShaderInfo {
public:
  std::vector<std::array<uint32_t, 4>> immConstantBufferData;
  std::map<uint32_t, ShaderResourceViewInfo> srvMap;
  std::map<uint32_t, UnorderedAccessViewInfo> uavMap;
  std::map<uint32_t, ConstantBufferInfo> cbufferMap;
  std::map<uint32_t, SamplerInfo> samplerMap;
  std::map<uint32_t, ThreadgroupBufferInfo> tgsmMap;
  uint32_t tempRegisterCount = 0;
  std::unordered_map<
    uint32_t, std::pair<uint32_t /* count */, uint32_t /* mask */>>
    indexableTempRegisterCounts;
  air::ArgumentBufferBuilder binding_table_cbuffer;
  air::ArgumentBufferBuilder binding_table;
  bool skipOptimization = false;
  bool refactoringAllowed = true;
  bool use_cmp_exch = false;
  bool no_control_point_phase_passthrough = false;
  bool output_control_point_read = false;
  std::vector<PhaseInfo> phases;
};

Instruction readInstruction(
  const microsoft::D3D10ShaderBinary::CInstruction &Inst,
  ShaderInfo &shader_info, uint32_t phase
);

using pvalue = dxmt::air::pvalue;
using epvalue = llvm::Expected<pvalue>;
using dxbc::Swizzle;
using dxbc::swizzle_identity;

struct context;
using IRValue = ReaderIO<context, pvalue>;
using IREffect = ReaderIO<context, std::monostate>;
using IndexedIRValue = std::function<IRValue(pvalue)>;

struct register_file {
  llvm::Value *ptr_int4 = nullptr;
  llvm::Value *ptr_float4 = nullptr;
};

struct indexable_register_file {
  llvm::Value *ptr_int_vec = nullptr;
  llvm::Value *ptr_float_vec = nullptr;
  uint32_t vec_size = 0;
};

struct phase_temp {
  register_file temp{};
  std::unordered_map<uint32_t, indexable_register_file> indexable_temp_map{};
};

struct io_binding_map {
  llvm::GlobalVariable *icb = nullptr;
  llvm::Value *icb_float = nullptr;
  std::unordered_map<uint32_t, IndexedIRValue> cb_range_map{};
  std::unordered_map<uint32_t, IndexedIRValue> sampler_range_map{};
  std::unordered_map<uint32_t, std::tuple<air::MSLTexture, IndexedIRValue>>
    srv_range_map{};
  std::unordered_map<
    uint32_t, std::tuple<IndexedIRValue, IndexedIRValue, uint32_t>>
    srv_buf_range_map{};
  std::unordered_map<uint32_t, std::tuple<air::MSLTexture, IndexedIRValue>>
    uav_range_map{};
  std::unordered_map<
    uint32_t, std::tuple<IndexedIRValue, IndexedIRValue, uint32_t>>
    uav_buf_range_map{};
  std::unordered_map<uint32_t, std::pair<uint32_t, llvm::GlobalVariable *>>
    tgsm_map{};
  std::unordered_map<uint32_t, IndexedIRValue> uav_counter_range_map{};

  register_file input{};
  register_file output{};
  register_file temp{};
  std::unordered_map<uint32_t, indexable_register_file> indexable_temp_map{};
  std::vector<phase_temp> phases;
  register_file patch_constant_output{};
  uint32_t input_element_count = 0;
  uint32_t output_element_count = 0;

  // special registers (input)
  llvm::Value *thread_id_arg = nullptr;
  llvm::Value *thread_group_id_arg = nullptr;
  llvm::Value *thread_id_in_group_arg = nullptr;
  llvm::Value *thread_id_in_group_flat_arg = nullptr;
  llvm::Value *coverage_mask_arg = nullptr;

  llvm::Value *domain = nullptr;
  llvm::Value *patch_id = nullptr;
  llvm::Value *instanced_patch_id = nullptr;

  llvm::Value *thread_id_in_patch = nullptr;

  // special registers (output)
  llvm::AllocaInst *depth_output_reg = nullptr;
  llvm::AllocaInst *stencil_ref_reg = nullptr;
  llvm::AllocaInst *coverage_mask_reg = nullptr;

  llvm::AllocaInst *cmp_exch_temp = nullptr;

  // special buffers for tessellation
  llvm::Value *control_point_buffer;  // int4*
  llvm::Value *patch_constant_buffer; // int*
  llvm::Value *tess_factor_buffer;    // half*

  // temp for fast look-up
  llvm::Value *vertex_id = nullptr;
  llvm::Value *vertex_id_with_base = nullptr;
  llvm::Value *instance_id = nullptr;
  llvm::Value *instance_id_with_base = nullptr;
  llvm::Value *base_vertex_id = nullptr;
  llvm::Value *base_instance_id = nullptr;
  llvm::Value *vertex_buffer_table = nullptr;
};

struct context {
  llvm::IRBuilder<> &builder;
  llvm::LLVMContext &llvm;
  llvm::Module &module;
  llvm::Function *function;
  io_binding_map &resource;
  air::AirType &types; // hmmm
  uint32_t pso_sample_mask;
  microsoft::D3D10_SB_TOKENIZED_PROGRAM_TYPE shader_type;
};

template <typename S> IRValue make_irvalue(S &&fs) {
  return IRValue(std::forward<S>(fs));
}

template <typename S> IRValue make_irvalue_bind(S &&fs) {
  return IRValue([fs = std::forward<S>(fs)](auto ctx) {
    return fs(ctx).build(ctx);
  });
}

template <typename S> IREffect make_effect(S &&fs) {
  return IREffect(std::forward<S>(fs));
}

template <typename S> IREffect make_effect_bind(S &&fs) {
  return IREffect([fs = std::forward<S>(fs)](auto ctx) mutable {
    return fs(ctx).build(ctx);
  });
}

IREffect store_at_vec4_array_masked(
  llvm::Value *array, pvalue index, pvalue maybe_vec4, uint32_t mask
);

IREffect init_input_reg(
  uint32_t with_fnarg_at, uint32_t to_reg, uint32_t mask,
  bool fix_w_component = false
);

std::function<IRValue(pvalue)>
pop_output_reg(uint32_t from_reg, uint32_t mask, uint32_t to_element);

std::function<IRValue(pvalue)> pop_output_tess_factor(
  uint32_t from_reg, uint32_t mask, uint32_t to_factor_indx, uint32_t factor_num
);

IREffect pull_vertex_input(
  air::FunctionSignatureBuilder *func_signature, uint32_t to_reg, uint32_t mask,
  SM50_IA_INPUT_ELEMENT element_info
);

llvm::Expected<llvm::BasicBlock *> convert_basicblocks(
  std::shared_ptr<BasicBlock> entry, context &ctx, llvm::BasicBlock *return_bb
);

constexpr air::MSLScalerOrVectorType to_msl_type(RegisterComponentType type) {
  switch (type) {
  case RegisterComponentType::Unknown: {
    assert(0 && "unknown component type");
    break;
  }
  case RegisterComponentType::Uint:
    return air::msl_uint4;
  case RegisterComponentType::Int:
    return air::msl_int4;
  case RegisterComponentType::Float:
    return air::msl_float4;
    break;
  }
}

struct ScalarInfo {
  uint8_t component : 2;
  uint8_t reg : 6;
};

class SM50ShaderInternal {
public:
  dxmt::dxbc::ShaderInfo shader_info;
  dxmt::air::FunctionSignatureBuilder func_signature;
  std::shared_ptr<dxmt::dxbc::BasicBlock> entry;
  std::vector<std::function<
    void(dxmt::dxbc::IREffect &, dxmt::air::FunctionSignatureBuilder *, SM50_SHADER_IA_INPUT_LAYOUT_DATA *)>>
    input_prelogue_;
  std::vector<std::function<void(dxmt::dxbc::IRValue &, dxmt::air::FunctionSignatureBuilder *,  bool)>> epilogue_;
  microsoft::D3D10_SB_TOKENIZED_PROGRAM_TYPE shader_type;
  /* for domain shader, it refers to patch constant input count */
  uint32_t max_input_register = 0;
  uint32_t max_output_register = 0;
  uint32_t max_patch_constant_output_register = 0;
  std::vector<MTL_SM50_SHADER_ARGUMENT> args_reflection_cbuffer;
  std::vector<MTL_SM50_SHADER_ARGUMENT> args_reflection;
  uint32_t threadgroup_size[3] = {0};
  uint32_t input_control_point_count = ~0u;
  uint32_t output_control_point_count = ~0u;
  uint32_t tessellation_partition = 0;
  float max_tesselation_factor = 64.0f;
  bool tessellation_anticlockwise = false;
  std::vector<ScalarInfo> patch_constant_scalars;
  uint32_t hull_maximum_threads_per_patch = 0;
  std::vector<ScalarInfo> clip_distance_scalars;
};

llvm::Error convert_dxbc_hull_shader(
  SM50ShaderInternal *pShaderInternal, const char *name,
  SM50ShaderInternal *pVertexStage, llvm::LLVMContext &context,
  llvm::Module &module, SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
);

llvm::Error convert_dxbc_domain_shader(
  SM50ShaderInternal *pShaderInternal, const char *name,
  SM50ShaderInternal *pHullStage, llvm::LLVMContext &context,
  llvm::Module &module, SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
);

llvm::Error convert_dxbc_vertex_for_hull_shader(
  const SM50ShaderInternal *pShaderInternal, const char *name,
  const SM50ShaderInternal *pHullStage,
  llvm::LLVMContext &context, llvm::Module &module,
  SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
);
} // namespace dxmt::dxbc
