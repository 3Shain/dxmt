#include "dxbc_converter.hpp"
#include "DXBCParser/BlobContainer.h"
#include "DXBCParser/ShaderBinary.h"
#include "airconv_error.hpp"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/FMF.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include <bit>
#include <memory>
#include <string>
#include <vector>

#include "metallib_writer.hpp"
#include "nt/air_builder.hpp"
#include "shader_common.hpp"

#include "airconv_context.hpp"

#include "ftl.hpp"

class SM50CompiledBitcodeInternal {
public:
  llvm::SmallVector<char, 0> vec;
};

class SM50ErrorInternal {
public:
  llvm::SmallVector<char, 0> buf;
};

namespace dxmt::dxbc {

inline dxmt::shader::common::ResourceType
to_shader_resource_type(microsoft::D3D10_SB_RESOURCE_DIMENSION dim) {
  switch (dim) {
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_UNKNOWN:
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_BUFFER:
  case microsoft::D3D11_SB_RESOURCE_DIMENSION_RAW_BUFFER:
  case microsoft::D3D11_SB_RESOURCE_DIMENSION_STRUCTURED_BUFFER:
    return shader::common::ResourceType::TextureBuffer;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE1D:
    return shader::common::ResourceType::Texture1D;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D:
    return shader::common::ResourceType::Texture2D;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMS:
    return shader::common::ResourceType::Texture2DMultisampled;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE3D:
    return shader::common::ResourceType::Texture3D;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBE:
    return shader::common::ResourceType::TextureCube;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE1DARRAY:
    return shader::common::ResourceType::Texture1DArray;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DARRAY:
    return shader::common::ResourceType::Texture2DArray;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
    return shader::common::ResourceType::Texture2DMultisampledArray;
  case microsoft::D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBEARRAY:
    return shader::common::ResourceType::TextureCubeArray;
  }
  assert(0 && "invalid D3D10_SB_RESOURCE_DIMENSION");
};

inline dxmt::shader::common::ScalerDataType
to_shader_scaler_type(microsoft::D3D10_SB_RESOURCE_RETURN_TYPE type) {
  switch (type) {

  case microsoft::D3D10_SB_RETURN_TYPE_UNORM:
  case microsoft::D3D10_SB_RETURN_TYPE_SNORM:
  case microsoft::D3D10_SB_RETURN_TYPE_FLOAT:
    return shader::common::ScalerDataType::Float;
  case microsoft::D3D10_SB_RETURN_TYPE_SINT:
    return shader::common::ScalerDataType::Int;
  case microsoft::D3D10_SB_RETURN_TYPE_UINT:
    return shader::common::ScalerDataType::Uint;
  case microsoft::D3D10_SB_RETURN_TYPE_MIXED:
    return shader::common::ScalerDataType::Uint; // kinda weird but ok
  case microsoft::D3D11_SB_RETURN_TYPE_DOUBLE:
  case microsoft::D3D11_SB_RETURN_TYPE_CONTINUED:
  case microsoft::D3D11_SB_RETURN_TYPE_UNUSED:
    break;
  }
  assert(0 && "invalid D3D10_SB_RESOURCE_RETURN_TYPE");
}

uint32_t next_pow2(uint32_t x) {
  return x == 1 ? 1 : 1 << (32 - __builtin_clz(x - 1));
}

auto get_item_in_argbuf_binding_table(uint32_t argbuf_index, uint32_t index) {
  return make_irvalue([=](context ctx) {
    auto argbuf = ctx.function->getArg(argbuf_index);
    auto argbuf_struct_type = llvm::cast<llvm::StructType>(
      llvm::cast<llvm::PointerType>(argbuf->getType())
        ->getNonOpaquePointerElementType()
    );
    return ctx.builder.CreateLoad(
      argbuf_struct_type->getElementType(index),
      ctx.builder.CreateStructGEP(
        llvm::cast<llvm::PointerType>(argbuf->getType())
          ->getNonOpaquePointerElementType(),
        argbuf, index
      )
    );
  });
};

void setup_binding_table(
  const ShaderInfo *shader_info, io_binding_map &resource_map,
  air::FunctionSignatureBuilder &func_signature, llvm::Module &module,
  uint32_t argbuffer_constant_slot, uint32_t argbuffer_slot
) {
  uint32_t binding_table_index = ~0u;
  uint32_t cbuf_table_index = ~0u;
  if (!shader_info->binding_table.Empty()) {
    auto [type, metadata] = shader_info->binding_table.Build(
      module.getContext(), module.getDataLayout()
    );
    std::string arg_name = "binding_table";
    if (argbuffer_slot != kArgumentBufferBindIndex)
      arg_name += std::to_string(argbuffer_slot);
    binding_table_index =
      func_signature.DefineInput(air::ArgumentBindingIndirectBuffer{
        .location_index = argbuffer_slot, // kArgumentBufferBindIndex
        .array_size = 1,
        .memory_access = air::MemoryAccess::read,
        .address_space = air::AddressSpace::constant,
        .struct_type = type,
        .struct_type_info = metadata,
        .arg_name = arg_name,
      });
  }
  if (!shader_info->binding_table_cbuffer.Empty()) {
    auto [type, metadata] = shader_info->binding_table_cbuffer.Build(
      module.getContext(), module.getDataLayout()
    );
    std::string arg_name = "cbuffer_table";
    if (argbuffer_constant_slot != kConstantBufferBindIndex)
      arg_name += std::to_string(argbuffer_constant_slot);
    cbuf_table_index =
      func_signature.DefineInput(air::ArgumentBindingIndirectBuffer{
        .location_index = argbuffer_constant_slot, // kConstantBufferBindIndex
        .array_size = 1,
        .memory_access = air::MemoryAccess::read,
        .address_space = air::AddressSpace::constant,
        .struct_type = type,
        .struct_type_info = metadata,
        .arg_name = arg_name,
      });
  }

  for (auto &[range_id, cbv] : shader_info->cbufferMap) {
    // TODO: abstract SM 5.0 binding
    auto index = cbv.arg_index;
    resource_map.cb_range_map[range_id] = [=](pvalue) {
      // ignore index in SM 5.0
      return get_item_in_argbuf_binding_table(cbuf_table_index, index);
    };
  }
  for (auto &[range_id, sampler] : shader_info->samplerMap) {
    // TODO: abstract SM 5.0 binding
    resource_map.sampler_range_map[range_id] = {
      [=, index = sampler.arg_index](pvalue) {
        // ignore index in SM 5.0
        return get_item_in_argbuf_binding_table(binding_table_index, index);
      },
      [=, index = sampler.arg_cube_index](pvalue) {
        // ignore index in SM 5.0
        return get_item_in_argbuf_binding_table(binding_table_index, index);
      },
      [=, index = sampler.arg_metadata_index](pvalue) {
        // ignore index in SM 5.0
        return get_item_in_argbuf_binding_table(binding_table_index, index);
      }
    };
  }
  for (auto &[range_id, srv] : shader_info->srvMap) {
    if (srv.resource_type != shader::common::ResourceType::NonApplicable) {
      // TODO: abstract SM 5.0 binding
      auto access =
        srv.sampled ? air::MemoryAccess::sample : air::MemoryAccess::read;
      auto texture_kind_logical = air::to_air_resource_type(srv.resource_type, srv.compared);
      auto scaler_type = air::to_air_scaler_type(srv.scaler_type);
      resource_map.srv_range_map[range_id] = {
        air::MSLTexture{
          .component_type = scaler_type,
          .memory_access = access,
          .resource_kind = air::lowering_texture_1d_to_2d(texture_kind_logical),
          .resource_kind_logical = texture_kind_logical,
        },
        [=, index = srv.arg_index](pvalue) {
          // ignore index in SM 5.0
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        },
        [=, index = srv.arg_metadata_index](pvalue) {
          // ignore index in SM 5.0
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        },
        false
      };
    } else {
      resource_map.srv_buf_range_map[range_id] = {
        srv.strucure_stride,
        [=, index = srv.arg_index](pvalue) {
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        },
        [=, index = srv.arg_metadata_index](pvalue) {
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        },
        false
      };
    }
  }
  for (auto &[range_id, uav] : shader_info->uavMap) {
    auto access = uav.written ? (uav.read ? air::MemoryAccess::read_write
                                          : air::MemoryAccess::write)
                              : air::MemoryAccess::read;
    if (uav.resource_type != shader::common::ResourceType::NonApplicable) {
      auto texture_kind_logical = air::to_air_resource_type(uav.resource_type);
      auto scaler_type = air::to_air_scaler_type(uav.scaler_type);
      resource_map.uav_range_map[range_id] = {
        air::MSLTexture{
          .component_type = scaler_type,
          .memory_access = access,
          .resource_kind = air::lowering_texture_1d_to_2d(texture_kind_logical),
          .resource_kind_logical = texture_kind_logical,
        },
        [=, index = uav.arg_index](pvalue) {
          // ignore index in SM 5.0
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        },
        [=, index = uav.arg_metadata_index](pvalue) {
          // ignore index in SM 5.0
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        },
        uav.global_coherent
      };
    } else {
      resource_map.uav_buf_range_map[range_id] = {
        uav.strucure_stride,
        [=, index = uav.arg_index](pvalue) {
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        },
        [=, index = uav.arg_metadata_index](pvalue) {
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        },
        uav.global_coherent
      };
      if (uav.with_counter) {
        auto argbuf_index_counterptr = uav.arg_counter_index;
        resource_map.uav_counter_range_map[range_id] = [=](pvalue) {
          return get_item_in_argbuf_binding_table(
            binding_table_index, argbuf_index_counterptr
          );
        };
      }
    }
  }
};

void setup_immediate_constant_buffer(
  const ShaderInfo *shader_info, io_binding_map &resource_map,
  air::AirType &types, llvm::Module &module, llvm::IRBuilder<> &builder
) {
  if (!shader_info->immConstantBufferData.size()) {
    return;
  }
  auto &context = module.getContext();
  auto type = llvm::ArrayType::get(
    types._int4, shader_info->immConstantBufferData.size()
  );
  auto const_data = llvm::ConstantArray::get(
    type,
    shader_info->immConstantBufferData |
      [&](auto data) {
        return llvm::ConstantVector::get(
          {llvm::ConstantInt::get(context, llvm::APInt{32, data[0], false}),
           llvm::ConstantInt::get(context, llvm::APInt{32, data[1], false}),
           llvm::ConstantInt::get(context, llvm::APInt{32, data[2], false}),
           llvm::ConstantInt::get(context, llvm::APInt{32, data[3], false})}
        );
      }
  );
  llvm::GlobalVariable *icb = new llvm::GlobalVariable(
    module, type, true, llvm::GlobalValue::InternalLinkage, const_data, "icb",
    nullptr, llvm::GlobalValue::NotThreadLocal, 2
  );
  icb->setAlignment(llvm::Align(4));
  resource_map.icb = icb;
  resource_map.icb_float = builder.CreateBitCast(
    resource_map.icb, llvm::ArrayType::get(
                        types._float4, shader_info->immConstantBufferData.size()
                      )
                        ->getPointerTo(2)
  );
}

void setup_tgsm(
  const ShaderInfo *shader_info, io_binding_map &resource_map,
  air::AirType &types, llvm::Module &module
) {
  for (auto &[id, tgsm] : shader_info->tgsmMap) {
    auto type = llvm::ArrayType::get(types._int, tgsm.size_in_uint);
    llvm::GlobalVariable *tgsm_h = new llvm::GlobalVariable(
      module, type, false, llvm::GlobalValue::InternalLinkage,
      llvm::UndefValue::get(type), "g" + std::to_string(id), nullptr,
      llvm::GlobalValue::NotThreadLocal, 3
    );
    tgsm_h->setAlignment(llvm::Align(4));
    resource_map.tgsm_map[id] = {tgsm.structured ? tgsm.stride : 0, tgsm_h};
  }
}

void setup_temp_register(
  const ShaderInfo *shader_info, io_binding_map &resource_map,
  air::AirType &types, llvm::Module &module, llvm::IRBuilder<> &builder
) {
  resource_map.temp.ptr_int4 = builder.CreateAlloca(
    llvm::ArrayType::get(types._int, 4 * shader_info->tempRegisterCount)
  );
  resource_map.temp.ptr_float4 = builder.CreateBitCast(
    resource_map.temp.ptr_int4,
    llvm::ArrayType::get(types._float, 4 * shader_info->tempRegisterCount)
      ->getPointerTo()
  );
  for (auto &phase : shader_info->phases) {
    resource_map.phases.push_back({});
    auto &phase_temp = resource_map.phases.back();

    phase_temp.temp.ptr_int4 = builder.CreateAlloca(
      llvm::ArrayType::get(types._int, 4 * phase.tempRegisterCount)
    );
    phase_temp.temp.ptr_float4 = builder.CreateBitCast(
      phase_temp.temp.ptr_int4,
      llvm::ArrayType::get(types._float, 4 * phase.tempRegisterCount)
        ->getPointerTo()
    );

    for (auto &[idx, info] : phase.indexableTempRegisterCounts) {
      auto &[numRegisters, mask] = info;
      auto channel_count = std::bit_width(mask);
      auto ptr_int_vec = builder.CreateAlloca(llvm::ArrayType::get(types._int, numRegisters * channel_count));
      auto ptr_float_vec = builder.CreateBitCast(
          ptr_int_vec, llvm::ArrayType::get(types._float, numRegisters * channel_count)->getPointerTo()
      );
      phase_temp.indexable_temp_map[idx] = {
        ptr_int_vec, ptr_float_vec, (uint32_t)channel_count
      };
    }
  }
  for (auto &[idx, info] : shader_info->indexableTempRegisterCounts) {
    auto &[numRegisters, mask] = info;
    auto channel_count = std::bit_width(mask);
    auto ptr_int_vec = builder.CreateAlloca(llvm::ArrayType::get(types._int, numRegisters * channel_count));
    auto ptr_float_vec = builder.CreateBitCast(
        ptr_int_vec, llvm::ArrayType::get(types._float, numRegisters * channel_count)->getPointerTo()
    );
    resource_map.indexable_temp_map[idx] = {
      ptr_int_vec, ptr_float_vec, (uint32_t)channel_count
    };
  }
  if (shader_info->use_cmp_exch) {
    resource_map.cmp_exch_temp = builder.CreateAlloca(types._int);
  }
}

void setup_metal_version(llvm::Module &module, SM50_SHADER_METAL_VERSION metal_verison) {
  using namespace llvm;
  auto &context = module.getContext();
  auto createUnsignedInteger = [&](uint32_t s) {
    return ConstantAsMetadata::get(ConstantInt::get(context, APInt{32, s, false}));
  };
  auto createString = [&](auto s) { return MDString::get(context, s); };

  auto airVersion = module.getOrInsertNamedMetadata("air.version");
  auto airLangVersion = module.getOrInsertNamedMetadata("air.language_version");
  switch (metal_verison) {
  case SM50_SHADER_METAL_320: {
    airVersion->addOperand(
        MDTuple::get(context, {createUnsignedInteger(2), createUnsignedInteger(7), createUnsignedInteger(0)})
    );
    airLangVersion->addOperand(MDTuple::get(
        context, {createString("Metal"), createUnsignedInteger(3), createUnsignedInteger(2), createUnsignedInteger(0)}
    ));
    module.setTargetTriple("air64-apple-macosx15.0.0");
    break;
  }
  default: {
    airVersion->addOperand(
        MDTuple::get(context, {createUnsignedInteger(2), createUnsignedInteger(6), createUnsignedInteger(0)})
    );
    airLangVersion->addOperand(MDTuple::get(
        context, {createString("Metal"), createUnsignedInteger(3), createUnsignedInteger(1), createUnsignedInteger(0)}
    ));
    break;
  }
  }
}

llvm::Error convert_dxbc_pixel_shader(
  SM50ShaderInternal *pShaderInternal, const char *name,
  llvm::LLVMContext &context, llvm::Module &module,
  SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
) {
  using namespace microsoft;

  auto func_signature = pShaderInternal->func_signature; // copy
  auto shader_info = &(pShaderInternal->shader_info);

  uint32_t max_input_register = pShaderInternal->max_input_register;
  uint32_t max_output_register = pShaderInternal->max_output_register;
  uint32_t pso_sample_mask = 0xffffffff;
  bool pso_dual_source_blending = false;
  bool pso_disable_depth_output = false;
  uint32_t pso_unorm_output_reg_mask = 0;
  SM50_SHADER_PSO_PIXEL_SHADER_DATA *pso_data = nullptr;
  if (args_get_data<SM50_SHADER_PSO_PIXEL_SHADER, SM50_SHADER_PSO_PIXEL_SHADER_DATA>(pArgs, &pso_data)) {
    pso_dual_source_blending = pso_data->dual_source_blending;
    pso_disable_depth_output = pso_data->disable_depth_output;
    pso_unorm_output_reg_mask = pso_data->unorm_output_reg_mask;
    pso_sample_mask = pso_data->sample_mask;
  }
  SM50_SHADER_METAL_VERSION metal_version = SM50_SHADER_METAL_310;
  SM50_SHADER_COMMON_DATA *sm50_common = nullptr;
  if (args_get_data<SM50_SHADER_COMMON, SM50_SHADER_COMMON_DATA>(pArgs, &sm50_common)) {
    metal_version = sm50_common->metal_version;
  }

  IREffect prologue([](auto) { return std::monostate(); });
  IRValue epilogue([](struct context ctx) -> pvalue {
    auto retTy = ctx.function->getReturnType();
    if (retTy->isVoidTy()) {
      return nullptr;
    }
    return llvm::ConstantAggregateZero::get(retTy);
  });

  io_binding_map resource_map;
  air::AirType types(context);

  {
    SignatureContext sig_ctx(prologue, epilogue, func_signature, resource_map);
    sig_ctx.dual_source_blending = pso_dual_source_blending;
    sig_ctx.disable_depth_output = pso_disable_depth_output;
    sig_ctx.pull_mode_reg_mask = shader_info->pull_mode_reg_mask;
    sig_ctx.unorm_output_reg_mask = pso_unorm_output_reg_mask;
    for (auto &p : pShaderInternal->signature_handlers) {
      p(sig_ctx);
    }
  }
  if (pso_sample_mask != 0xffffffff) {
    auto assigned_index =
      func_signature.DefineOutput(air::OutputCoverageMask{});
    epilogue >> [=](pvalue value) -> IRValue {
      return make_irvalue([=](struct context ctx) {
        auto &builder = ctx.builder;
        if (ctx.resource.coverage_mask_reg)
          return value;
        return builder.CreateInsertValue(
          value, builder.getInt32(ctx.pso_sample_mask), {assigned_index}
        );
      });
    };
  }

  setup_binding_table(shader_info, resource_map, func_signature, module);

  auto [function, function_metadata] =
    func_signature.CreateFunction(name, context, module, 0, false);

  auto entry_bb = llvm::BasicBlock::Create(context, "entry", function);
  auto epilogue_bb = llvm::BasicBlock::Create(context, "epilogue", function);
  llvm::IRBuilder<> builder(entry_bb);
  llvm::raw_null_ostream nulldbg{};
  llvm::air::AIRBuilder air(builder, nulldbg);

  setup_metal_version(module, metal_version);

  resource_map.input.ptr_int4 =
    builder.CreateAlloca(llvm::ArrayType::get(types._int4, max_input_register));
  resource_map.input.ptr_float4 = builder.CreateBitCast(
    resource_map.input.ptr_int4,
    llvm::ArrayType::get(types._float4, max_input_register)->getPointerTo()
  );
  resource_map.input_element_count = max_input_register;
  resource_map.output.ptr_int4 =
    builder.CreateAlloca(llvm::ArrayType::get(types._int4, max_output_register)
    );
  resource_map.output.ptr_float4 = builder.CreateBitCast(
    resource_map.output.ptr_int4,
    llvm::ArrayType::get(types._float4, max_output_register)->getPointerTo()
  );
  resource_map.output_element_count = max_output_register;

  setup_temp_register(shader_info, resource_map, types, module, builder);
  setup_immediate_constant_buffer(
    shader_info, resource_map, types, module, builder
  );

  struct context ctx {
    .builder = builder, .air = air, .llvm = context, .module = module, .function = function,
    .resource = resource_map, .types = types,
    .pso_sample_mask = pso_sample_mask,
    .shader_type = pShaderInternal->shader_type,
    .metal_version = metal_version,
  };

  if (auto err = prologue.build(ctx).takeError()) {
    return err;
  }
  auto real_entry = convert_basicblocks(pShaderInternal->entry(), ctx, epilogue_bb);
  if (auto err = real_entry.takeError()) {
    return err;
  }
  builder.CreateBr(real_entry.get());

  builder.SetInsertPoint(epilogue_bb);
  auto epilogue_result = epilogue.build(ctx);
  if (auto err = epilogue_result.takeError()) {
    return err;
  }
  auto value = epilogue_result.get();
  if (value == nullptr) {
    builder.CreateRetVoid();
  } else {
    builder.CreateRet(value);
  }

  module.getOrInsertNamedMetadata("air.fragment")
    ->addOperand(function_metadata);

  return llvm::Error::success();
};

llvm::Error convert_dxbc_compute_shader(
  SM50ShaderInternal *pShaderInternal, const char *name,
  llvm::LLVMContext &context, llvm::Module &module,
  SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
) {
  using namespace microsoft;

  auto func_signature = pShaderInternal->func_signature; // copy
  auto shader_info = &(pShaderInternal->shader_info);

  SM50_SHADER_METAL_VERSION metal_version = SM50_SHADER_METAL_310;
  SM50_SHADER_COMMON_DATA *sm50_common = nullptr;
  if (args_get_data<SM50_SHADER_COMMON, SM50_SHADER_COMMON_DATA>(pArgs, &sm50_common)) {
    metal_version = sm50_common->metal_version;
  }

  IREffect prologue([](auto) { return std::monostate(); });
  IRValue epilogue([](struct context ctx) -> pvalue {
    auto retTy = ctx.function->getReturnType();
    if (retTy->isVoidTy()) {
      return nullptr;
    }
    return llvm::ConstantAggregateZero::get(retTy);
  });

  io_binding_map resource_map;
  air::AirType types(context);

  {
    SignatureContext sig_ctx(prologue, epilogue, func_signature, resource_map);
    for (auto &p : pShaderInternal->signature_handlers) {
      p(sig_ctx);
    }
  }

  setup_binding_table(shader_info, resource_map, func_signature, module);
  setup_tgsm(shader_info, resource_map, types, module);

  auto [function, function_metadata] =
    func_signature.CreateFunction(name, context, module, 0, false);

  auto entry_bb = llvm::BasicBlock::Create(context, "entry", function);
  auto epilogue_bb = llvm::BasicBlock::Create(context, "epilogue", function);
  llvm::IRBuilder<> builder(entry_bb);
  llvm::raw_null_ostream nulldbg{};
  llvm::air::AIRBuilder air(builder, nulldbg);

  setup_metal_version(module, metal_version);
  setup_temp_register(shader_info, resource_map, types, module, builder);
  setup_immediate_constant_buffer(
    shader_info, resource_map, types, module, builder
  );

  struct context ctx {
    .builder = builder, .air = air, .llvm = context, .module = module, .function = function,
    .resource = resource_map, .types = types, .pso_sample_mask = 0xffffffff,
    .shader_type = pShaderInternal->shader_type,
    .metal_version = metal_version,
  };

  if (auto err = prologue.build(ctx).takeError()) {
    return err;
  }
  auto real_entry = convert_basicblocks(pShaderInternal->entry(), ctx, epilogue_bb);
  if (auto err = real_entry.takeError()) {
    return err;
  }
  builder.CreateBr(real_entry.get());

  builder.SetInsertPoint(epilogue_bb);

  if (auto err = epilogue.build(ctx).takeError()) {
    return err;
  }
  builder.CreateRetVoid();

  module.getOrInsertNamedMetadata("air.kernel")->addOperand(function_metadata);

  return llvm::Error::success();
};

llvm::Error convert_dxbc_vertex_shader(
  SM50ShaderInternal *pShaderInternal, const char *name,
  llvm::LLVMContext &context, llvm::Module &module,
  SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
) {
  using namespace microsoft;

  auto func_signature = pShaderInternal->func_signature; // copy
  auto shader_info = &(pShaderInternal->shader_info);

  uint32_t max_input_register = pShaderInternal->max_input_register;
  uint32_t max_output_register = pShaderInternal->max_output_register;
  SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA *vertex_so = nullptr;
  args_get_data<SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT, SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA>(
      pArgs, &vertex_so
  );
  SM50_SHADER_IA_INPUT_LAYOUT_DATA *ia_layout = nullptr;
  args_get_data<SM50_SHADER_IA_INPUT_LAYOUT, SM50_SHADER_IA_INPUT_LAYOUT_DATA>(pArgs, &ia_layout);
  SM50_SHADER_GS_PASS_THROUGH_DATA *gs_passthrough = nullptr;
  bool rasterization_disabled =
      (args_get_data<SM50_SHADER_GS_PASS_THROUGH, SM50_SHADER_GS_PASS_THROUGH_DATA>(pArgs, &gs_passthrough) &&
       gs_passthrough->RasterizationDisabled) ||
      (vertex_so != nullptr);
  SM50_SHADER_METAL_VERSION metal_version = SM50_SHADER_METAL_310;
  SM50_SHADER_COMMON_DATA *sm50_common = nullptr;
  if (args_get_data<SM50_SHADER_COMMON, SM50_SHADER_COMMON_DATA>(pArgs, &sm50_common)) {
    metal_version = sm50_common->metal_version;
  }

  IREffect prologue([](auto) { return std::monostate(); });
  IRValue epilogue([](struct context ctx) -> pvalue {
    auto retTy = ctx.function->getReturnType();
    if (retTy->isVoidTy()) {
      return nullptr;
    }
    return llvm::ConstantAggregateZero::get(retTy);
  });

  io_binding_map resource_map;
  air::AirType types(context);

  {
    SignatureContext sig_ctx(prologue, epilogue, func_signature, resource_map);
    sig_ctx.ia_layout = ia_layout;
    sig_ctx.skip_vertex_output = rasterization_disabled;
    for (auto &p : pShaderInternal->signature_handlers) {
      p(sig_ctx);
    }

    for (auto& out : pShaderInternal->output_signature) {
      if(out.isSystemValue()) continue;
      func_signature.DefineOutput(air::OutputVertex{
        .user = out.fullSemanticString(),
        .type = to_msl_type(out.componentType()),
      });
    }
  }
  if (vertex_so) {
    auto bv = func_signature.DefineInput(air::InputBaseVertex{});
    auto vid = func_signature.DefineInput(air::InputVertexID{});
    auto slot_0 = func_signature.DefineInput(air::ArgumentBindingBuffer{
      .buffer_size = {},
      .location_index = 20,
      .array_size = 0,
      .memory_access = air::MemoryAccess::write,
      .address_space = air::AddressSpace::device,
      .type = air::MSLUint{},
      .arg_name = "so_slot0",
      .raster_order_group = {}
    });
    epilogue << make_irvalue([=](struct context ctx) {
      auto &builder = ctx.builder;
      auto base_vertex = ctx.function->getArg(bv);
      auto vertex_id = ctx.function->getArg(vid);
      auto slot0 = ctx.function->getArg(slot_0);
      auto adjusted_vertex_id = builder.CreateSub(vertex_id, base_vertex);
      auto output_regs = builder.CreateBitOrPointerCast(
        ctx.resource.output.ptr_int4, llvm::PointerType::get(ctx.types._int, 0)
      );
      for (unsigned i = 0; i < vertex_so->num_elements; i++) {
        auto &element = vertex_so->elements[i];
        if (element.reg_id == 0xffffffff)
          continue;
        auto ptr = ctx.builder.CreateConstGEP1_32(
          ctx.types._int, output_regs,
          (unsigned)(element.reg_id * 4 + element.component)
        );
        auto target_offset = ctx.builder.CreateAdd(
          ctx.builder.CreateMul(
            adjusted_vertex_id,
            ctx.builder.getInt32(vertex_so->strides[element.output_slot])
          ),
          ctx.builder.getInt32(element.offset)
        );
        auto target_ptr = ctx.builder.CreateGEP(
          ctx.types._int, slot0, {ctx.builder.CreateLShr(target_offset, 2)}
        );
        ctx.builder.CreateStore(
          ctx.builder.CreateLoad(ctx.types._int, ptr), target_ptr, true
        );
      }
      return nullptr;
    });
  }

  uint32_t vertex_idx = func_signature.DefineInput(air::InputVertexID{});
  uint32_t base_vertex_idx = func_signature.DefineInput(air::InputBaseVertex{});
  uint32_t instance_idx = func_signature.DefineInput(air::InputInstanceID{});
  uint32_t base_instance_idx =
    func_signature.DefineInput(air::InputBaseInstance{});

  uint32_t clip_distance_out_idx = ~0u;
  if (pShaderInternal->clip_distance_scalars.size()) {
    clip_distance_out_idx = func_signature.DefineOutput(
      air::OutputClipDistance{pShaderInternal->clip_distance_scalars.size()}
    );
  };

  setup_binding_table(shader_info, resource_map, func_signature, module);

  uint32_t rta_idx_out = ~0u;
  if (gs_passthrough && gs_passthrough->Data.RenderTargetArrayIndexReg != 255) {
    rta_idx_out =
      func_signature.DefineOutput(air::OutputRenderTargetArrayIndex{});
  }
  uint32_t va_idx_out = ~0u;
  if (gs_passthrough && gs_passthrough->Data.ViewportArrayIndexReg != 255) {
    va_idx_out = func_signature.DefineOutput(air::OutputViewportArrayIndex{});
  }

  auto [function, function_metadata] = func_signature.CreateFunction(
    name, context, module, 0,
    rasterization_disabled
  );

  auto entry_bb = llvm::BasicBlock::Create(context, "entry", function);
  auto epilogue_bb = llvm::BasicBlock::Create(context, "epilogue", function);
  llvm::IRBuilder<> builder(entry_bb);
  llvm::raw_null_ostream nulldbg{};
  llvm::air::AIRBuilder air(builder, nulldbg);

  setup_metal_version(module, metal_version);

  resource_map.vertex_id_with_base = function->getArg(vertex_idx);
  resource_map.base_vertex_id = function->getArg(base_vertex_idx);
  resource_map.instance_id_with_base = function->getArg(instance_idx);
  resource_map.base_instance_id = function->getArg(base_instance_idx);
  resource_map.vertex_id = builder.CreateSub(
    resource_map.vertex_id_with_base, resource_map.base_vertex_id
  );
  resource_map.instance_id = builder.CreateSub(
    resource_map.instance_id_with_base, resource_map.base_instance_id
  );

  resource_map.input.ptr_int4 =
    builder.CreateAlloca(llvm::ArrayType::get(types._int4, max_input_register));
  resource_map.input.ptr_float4 = builder.CreateBitCast(
    resource_map.input.ptr_int4,
    llvm::ArrayType::get(types._float4, max_input_register)->getPointerTo()
  );
  resource_map.input_element_count = max_input_register;
  resource_map.output.ptr_int4 =
    builder.CreateAlloca(llvm::ArrayType::get(types._int4, max_output_register)
    );
  resource_map.output.ptr_float4 = builder.CreateBitCast(
    resource_map.output.ptr_int4,
    llvm::ArrayType::get(types._float4, max_output_register)->getPointerTo()
  );
  resource_map.output_element_count = max_output_register;

  setup_temp_register(shader_info, resource_map, types, module, builder);
  setup_immediate_constant_buffer(
    shader_info, resource_map, types, module, builder
  );

  struct context ctx {
    .builder = builder, .air = air, .llvm = context, .module = module, .function = function,
    .resource = resource_map, .types = types, .pso_sample_mask = 0xffffffff,
    .shader_type = pShaderInternal->shader_type,
    .metal_version = metal_version,
  };

  if (auto err = prologue.build(ctx).takeError()) {
    return err;
  }
  auto real_entry = convert_basicblocks(pShaderInternal->entry(), ctx, epilogue_bb);
  if (auto err = real_entry.takeError()) {
    return err;
  }
  builder.CreateBr(real_entry.get());

  builder.SetInsertPoint(epilogue_bb);
  auto epilogue_result = epilogue.build(ctx);
  if (auto err = epilogue_result.takeError()) {
    return err;
  }
  auto value = epilogue_result.get();

  if (pShaderInternal->clip_distance_scalars.size()) {
    auto clip_distance_ty = llvm::ArrayType::get(
      types._float, pShaderInternal->clip_distance_scalars.size()
    );
    pvalue clip_distance_array = llvm::ConstantAggregateZero::get(clip_distance_ty);
    unsigned clip_distance_idx = 0;
    for (auto &scalar : pShaderInternal->clip_distance_scalars) {
      auto src_ptr = builder.CreateGEP(
        resource_map.output.ptr_float4->getType()
          ->getNonOpaquePointerElementType(),
        resource_map.output.ptr_float4,
        {builder.getInt32(0), builder.getInt32(scalar.reg),
         builder.getInt32(scalar.component)}
      );
      clip_distance_array = builder.CreateInsertValue(
        clip_distance_array, builder.CreateLoad(types._float, src_ptr), {clip_distance_idx++}
      );
    }
    value = builder.CreateInsertValue(
      value, clip_distance_array, {clip_distance_out_idx}
    );
  };

  if (value == nullptr) {
    builder.CreateRetVoid();
  } else {
    if (rta_idx_out != ~0u) {
      auto src_ptr = builder.CreateGEP(
        resource_map.output.ptr_int4->getType()->getNonOpaquePointerElementType(
        ),
        resource_map.output.ptr_int4,
        {builder.getInt32(0),
         builder.getInt32(gs_passthrough->Data.RenderTargetArrayIndexReg),
         builder.getInt32(gs_passthrough->Data.RenderTargetArrayIndexComponent)}
      );
      value = builder.CreateInsertValue(
        value, builder.CreateLoad(types._int, src_ptr), {rta_idx_out}
      );
    }
    if (va_idx_out != ~0u) {
      auto src_ptr = builder.CreateGEP(
        resource_map.output.ptr_int4->getType()->getNonOpaquePointerElementType(
        ),
        resource_map.output.ptr_int4,
        {builder.getInt32(0),
         builder.getInt32(gs_passthrough->Data.ViewportArrayIndexReg),
         builder.getInt32(gs_passthrough->Data.ViewportArrayIndexComponent)}
      );
      value = builder.CreateInsertValue(
        value, builder.CreateLoad(types._int, src_ptr), {va_idx_out}
      );
    }
    builder.CreateRet(value);
  }

  module.getOrInsertNamedMetadata("air.vertex")->addOperand(function_metadata);
  return llvm::Error::success();
};

llvm::Error convertDXBC(
  sm50_shader_t pShader, const char *name, llvm::LLVMContext &context,
  llvm::Module &module, SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
) {
  using namespace microsoft;

  auto pShaderInternal = (SM50ShaderInternal *)pShader;

  switch (pShaderInternal->shader_type) {
  case microsoft::D3D10_SB_PIXEL_SHADER:
    return convert_dxbc_pixel_shader(
      pShaderInternal, name, context, module, pArgs
    );
  case microsoft::D3D10_SB_VERTEX_SHADER:
    return convert_dxbc_vertex_shader(
      pShaderInternal, name, context, module, pArgs
    );
  case microsoft::D3D11_SB_COMPUTE_SHADER:
    return convert_dxbc_compute_shader(
      pShaderInternal, name, context, module, pArgs
    );
  case microsoft::D3D11_SB_HULL_SHADER:
  case microsoft::D3D11_SB_DOMAIN_SHADER:
    return llvm::make_error<UnsupportedFeature>(
      "Hull and domain shader cannot be independently converted."
    );
  case microsoft::D3D10_SB_GEOMETRY_SHADER:
    return llvm::make_error<UnsupportedFeature>(
      "Geometry shader cannot be independently converted."
    );
  case microsoft::D3D12_SB_MESH_SHADER:
  case microsoft::D3D12_SB_AMPLIFICATION_SHADER:
  case microsoft::D3D11_SB_RESERVED0:
    break;
  }
  return llvm::make_error<UnsupportedFeature>("Not supported shader type");
};
} // namespace dxmt::dxbc

bool CheckGSBBIsPassThrough(dxmt::dxbc::BasicBlock *bb) {
  using namespace dxmt::dxbc;
  if (bb->instructions.any(patterns{
        [](InstNop &) { return false; }, [](InstMov &mov) { return false; },
        [](InstEmit &) { return false; }, [](InstCut &) { return false; },
        [](auto &) { return true; }
      }))
    return false;

  return std::visit(
    patterns{
      [](dxmt::dxbc::BasicBlockReturn) { return true; },
      [](dxmt::dxbc::BasicBlockUnconditionalBranch uncond) {
        return CheckGSBBIsPassThrough(uncond.target);
      },
      [](auto) { return false; }
    },
    bb->target
  );
}

bool CheckGSSignatureIsPassThrough(
  microsoft::CSignatureParser &input, microsoft::CSignatureParser5 &output,
  MTL_GEOMETRY_SHADER_PASS_THROUGH &data
) {
  using namespace microsoft;

  data.RenderTargetArrayIndexComponent = 255;
  data.RenderTargetArrayIndexReg = 255;
  data.ViewportArrayIndexComponent = 255;
  data.ViewportArrayIndexReg = 255;

  if (output.NumStreams() > 1) {
    return false;
  }
  if (output.RasterizedStream()) {
    return false;
  }

  const D3D11_SIGNATURE_PARAMETER *input_parameters, *output_parameters;
  input.GetParameters(&input_parameters);
  output.Signature(0)->GetParameters(&output_parameters);
  size_t num_input, num_output;
  num_input = input.GetNumParameters();
  num_output = output.Signature(0)->GetNumParameters();
  if (num_output > num_input) {
    // at least it is a non-trival shader that generates new data
    return false;
  }
  bool has_match_record;
  for (unsigned i = 0; i < num_output; i++) {
    has_match_record = false;
    const D3D11_SIGNATURE_PARAMETER &out = output_parameters[i];
    for (unsigned j = 0; j < num_input; j++) {
      const D3D11_SIGNATURE_PARAMETER &in = input_parameters[j];
      if (out.Register == in.Register && (out.Mask & in.Mask) == out.Mask) {
        if (out.SystemValue == D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX) {
          data.RenderTargetArrayIndexReg = out.Register;
          data.RenderTargetArrayIndexComponent = __builtin_ctz(out.Mask);
        } else if (out.SystemValue == D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX) {
          data.ViewportArrayIndexReg = out.Register;
          data.ViewportArrayIndexComponent = __builtin_ctz(out.Mask);
        } else {
          if (out.SemanticIndex != in.SemanticIndex) {
            return false;
          }
          if (strcasecmp(out.SemanticName, in.SemanticName)) {
            return false;
          }
        }
        has_match_record = true;
        break;
      }
    }
    if (!has_match_record) {
      return false;
    }
  }

  return true;
};

AIRCONV_API int SM50Initialize(
  const void *pBytecode, size_t BytecodeSize, sm50_shader_t *ppShader,
  MTL_SHADER_REFLECTION *pRefl, sm50_error_t *ppError
) {
  using namespace microsoft;
  using namespace dxmt::dxbc;
  using namespace dxmt::air;
  using namespace dxmt::shader::common;
  if (ppError) {
    *ppError = nullptr;
  }
  auto errorObj = new SM50ErrorInternal();
  llvm::raw_svector_ostream errorOut(errorObj->buf);

  if (ppShader == nullptr) {
    errorOut << "ppShader can not be null\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  CDXBCParser DXBCParser;
  if (DXBCParser.ReadDXBC(pBytecode, BytecodeSize) != S_OK) {
    errorOut << "Invalid DXBC bytecode\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  UINT codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShaderEx);
  if (codeBlobIdx == DXBC_BLOB_NOT_FOUND) {
    codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShader);
  }
  if (codeBlobIdx == DXBC_BLOB_NOT_FOUND) {
    errorOut << "Invalid DXBC bytecode: shader blob not found\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }
  const void *codeBlob = DXBCParser.GetBlob(codeBlobIdx);

  CShaderToken *ShaderCode = (CShaderToken *)(BYTE *)codeBlob;
  // 1. Collect information about the shader.
  D3D10ShaderBinary::CShaderCodeParser CodeParser(ShaderCode);
  CSignatureParser inputParser;
  if (DXBCGetInputSignature(pBytecode, &inputParser) != S_OK) {
    errorOut << "Invalid DXBC bytecode: input signature not found\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }
  CSignatureParser5 outputParser;
  if (DXBCGetOutputSignature(pBytecode, &outputParser) != S_OK) {
    errorOut << "Invalid DXBC bytecode: output signature not found\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  auto sm50_shader = new SM50ShaderInternal();
  sm50_shader->shader_type = CodeParser.ShaderType();
  auto shader_info = &(sm50_shader->shader_info);

  {
    const D3D11_SIGNATURE_PARAMETER *parameters;
    outputParser.RastSignature()->GetParameters(&parameters);
    for (unsigned i = 0; i < outputParser.RastSignature()->GetNumParameters(); i++) {
      sm50_shader->output_signature.push_back(Signature(parameters[i]));
    }
  }

  sm50_shader->bbs = read_control_flow(CodeParser, sm50_shader, inputParser, outputParser);

  auto &binding_table = shader_info->binding_table;
  auto &binding_table_cbuffer = shader_info->binding_table_cbuffer;

  uint16_t binding_cbuffer_mask = 0;
  uint16_t binding_sampler_mask = 0;
  uint64_t binding_uav_mask = 0;
  uint64_t binding_srv_hi_mask = 0;
  uint64_t binding_srv_lo_mask = 0;

  for (auto &[range_id, cbv] : shader_info->cbufferMap) {
    // TODO: abstract SM 5.0 binding
    cbv.arg_index = binding_table_cbuffer.DefineBuffer(
      "cb" + std::to_string(range_id), AddressSpace::constant,
      MemoryAccess::read, msl_uint4,
      GetArgumentIndex(SM50BindingType::ConstantBuffer, range_id)
    );
    sm50_shader->args_reflection_cbuffer.push_back({
      .Type = SM50BindingType::ConstantBuffer,
      .SM50BindingSlot = range_id,
      .Flags =
        MTL_SM50_SHADER_ARGUMENT_BUFFER | MTL_SM50_SHADER_ARGUMENT_READ_ACCESS,
      .StructurePtrOffset = cbv.arg_index,
    });
    binding_cbuffer_mask |= (1 << range_id);
  }
  for (auto &[range_id, sampler] : shader_info->samplerMap) {
    // TODO: abstract SM 5.0 binding
    auto attr_index = GetArgumentIndex(SM50BindingType::Sampler, range_id);
    sampler.arg_index =
      binding_table.DefineSampler("s" + std::to_string(range_id), attr_index);
    sampler.arg_cube_index =
      binding_table.DefineSampler("cube_s" + std::to_string(range_id), attr_index + 1);
    sampler.arg_metadata_index = binding_table.DefineInteger64(
      "meta_s" + std::to_string(range_id), attr_index + 2
    );
    sm50_shader->args_reflection.push_back({
      .Type = SM50BindingType::Sampler,
      .SM50BindingSlot = range_id,
      .Flags = (MTL_SM50_SHADER_ARGUMENT_FLAG)0,
      .StructurePtrOffset = sampler.arg_index,
    });
    binding_sampler_mask |= (1 << range_id);
  }
  for (auto &[range_id, srv] : shader_info->srvMap) {
    auto attr_index = GetArgumentIndex(SM50BindingType::SRV, range_id);
    if (srv.resource_type != ResourceType::NonApplicable) {
      // TODO: abstract SM 5.0 binding
      auto access = srv.sampled ? MemoryAccess::sample : MemoryAccess::read;
      auto texture_kind = to_air_resource_type(srv.resource_type, srv.compared);
      auto scaler_type = to_air_scaler_type(srv.scaler_type);
      srv.arg_index = binding_table.DefineTexture(
        "t" + std::to_string(range_id), texture_kind, access, scaler_type,
        attr_index
      );
    } else {
      srv.arg_index = binding_table.DefineBuffer(
        "t" + std::to_string(range_id),
        AddressSpace::device, // it needs to be `device` since `constant` has
                              // size and alignment restriction
        MemoryAccess::read, msl_uint, attr_index
      );
    }
    srv.arg_metadata_index = binding_table.DefineInteger64(
      "meta_t" + std::to_string(range_id), attr_index + 1
    );
    MTL_SM50_SHADER_ARGUMENT_FLAG flags = (MTL_SM50_SHADER_ARGUMENT_FLAG)0;
    if (srv.resource_type == ResourceType::NonApplicable) {
      flags |= MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH |
               MTL_SM50_SHADER_ARGUMENT_BUFFER;
    } else if (srv.resource_type == ResourceType::TextureBuffer) {
      flags |= MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET |
               MTL_SM50_SHADER_ARGUMENT_TEXTURE;
    } else {
      flags |= MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP |
               MTL_SM50_SHADER_ARGUMENT_TEXTURE;
      switch (srv.resource_type) {
      case ResourceType::Texture1DArray:
      case ResourceType::Texture2DArray:
      case ResourceType::Texture2DMultisampledArray:
      case ResourceType::TextureCubeArray:
        flags |= MTL_SM50_SHADER_ARGUMENT_TEXTURE_ARRAY;
        break;
      default:
        break;
      }
    }
    if (srv.read || srv.sampled || srv.compared) {
      flags |= MTL_SM50_SHADER_ARGUMENT_READ_ACCESS;
    }
    sm50_shader->args_reflection.push_back({
      .Type = SM50BindingType::SRV,
      .SM50BindingSlot = range_id,
      .Flags = flags,
      .StructurePtrOffset = srv.arg_index,
    });
    if (range_id & 64) {
      binding_srv_hi_mask |= (1ULL << (range_id - 64));
    } else {
      binding_srv_lo_mask |= (1ULL << range_id);
    }
  }
  for (auto &[range_id, uav] : shader_info->uavMap) {
    auto access =
      uav.written ? (uav.read ? MemoryAccess::read_write : MemoryAccess::write)
                  : MemoryAccess::read;
    auto attr_index = GetArgumentIndex(SM50BindingType::UAV, range_id);
    if (uav.resource_type != ResourceType::NonApplicable) {
      auto texture_kind = to_air_resource_type(uav.resource_type);
      auto scaler_type = to_air_scaler_type(uav.scaler_type);
      uav.arg_index = binding_table.DefineTexture(
        "u" + std::to_string(range_id), texture_kind, access, scaler_type,
        attr_index, uav.rasterizer_order ? std::optional(1) : std::nullopt
      );
    } else {
      uav.arg_index = binding_table.DefineBuffer(
        "u" + std::to_string(range_id), AddressSpace::device, access, msl_uint,
        attr_index, uav.rasterizer_order ? std::optional(1) : std::nullopt
      );
    }
    uav.arg_metadata_index = binding_table.DefineInteger64(
      "meta_u" + std::to_string(range_id), attr_index + 1
    );
    if (uav.with_counter) {
      uav.arg_counter_index = binding_table.DefineBuffer(
        "counter" + std::to_string(range_id), AddressSpace::device,
        MemoryAccess::read_write, msl_uint, attr_index + 2,
        uav.rasterizer_order ? std::optional(1) : std::nullopt
      );
    }
    MTL_SM50_SHADER_ARGUMENT_FLAG flags = (MTL_SM50_SHADER_ARGUMENT_FLAG)0;
    if (uav.resource_type == ResourceType::NonApplicable) {
      flags |= MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH |
               MTL_SM50_SHADER_ARGUMENT_BUFFER;
    } else if (uav.resource_type == ResourceType::TextureBuffer) {
      flags |= MTL_SM50_SHADER_ARGUMENT_TBUFFER_OFFSET |
               MTL_SM50_SHADER_ARGUMENT_TEXTURE;
    } else {
      flags |= MTL_SM50_SHADER_ARGUMENT_TEXTURE_MINLOD_CLAMP |
               MTL_SM50_SHADER_ARGUMENT_TEXTURE;
      switch (uav.resource_type) {
      case ResourceType::Texture1DArray:
      case ResourceType::Texture2DArray:
      case ResourceType::Texture2DMultisampledArray:
      case ResourceType::TextureCubeArray:
        flags |= MTL_SM50_SHADER_ARGUMENT_TEXTURE_ARRAY;
        break;
      default:
        break;
      }
    }
    if (uav.read) {
      flags |= MTL_SM50_SHADER_ARGUMENT_READ_ACCESS;
    }
    if (uav.written) {
      flags |= MTL_SM50_SHADER_ARGUMENT_WRITE_ACCESS;
    }
    if (uav.with_counter) {
      flags |= MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER;
    }
    sm50_shader->args_reflection.push_back({
      .Type = SM50BindingType::UAV,
      .SM50BindingSlot = range_id,
      .Flags = flags,
      .StructurePtrOffset = uav.arg_index,
    });
    binding_uav_mask |= (1ULL << range_id);
  }

  if (sm50_shader->shader_type == microsoft::D3D11_SB_HULL_SHADER &&
      !sm50_shader->shader_info.no_control_point_phase_passthrough) {
    assert(sm50_shader->max_output_register == 0);
    uint32_t num_input_params = inputParser.GetNumParameters();
    D3D11_SIGNATURE_PARAMETER const * params;
    inputParser.GetParameters(&params);
    for (unsigned i = 0; i < num_input_params; i++) {
      sm50_shader->max_output_register = std::max(
        sm50_shader->max_output_register,
        params[i].Register + 1
      );
    }
  }

  if (pRefl) {
    pRefl->ConstanttBufferTableBindIndex =
      sm50_shader->args_reflection_cbuffer.size() > 0 ? 29 : ~0u;
    pRefl->ArgumentBufferBindIndex =
      sm50_shader->args_reflection.size() > 0 ? 30 : ~0u;
    pRefl->ConstantBufferSlotMask = binding_cbuffer_mask;
    pRefl->SamplerSlotMask = binding_sampler_mask;
    pRefl->SRVSlotMaskHi = binding_srv_hi_mask;
    pRefl->SRVSlotMaskLo = binding_srv_lo_mask;
    pRefl->UAVSlotMask = binding_uav_mask;
    pRefl->NumConstantBuffers = sm50_shader->args_reflection_cbuffer.size();
    pRefl->NumArguments = sm50_shader->args_reflection.size();
    if (sm50_shader->shader_type == microsoft::D3D11_SB_COMPUTE_SHADER) {
      pRefl->ThreadgroupSize[0] = sm50_shader->threadgroup_size[0];
      pRefl->ThreadgroupSize[1] = sm50_shader->threadgroup_size[1];
      pRefl->ThreadgroupSize[2] = sm50_shader->threadgroup_size[2];
    }
    if (sm50_shader->shader_type == microsoft::D3D11_SB_HULL_SHADER) {
      auto threads_per_patch = next_pow2(sm50_shader->hull_maximum_threads_per_patch);
      if (threads_per_patch > 32) {
        errorOut << "Threadgroup size of tessellation pipeline is too large.";
        *ppError = (sm50_error_t)errorObj;
        return 1;
      }
      auto patch_per_group = 32 / threads_per_patch;
      float max_tesselation_factor = sm50_shader->max_tesselation_factor;

      while (estimate_payload_size(sm50_shader, max_tesselation_factor, patch_per_group) > 16384) {
        if (patch_per_group == 1) {
          if (max_tesselation_factor > 1.0f) {
            max_tesselation_factor = std::max(max_tesselation_factor - 2.0f, 1.0f);
          } else {
            errorOut << "Payload size of tessellation pipeline is too large.";
            *ppError = (sm50_error_t)errorObj;
            return 1;
          }
        } else {
          patch_per_group = patch_per_group >> 1;
        }
      }
      sm50_shader->hull_maximum_threads_per_patch = 32 / patch_per_group;
      sm50_shader->max_tesselation_factor = max_tesselation_factor;

      pRefl->ThreadsPerPatch = sm50_shader->hull_maximum_threads_per_patch;
      pRefl->Tessellator = {
        .Partition = sm50_shader->tessellation_partition,
        .MaxFactor = sm50_shader->max_tesselation_factor,
        .OutputPrimitive = (MTL_TESSELLATOR_OUTPUT_PRIMITIVE
        )sm50_shader->tessellator_output_primitive,
      };
    }
    if (sm50_shader->shader_type == microsoft::D3D11_SB_DOMAIN_SHADER) {
      uint32_t max_potential_tess_factor = 1;

      for (int tess_factor = 1; tess_factor <= 64; tess_factor++) {
        auto x = estimate_mesh_size(sm50_shader, tess_factor);
        if (x > 32768)
          break;
        max_potential_tess_factor = tess_factor;
      }

      pRefl->PostTessellator = {.MaxPotentialTessFactor = max_potential_tess_factor};
    }
    if (sm50_shader->shader_type == microsoft::D3D10_SB_GEOMETRY_SHADER) {
      if (binding_cbuffer_mask || binding_sampler_mask || binding_uav_mask ||
          binding_srv_hi_mask || binding_srv_lo_mask ||
          !CheckGSBBIsPassThrough(sm50_shader->entry())) {
        pRefl->GeometryShader.GSPassThrough = ~0u;
      } else {
        CheckGSSignatureIsPassThrough(
          inputParser, outputParser, pRefl->GeometryShader.Data
        );
      }
      pRefl->GeometryShader.Primitive = sm50_shader->gs_input_primitive;
    }
    if (sm50_shader->shader_type == microsoft::D3D10_SB_PIXEL_SHADER) {
      pRefl->PSValidRenderTargets = sm50_shader->pso_valid_output_reg_mask;
    }
    pRefl->NumOutputElement = sm50_shader->max_output_register;
    pRefl->ArgumentTableQwords = binding_table.Size();
  }

  *ppShader = sm50_shader;
  return 0;
};

AIRCONV_API void SM50GetArgumentsInfo(
  sm50_shader_t pShader, struct MTL_SM50_SHADER_ARGUMENT *pConstantBuffers,
  struct MTL_SM50_SHADER_ARGUMENT *pArguments
) {
  auto sm50_shader = (dxmt::dxbc::SM50ShaderInternal *)pShader;
  if (pConstantBuffers && sm50_shader->args_reflection_cbuffer.size())
    memcpy(
      pConstantBuffers, sm50_shader->args_reflection_cbuffer.data(),
      sm50_shader->args_reflection_cbuffer.size() *
        sizeof(struct MTL_SM50_SHADER_ARGUMENT)
    );
  if (pArguments && sm50_shader->args_reflection.size())
    memcpy(
      pArguments, sm50_shader->args_reflection.data(),
      sm50_shader->args_reflection.size() *
        sizeof(struct MTL_SM50_SHADER_ARGUMENT)
    );
}

AIRCONV_API void SM50Destroy(sm50_shader_t pShader) {
  delete (dxmt::dxbc::SM50ShaderInternal *)pShader;
}

AIRCONV_API int SM50Compile(
  sm50_shader_t pShader, SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs,
  const char *FunctionName, sm50_bitcode_t *ppBitcode, sm50_error_t *ppError
) {
  using namespace llvm;
  using namespace dxmt;

  if (ppError) {
    *ppError = nullptr;
  }
  auto errorObj = new SM50ErrorInternal();
  llvm::raw_svector_ostream errorOut(errorObj->buf);
  if (ppBitcode == nullptr) {
    errorOut << "ppBitcode can not be null\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  // pArgs is ignored for now
  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  auto &shader_info = ((dxmt::dxbc::SM50ShaderInternal *)pShader)->shader_info;

  auto pModule = std::make_unique<Module>("shader.air", context);
  initializeModule(*pModule);

  if (auto err = dxmt::dxbc::convertDXBC(
        pShader, FunctionName, context, *pModule, pArgs
      )) {
    llvm::handleAllErrors(std::move(err), [&](const UnsupportedFeature &u) {
      errorOut << u.msg;
    });
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  if (shader_info.use_msad)
    linkMSAD(*pModule);
  if (shader_info.use_samplepos)
    linkSamplePos(*pModule);

  runOptimizationPasses(*pModule);

  // Serialize AIR
  auto compiled = new SM50CompiledBitcodeInternal();

  raw_svector_ostream OS(compiled->vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  pModule.reset();

  *ppBitcode = (sm50_bitcode_t)compiled;
  return 0;
}

AIRCONV_API int SM50CompileTessellationPipelineHull(
  sm50_shader_t pVertexShader, sm50_shader_t pHullShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pHullShaderArgs,
  const char *FunctionName, sm50_bitcode_t *ppBitcode, sm50_error_t *ppError
) {
  using namespace llvm;
  using namespace dxmt;

  if (ppError) {
    *ppError = nullptr;
  }
  auto errorObj = new SM50ErrorInternal();
  llvm::raw_svector_ostream errorOut(errorObj->buf);
  if (ppBitcode == nullptr) {
    errorOut << "ppBitcode can not be null\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  // pArgs is ignored for now
  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  auto &shader_info =
    ((dxmt::dxbc::SM50ShaderInternal *)pHullShader)->shader_info;

  auto pModule = std::make_unique<Module>("shader.air", context);
  initializeModule(*pModule);

  if (auto err = dxmt::dxbc::convert_dxbc_vertex_hull_shader(
        (dxbc::SM50ShaderInternal *)pVertexShader, (dxbc::SM50ShaderInternal *)pHullShader, 
        FunctionName, context, *pModule, pHullShaderArgs
      )) {
    llvm::handleAllErrors(std::move(err), [&](const UnsupportedFeature &u) {
      errorOut << u.msg;
    });
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  if (shader_info.use_msad)
    linkMSAD(*pModule);
  if (shader_info.use_samplepos)
    linkSamplePos(*pModule);
  linkTessellation(*pModule);

  runOptimizationPasses(*pModule);

  // Serialize AIR
  auto compiled = new SM50CompiledBitcodeInternal();

  raw_svector_ostream OS(compiled->vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  pModule.reset();

  *ppBitcode = (sm50_bitcode_t)compiled;
  return 0;
}

AIRCONV_API int SM50CompileTessellationPipelineDomain(
  sm50_shader_t pHullShader, sm50_shader_t pDomainShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pDomainShaderArgs,
  const char *FunctionName, sm50_bitcode_t *ppBitcode, sm50_error_t *ppError
) {
  using namespace llvm;
  using namespace dxmt;

  if (ppError) {
    *ppError = nullptr;
  }
  auto errorObj = new SM50ErrorInternal();
  llvm::raw_svector_ostream errorOut(errorObj->buf);
  if (ppBitcode == nullptr) {
    errorOut << "ppBitcode can not be null\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  // pArgs is ignored for now
  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  auto &shader_info =
    ((dxmt::dxbc::SM50ShaderInternal *)pDomainShader)->shader_info;

  auto pModule = std::make_unique<Module>("shader.air", context);
  initializeModule(*pModule);

  if (auto err = dxmt::dxbc::convert_dxbc_tesselator_domain_shader(
        (dxbc::SM50ShaderInternal *)pDomainShader, FunctionName,
        (dxbc::SM50ShaderInternal *)pHullShader, context, *pModule,
        pDomainShaderArgs
      )) {
    llvm::handleAllErrors(std::move(err), [&](const UnsupportedFeature &u) {
      errorOut << u.msg;
    });
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  if (shader_info.use_msad)
    linkMSAD(*pModule);
  if (shader_info.use_samplepos)
    linkSamplePos(*pModule);
  linkTessellation(*pModule);
  
  runOptimizationPasses(*pModule);

  // Serialize AIR
  auto compiled = new SM50CompiledBitcodeInternal();

  raw_svector_ostream OS(compiled->vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  pModule.reset();

  *ppBitcode = (sm50_bitcode_t)compiled;
  return 0;
}

AIRCONV_API int SM50CompileGeometryPipelineVertex(
  sm50_shader_t pVertexShader, sm50_shader_t pGeometryShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pVertexShaderArgs,
  const char *FunctionName, sm50_bitcode_t *ppBitcode, sm50_error_t *ppError
) {
  using namespace llvm;
  using namespace dxmt;

  if (ppError) {
    *ppError = nullptr;
  }
  auto errorObj = new SM50ErrorInternal();
  llvm::raw_svector_ostream errorOut(errorObj->buf);
  if (ppBitcode == nullptr) {
    errorOut << "ppBitcode can not be null\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  // pArgs is ignored for now
  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  auto &shader_info = ((dxmt::dxbc::SM50ShaderInternal *)pGeometryShader)->shader_info;

  auto pModule = std::make_unique<Module>("shader.air", context);
  initializeModule(*pModule);

  if (auto err = dxmt::dxbc::convert_dxbc_vertex_for_geometry_shader(
        (dxbc::SM50ShaderInternal *)pVertexShader, FunctionName,
        (dxbc::SM50ShaderInternal *)pGeometryShader, context, *pModule,
        pVertexShaderArgs
      )) {
    llvm::handleAllErrors(std::move(err), [&](const UnsupportedFeature &u) {
      errorOut << u.msg;
    });
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  if (shader_info.use_msad)
    linkMSAD(*pModule);
  if (shader_info.use_samplepos)
    linkSamplePos(*pModule);

  runOptimizationPasses(*pModule);

  // Serialize AIR
  auto compiled = new SM50CompiledBitcodeInternal();

  raw_svector_ostream OS(compiled->vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  pModule.reset();

  *ppBitcode = (sm50_bitcode_t)compiled;
  return 0;
}

AIRCONV_API int SM50CompileGeometryPipelineGeometry(
  sm50_shader_t pVertexShader, sm50_shader_t pGeometryShader,
  struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pGeometryShaderArgs,
  const char *FunctionName, sm50_bitcode_t *ppBitcode, sm50_error_t *ppError
) {
  using namespace llvm;
  using namespace dxmt;

  if (ppError) {
    *ppError = nullptr;
  }
  auto errorObj = new SM50ErrorInternal();
  llvm::raw_svector_ostream errorOut(errorObj->buf);
  if (ppBitcode == nullptr) {
    errorOut << "ppBitcode can not be null\0";
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  // pArgs is ignored for now
  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  auto &shader_info =
    ((dxmt::dxbc::SM50ShaderInternal *)pGeometryShader)->shader_info;

  auto pModule = std::make_unique<Module>("shader.air", context);
  initializeModule(*pModule);

  if (auto err = dxmt::dxbc::convert_dxbc_geometry_shader(
        (dxbc::SM50ShaderInternal *)pGeometryShader, FunctionName,
        (dxbc::SM50ShaderInternal *)pVertexShader, context, *pModule,
        pGeometryShaderArgs
      )) {
    llvm::handleAllErrors(std::move(err), [&](const UnsupportedFeature &u) {
      errorOut << u.msg;
    });
    *ppError = (sm50_error_t)errorObj;
    return 1;
  }

  if (shader_info.use_msad)
    linkMSAD(*pModule);
  if (shader_info.use_samplepos)
    linkSamplePos(*pModule);
  
  runOptimizationPasses(*pModule);

  // Serialize AIR
  auto compiled = new SM50CompiledBitcodeInternal();

  raw_svector_ostream OS(compiled->vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  pModule.reset();

  *ppBitcode = (sm50_bitcode_t)compiled;
  return 0;
}

AIRCONV_API void SM50GetCompiledBitcode(
  sm50_bitcode_t pBitcode, SM50_COMPILED_BITCODE *pData
) {
  auto pBitcodeInternal = (SM50CompiledBitcodeInternal *)pBitcode;
  pData->Data = pBitcodeInternal->vec.data();
  pData->Size = pBitcodeInternal->vec.size();
}

AIRCONV_API void SM50DestroyBitcode(sm50_bitcode_t pBitcode) {
  auto pBitcodeInternal = (SM50CompiledBitcodeInternal *)pBitcode;
  delete pBitcodeInternal;
}

AIRCONV_API size_t SM50GetErrorMessage(sm50_error_t pError, char *pBuffer, size_t BufferSize) {
  auto pInternal = (SM50ErrorInternal *)pError;
  auto str_len = std::min(pInternal->buf.size(), BufferSize - 1);
  memcpy(pBuffer, pInternal->buf.data(), str_len);
  pBuffer[str_len] = '\0';
  return str_len;
}

AIRCONV_API void SM50FreeError(sm50_error_t pError) {
  if (pError == nullptr)
    return;
  auto pInternal = (SM50ErrorInternal *)pError;
  delete pInternal;
}
