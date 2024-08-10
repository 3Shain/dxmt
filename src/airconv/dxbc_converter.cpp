#include "dxbc_converter.hpp"
#include "DXBCParser/BlobContainer.h"
#include "DXBCParser/DXBCUtils.h"
#include "DXBCParser/ShaderBinary.h"
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "DXBCParser/winerror.h"
#include "air_signature.hpp"
#include "air_type.hpp"
#include "airconv_error.hpp"
#include "dxbc_constants.hpp"
#include "dxbc_signature.hpp"
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

/* separated implementation details */
#include "dxbc_converter_inc.hpp"
#include "dxbc_converter_instruction_inc.hpp"
#include "metallib_writer.hpp"
#include "shader_common.hpp"

#include "airconv_context.hpp"
#include "airconv_public.h"

#include "abrt_handle.h"

char dxmt::UnsupportedFeature::ID;

class SM50ShaderInternal {
public:
  dxmt::dxbc::ShaderInfo shader_info;
  dxmt::air::FunctionSignatureBuilder func_signature;
  std::shared_ptr<dxmt::dxbc::BasicBlock> entry;
  std::vector<std::function<void(dxmt::dxbc::IREffect &)>> prelogue_;
  std::vector<std::function<void(dxmt::dxbc::IRValue &)>> epilogue_;
  microsoft::D3D10_SB_TOKENIZED_PROGRAM_TYPE shader_type;
  uint32_t max_input_register = 0;
  uint32_t max_output_register = 0;
  std::vector<MTL_SM50_SHADER_ARGUMENT> args_reflection_cbuffer;
  std::vector<MTL_SM50_SHADER_ARGUMENT> args_reflection;
  uint32_t threadgroup_size[3] = {0};
};

class SM50CompiledBitcodeInternal {
public:
  llvm::SmallVector<char, 0> vec;
};

class SM50ErrorInternal {
public:
  llvm::SmallVector<char, 0> buf;
};

namespace dxmt::dxbc {

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

llvm::Error convertDXBC(
  SM50Shader *pShader, const char *name, llvm::LLVMContext &context,
  llvm::Module &module, SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs
) {
  using namespace microsoft;

  auto pShaderInternal = (SM50ShaderInternal *)pShader;
  auto func_signature = pShaderInternal->func_signature; // we need to copy one!
  auto shader_info = &(pShaderInternal->shader_info);
  auto shader_type = pShaderInternal->shader_type;

  uint32_t binding_table_index = ~0u;
  uint32_t cbuf_table_index = ~0u;
  uint32_t const &max_input_register = pShaderInternal->max_input_register;
  uint32_t const &max_output_register = pShaderInternal->max_output_register;
  uint64_t sign_mask = 0;
  uint32_t pso_sample_mask = 0xffffffff;
  SM50_SHADER_COMPILATION_ARGUMENT_DATA *arg = pArgs;
  SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA *vertex_so = nullptr;
  uint64_t debug_id = ~0u;
  while (arg) {
    switch (arg->type) {
    case SM50_SHADER_COMPILATION_INPUT_SIGN_MASK:
      sign_mask =
        ((SM50_SHADER_COMPILATION_INPUT_SIGN_MASK_DATA *)arg)->sign_mask;
      break;
    case SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT:
      if (shader_type != microsoft::D3D10_SB_VERTEX_SHADER)
        break;
      vertex_so = (SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA *)arg;
      break;
    case SM50_SHADER_DEBUG_IDENTITY:
      debug_id = ((SM50_SHADER_DEBUG_IDENTITY_DATA *)arg)->id;
      break;
    case SM50_SHADER_PSO_SAMPLE_MASK:
      pso_sample_mask = 
        ((SM50_SHADER_PSO_SAMPLE_MASK_DATA *)arg)->sample_mask;
      break;
    }
    arg = (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)arg->next;
  }

  IREffect prelogue([](auto) { return std::monostate(); });
  IRValue epilogue([](struct context ctx) -> pvalue {
    auto retTy = ctx.function->getReturnType();
    if (retTy->isVoidTy()) {
      return nullptr;
    }
    return llvm::UndefValue::get(retTy);
  });
  for (auto &p : pShaderInternal->prelogue_) {
    p(prelogue);
  }
  for (auto &e : pShaderInternal->epilogue_) {
    if (vertex_so)
      continue;
    e(epilogue);
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
  if(pso_sample_mask != 0xffffffff) {
    auto assigned_index = func_signature.DefineOutput(air::OutputCoverageMask {});
    epilogue >> [=](pvalue value) -> IRValue {
      return make_irvalue([=](struct context ctx) {
      auto &builder = ctx.builder;
      if(ctx.resource.coverage_mask_reg) return value;
      return builder.CreateInsertValue(value, builder.getInt32(ctx.pso_sample_mask), {assigned_index});
    });
    };
  }

  io_binding_map resource_map;

  // post convert
  for (auto &[range_id, cbv] : shader_info->cbufferMap) {
    // TODO: abstract SM 5.0 binding
    auto index = cbv.arg_index;
    resource_map.cb_range_map[range_id] = [=, &cbuf_table_index](pvalue) {
      // ignore index in SM 5.0
      return get_item_in_argbuf_binding_table(cbuf_table_index, index);
    };
  }
  for (auto &[range_id, sampler] : shader_info->samplerMap) {
    // TODO: abstract SM 5.0 binding
    auto index = sampler.arg_index;
    resource_map.sampler_range_map[range_id] = [=,
                                                &binding_table_index](pvalue) {
      // ignore index in SM 5.0
      return get_item_in_argbuf_binding_table(binding_table_index, index);
    };
  }
  for (auto &[range_id, srv] : shader_info->srvMap) {
    if (srv.resource_type != shader::common::ResourceType::NonApplicable) {
      // TODO: abstract SM 5.0 binding
      auto access =
        srv.sampled ? air::MemoryAccess::sample : air::MemoryAccess::read;
      auto texture_kind =
        air::to_air_resource_type(srv.resource_type, srv.compared);
      auto scaler_type = air::to_air_scaler_type(srv.scaler_type);
      auto index = srv.arg_index;
      resource_map.srv_range_map[range_id] = {
        air::MSLTexture{
          .component_type = scaler_type,
          .memory_access = access,
          .resource_kind = texture_kind,
        },
        [=, &binding_table_index](pvalue) {
          // ignore index in SM 5.0
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        }
      };
    } else {
      auto argbuf_index_bufptr = srv.arg_index;
      auto argbuf_index_size = srv.arg_size_index;
      resource_map.srv_buf_range_map[range_id] = {
        [=, &binding_table_index](pvalue) {
          return get_item_in_argbuf_binding_table(
            binding_table_index, argbuf_index_bufptr
          );
        },
        [=, &binding_table_index](pvalue) {
          return get_item_in_argbuf_binding_table(
            binding_table_index, argbuf_index_size
          );
        },
        srv.strucure_stride
      };
    }
  }
  for (auto &[range_id, uav] : shader_info->uavMap) {
    auto access = uav.written ? (uav.read ? air::MemoryAccess::read_write
                                          : air::MemoryAccess::write)
                              : air::MemoryAccess::read;
    if (uav.resource_type != shader::common::ResourceType::NonApplicable) {
      auto texture_kind = air::to_air_resource_type(uav.resource_type);
      auto scaler_type = air::to_air_scaler_type(uav.scaler_type);
      auto index = uav.arg_index;
      resource_map.uav_range_map[range_id] = {
        air::MSLTexture{
          .component_type = scaler_type,
          .memory_access = access,
          .resource_kind = texture_kind,
        },
        [=, &binding_table_index](pvalue) {
          // ignore index in SM 5.0
          return get_item_in_argbuf_binding_table(binding_table_index, index);
        }
      };
    } else {
      auto argbuf_index_bufptr = uav.arg_index;
      auto argbuf_index_size = uav.arg_size_index;
      resource_map.uav_buf_range_map[range_id] = {
        [=, &binding_table_index](pvalue) {
          return get_item_in_argbuf_binding_table(
            binding_table_index, argbuf_index_bufptr
          );
        },
        [=, &binding_table_index](pvalue) {
          return get_item_in_argbuf_binding_table(
            binding_table_index, argbuf_index_size
          );
        },
        uav.strucure_stride
      };
      if (uav.with_counter) {
        auto argbuf_index_counterptr = uav.arg_counter_index;
        resource_map.uav_counter_range_map[range_id] =
          [=, &binding_table_index](pvalue) {
            return get_item_in_argbuf_binding_table(
              binding_table_index, argbuf_index_counterptr
            );
          };
      }
    }
  }
  air::AirType types(context);
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
  if (shader_info->immConstantBufferData.size()) {
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
    icb->setAlignment(llvm::Align(16));
    resource_map.icb = icb;
  }

  if (!shader_info->binding_table.empty()) {
    auto [type, metadata] =
      shader_info->binding_table.Build(context, module.getDataLayout());
    binding_table_index =
      func_signature.DefineInput(air::ArgumentBindingIndirectBuffer{
        .location_index = 30, // kArgumentBufferBindIndex
        .array_size = 1,
        .memory_access = air::MemoryAccess::read,
        .address_space = air::AddressSpace::constant,
        .struct_type = type,
        .struct_type_info = metadata,
        .arg_name = "binding_table",
      });
  }
  if (!shader_info->binding_table_cbuffer.empty()) {
    auto [type, metadata] =
      shader_info->binding_table_cbuffer.Build(context, module.getDataLayout());
    cbuf_table_index =
      func_signature.DefineInput(air::ArgumentBindingIndirectBuffer{
        .location_index = 29, // kConstantBufferBindIndex
        .array_size = 1,
        .memory_access = air::MemoryAccess::read,
        .address_space = air::AddressSpace::constant,
        .struct_type = type,
        .struct_type_info = metadata,
        .arg_name = "cbuffer_table",
      });
  }
  auto [function, function_metadata] = func_signature.CreateFunction(
    name, context, module, sign_mask, vertex_so != nullptr
  );

  auto entry_bb = llvm::BasicBlock::Create(context, "entry", function);
  auto epilogue_bb = llvm::BasicBlock::Create(context, "epilogue", function);
  llvm::IRBuilder<> builder(entry_bb);
  // what a mess
  if (auto options = module.getNamedMetadata("air.compile_options")) {
    for (auto operand : options->operands()) {
      if (isa<llvm::MDTuple>(operand) &&
          cast<llvm::MDTuple>(operand)->getNumOperands() == 1 &&
          isa<llvm::MDString>(cast<llvm::MDTuple>(operand)->getOperand(0)) &&
          cast<llvm::MDString>(cast<llvm::MDTuple>(operand)->getOperand(0))
              ->getString()
              .compare("air.compile.fast_math_enable") == 0) {
        builder.getFastMathFlags().setFast(true);
      }
    }
  }
  resource_map.input.ptr_int4 =
    builder.CreateAlloca(llvm::ArrayType::get(types._int4, max_input_register));
  resource_map.input.ptr_float4 = builder.CreateBitCast(
    resource_map.input.ptr_int4,
    llvm::ArrayType::get(types._float4, max_input_register)->getPointerTo()
  );
  resource_map.output.ptr_int4 =
    builder.CreateAlloca(llvm::ArrayType::get(types._int4, max_output_register)
    );
  resource_map.output.ptr_float4 = builder.CreateBitCast(
    resource_map.output.ptr_int4,
    llvm::ArrayType::get(types._float4, max_output_register)->getPointerTo()
  );
  resource_map.temp.ptr_int4 = builder.CreateAlloca(
    llvm::ArrayType::get(types._int4, shader_info->tempRegisterCount)
  );
  resource_map.temp.ptr_float4 = builder.CreateBitCast(
    resource_map.temp.ptr_int4,
    llvm::ArrayType::get(types._float4, shader_info->tempRegisterCount)
      ->getPointerTo()
  );
  if (shader_info->immConstantBufferData.size()) {
    resource_map.icb_float = builder.CreateBitCast(
      resource_map.icb,
      llvm::ArrayType::get(
        types._float4, shader_info->immConstantBufferData.size()
      )
        ->getPointerTo(2)
    );
  }
  for (auto &[idx, info] : shader_info->indexableTempRegisterCounts) {
    auto &[numRegisters, mask] = info;
    auto channel_count = std::bit_width(mask);
    auto ptr_int_vec = builder.CreateAlloca(llvm::ArrayType::get(
      llvm::FixedVectorType::get(types._int, channel_count), numRegisters
    ));
    auto ptr_float_vec = builder.CreateBitCast(
      ptr_int_vec,
      llvm::ArrayType::get(
        llvm::FixedVectorType::get(types._float, channel_count), numRegisters
      )
        ->getPointerTo()
    );
    resource_map.indexable_temp_map[idx] = {
      ptr_int_vec, ptr_float_vec, (uint32_t)channel_count
    };
  }
  if(shader_info->use_cmp_exch) {
    resource_map.cmp_exch_temp = builder.CreateAlloca(types._int);
  }

  struct context ctx {
    .builder = builder, .llvm = context, .module = module, .function = function,
    .resource = resource_map, .types = types, .pso_sample_mask = pso_sample_mask
  };
  // then we can start build ... real IR code (visit all basicblocks)
  auto prelogue_result = prelogue.build(ctx);
  if (auto err = prelogue_result.takeError()) {
    return err;
  }
  auto real_entry =
    convert_basicblocks(pShaderInternal->entry, ctx, epilogue_bb);
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

  if (shader_type == D3D10_SB_VERTEX_SHADER) {
    module.getOrInsertNamedMetadata("air.vertex")
      ->addOperand(function_metadata);
  } else if (shader_type == D3D10_SB_PIXEL_SHADER) {
    module.getOrInsertNamedMetadata("air.fragment")
      ->addOperand(function_metadata);
  } else if (shader_type == D3D11_SB_COMPUTE_SHADER) {
    module.getOrInsertNamedMetadata("air.kernel")
      ->addOperand(function_metadata);
  } else {
    // throw
    assert(0 && "Unsupported shader type");
  }
  return llvm::Error::success();
};
} // namespace dxmt::dxbc

int SM50Initialize(
  const void *pBytecode, size_t BytecodeSize, SM50Shader **ppShader,
  MTL_SHADER_REFLECTION *pRefl, SM50Error **ppError
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
    *ppError = (SM50Error *)errorObj;
    return 1;
  }

  CDXBCParser DXBCParser;
  if (DXBCParser.ReadDXBC(pBytecode, BytecodeSize) != S_OK) {
    errorOut << "Invalid DXBC bytecode\0";
    *ppError = (SM50Error *)errorObj;
    return 1;
  }

  UINT codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShaderEx);
  if (codeBlobIdx == DXBC_BLOB_NOT_FOUND) {
    codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShader);
  }
  if (codeBlobIdx == DXBC_BLOB_NOT_FOUND) {
    errorOut << "Invalid DXBC bytecode: shader blob not found\0";
    *ppError = (SM50Error *)errorObj;
    return 1;
  }
  const void *codeBlob = DXBCParser.GetBlob(codeBlobIdx);

  CShaderToken *ShaderCode = (CShaderToken *)(BYTE *)codeBlob;
  // 1. Collect information about the shader.
  D3D10ShaderBinary::CShaderCodeParser CodeParser(ShaderCode);
  CSignatureParser inputParser;
  if (DXBCGetInputSignature(pBytecode, &inputParser) != S_OK) {
    errorOut << "Invalid DXBC bytecode: input signature not found\0";
    *ppError = (SM50Error *)errorObj;
    return 1;
  }
  CSignatureParser outputParser;
  if (DXBCGetOutputSignature(pBytecode, &outputParser) != S_OK) {
    errorOut << "Invalid DXBC bytecode: output signature not found\0";
    *ppError = (SM50Error *)errorObj;
    return 1;
  }

  auto findInputElement = [&](auto matcher) -> Signature {
    const D3D11_SIGNATURE_PARAMETER *parameters;
    inputParser.GetParameters(&parameters);
    for (unsigned i = 0; i < inputParser.GetNumParameters(); i++) {
      auto sig = Signature(parameters[i]);
      if (matcher(sig)) {
        return sig;
      }
    }
    assert(inputParser.GetNumParameters());
    assert(0 && "try to access an undefined input");
  };
  auto findOutputElement = [&](auto matcher) -> Signature {
    const D3D11_SIGNATURE_PARAMETER *parameters;
    outputParser.GetParameters(&parameters);
    for (unsigned i = 0; i < outputParser.GetNumParameters(); i++) {
      auto sig = Signature(parameters[i]);
      if (matcher(sig)) {
        return sig;
      }
    }
    assert(0 && "try to access an undefined output");
  };

  std::function<std::shared_ptr<BasicBlock>(
    const std::shared_ptr<BasicBlock> &ctx,
    const std::shared_ptr<BasicBlock> &block_after_endif,
    const std::shared_ptr<BasicBlock> &continue_point,
    const std::shared_ptr<BasicBlock> &break_point,
    const std::shared_ptr<BasicBlock> &return_point,
    std::shared_ptr<BasicBlockSwitch> &switch_context
  )>
    readControlFlow;

  std::shared_ptr<BasicBlock> null_bb;
  std::shared_ptr<BasicBlockSwitch> null_switch_context;

  bool sm_ver_5_1_ = CodeParser.ShaderMajorVersion() == 5 &&
                     CodeParser.ShaderMinorVersion() >= 1;

  auto sm50_shader = new SM50ShaderInternal();
  sm50_shader->shader_type = CodeParser.ShaderType();
  auto shader_info = &(sm50_shader->shader_info);
  auto &func_signature = sm50_shader->func_signature;

  uint32_t &max_input_register = sm50_shader->max_input_register;
  uint32_t &max_output_register = sm50_shader->max_output_register;

  auto &prelogue_ = sm50_shader->prelogue_;
  auto &epilogue_ = sm50_shader->epilogue_;

  readControlFlow = [&](
                      const std::shared_ptr<BasicBlock> &ctx,
                      const std::shared_ptr<BasicBlock> &block_after_endif,
                      const std::shared_ptr<BasicBlock> &continue_point,
                      const std::shared_ptr<BasicBlock> &break_point,
                      const std::shared_ptr<BasicBlock> &return_point,
                      std::shared_ptr<BasicBlockSwitch> &switch_context
                    ) -> std::shared_ptr<BasicBlock> {
    while (!CodeParser.EndOfShader()) {
      D3D10ShaderBinary::CInstruction Inst;
      CodeParser.ParseInstruction(&Inst);
      switch (Inst.m_OpCode) {
#pragma region control flow
      case D3D10_SB_OPCODE_IF: {
        // scope start: if-else-endif
        auto true_ = std::make_shared<BasicBlock>("if_true");
        auto alternative_ = std::make_shared<BasicBlock>("if_alternative");
        // alternative_ might be the block after ENDIF, but ELSE is possible
        ctx->target = BasicBlockConditionalBranch{
          readCondition(Inst, 0), true_, alternative_
        };
        auto after_endif = readControlFlow(
          true_, alternative_, continue_point, break_point, return_point,
          null_switch_context
        ); // read till ENDIF
        // scope end
        return readControlFlow(
          after_endif, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_ELSE: {
        assert(block_after_endif.get() && "");
        auto real_exit = std::make_shared<BasicBlock>("endif");
        ctx->target = BasicBlockUnconditionalBranch{real_exit};
        return readControlFlow(
          block_after_endif, real_exit, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_ENDIF: {
        assert(block_after_endif.get() && "");
        ctx->target = BasicBlockUnconditionalBranch{block_after_endif};
        return block_after_endif;
      }
      case D3D10_SB_OPCODE_LOOP: {
        auto loop_entrance = std::make_shared<BasicBlock>("loop_entrance");
        auto after_endloop = std::make_shared<BasicBlock>("endloop");
        // scope start: loop
        ctx->target = BasicBlockUnconditionalBranch{loop_entrance};
        auto _ = readControlFlow(
          loop_entrance, null_bb, loop_entrance, after_endloop, return_point,
          null_switch_context
        ); // return from ENDLOOP
        assert(_.get() == after_endloop.get());
        // scope end
        return readControlFlow(
          after_endloop, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_BREAK: {
        ctx->target = BasicBlockUnconditionalBranch{break_point};
        auto after_break = std::make_shared<BasicBlock>("after_break");
        return readControlFlow(
          after_break, block_after_endif, continue_point, break_point,
          return_point, switch_context
        ); // ?
      }
      case D3D10_SB_OPCODE_BREAKC: {
        auto after_break = std::make_shared<BasicBlock>("after_breakc");
        ctx->target = BasicBlockConditionalBranch{
          readCondition(Inst, 0), break_point, after_break
        };
        return readControlFlow(
          after_break, block_after_endif, continue_point, break_point,
          return_point, switch_context
        ); // ?
      }
      case D3D10_SB_OPCODE_CONTINUE: {
        ctx->target = BasicBlockUnconditionalBranch{continue_point};
        auto after_continue = std::make_shared<BasicBlock>("after_continue");
        return readControlFlow(
          after_continue, block_after_endif, continue_point, break_point,
          return_point,
          switch_context
        ); // ?
      }
      case D3D10_SB_OPCODE_CONTINUEC: {
        auto after_continue = std::make_shared<BasicBlock>("after_continuec");
        ctx->target = BasicBlockConditionalBranch{
          readCondition(Inst, 0), continue_point, after_continue
        };
        return readControlFlow(
          after_continue, block_after_endif, continue_point, break_point,
          return_point,
          switch_context
        ); // ?
      }
      case D3D10_SB_OPCODE_ENDLOOP: {
        ctx->target = BasicBlockUnconditionalBranch{continue_point};
        return break_point;
      }
      case D3D10_SB_OPCODE_SWITCH: {
        auto after_endswitch = std::make_shared<BasicBlock>("endswitch");
        // scope start: switch
        auto local_switch_context = std::make_shared<BasicBlockSwitch>();
        local_switch_context->value = readSrcOperand(Inst.Operand(0));
        auto empty_body = std::make_shared<BasicBlock>("switch_empty"
        ); // it will unconditional jump to
           // first case (and then ignored)
        auto _ = readControlFlow(
          empty_body, null_bb, continue_point, after_endswitch, return_point,
          local_switch_context
        );
        assert(_.get() == after_endswitch.get());
        ctx->target = std::move(*local_switch_context);
        // scope end
        return readControlFlow(
          after_endswitch, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_CASE: {
        auto case_body = std::make_shared<BasicBlock>("switch_case");
        // always fallthrough
        ctx->target = BasicBlockUnconditionalBranch{case_body};

        const D3D10ShaderBinary::COperandBase &O = Inst.m_Operands[0];
        DXASSERT_DXBC(
          O.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 &&
          O.m_NumComponents == D3D10_SB_OPERAND_1_COMPONENT
        );
        uint32_t case_value = O.m_Value[0];

        switch_context->cases.insert(std::make_pair(case_value, case_body));
        return readControlFlow(
          case_body, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_DEFAULT: {
        ctx->target = BasicBlockUnconditionalBranch{break_point};
        auto case_body = std::make_shared<BasicBlock>("switch_default");
        switch_context->case_default = case_body;
        return readControlFlow(
          case_body, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_ENDSWITCH: {
        ctx->target = BasicBlockUnconditionalBranch{break_point};
        return break_point;
      }
      case D3D10_SB_OPCODE_RET: {
        ctx->target = BasicBlockUnconditionalBranch{return_point};
        if (
            // if it's inside a loop or switch, break_point is not null
            break_point.get() == nullptr &&
            // if it's inside if-else, block_after_endif is not null
            block_after_endif.get() == nullptr) {
          // not inside any scope, this is the final ret
          return return_point;
        }
        auto after_ret = std::make_shared<BasicBlock>("after_ret");
        // if it's inside a scope, then return is not the end
        return readControlFlow(
          after_ret, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_RETC: {
        auto after_retc = std::make_shared<BasicBlock>("after_retc");
        ctx->target = BasicBlockConditionalBranch{
          readCondition(Inst, 0), return_point, after_retc
        };
        return readControlFlow(
          after_retc, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_DISCARD: {
        auto fulfilled_ = std::make_shared<BasicBlock>("discard_fulfilled");
        auto otherwise_ = std::make_shared<BasicBlock>("discard_otherwise");
        ctx->target = BasicBlockConditionalBranch{
          readCondition(Inst, 0), fulfilled_, otherwise_
        };
        fulfilled_->target = BasicBlockUnconditionalBranch{otherwise_};
        fulfilled_->instructions.push_back(InstPixelDiscard{});
        return readControlFlow(
          otherwise_, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
#pragma endregion
#pragma region declaration
      case D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER: {
        unsigned RangeID = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        unsigned CBufferSize = Inst.m_ConstantBufferDecl.Size;
        unsigned LB, RangeSize;
        switch (Inst.m_Operands[0].m_IndexDimension) {
        case D3D10_SB_OPERAND_INDEX_2D: // SM 5.0-
          LB = RangeID;
          RangeSize = 1;
          break;
        case D3D10_SB_OPERAND_INDEX_3D: // SM 5.1
          LB = Inst.m_Operands[0].m_Index[1].m_RegIndex;
          RangeSize = Inst.m_Operands[0].m_Index[2].m_RegIndex != UINT_MAX
                        ? Inst.m_Operands[0].m_Index[2].m_RegIndex - LB + 1
                        : UINT_MAX;
          break;
        default:
          DXASSERT_DXBC(false);
        }
        shader_info->cbufferMap[RangeID] = {
          .range =
            {.range_id = RangeID,
             .lower_bound = LB,
             .size = RangeSize,
             .space = Inst.m_ConstantBufferDecl.Space},
          .size_in_vec4 = CBufferSize,
          .arg_index = 0, // set it later
        };
        break;
      }
      case D3D10_SB_OPCODE_DCL_SAMPLER: {
        // Root signature bindings.
        unsigned RangeID = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        unsigned LB, RangeSize;
        switch (Inst.m_Operands[0].m_IndexDimension) {
        case D3D10_SB_OPERAND_INDEX_1D: // SM 5.0-
          LB = RangeID;
          RangeSize = 1;
          break;
        case D3D10_SB_OPERAND_INDEX_3D: // SM 5.1
          LB = Inst.m_Operands[0].m_Index[1].m_RegIndex;
          RangeSize = Inst.m_Operands[0].m_Index[2].m_RegIndex != UINT_MAX
                        ? Inst.m_Operands[0].m_Index[2].m_RegIndex - LB + 1
                        : UINT_MAX;
          break;
        default:
          DXASSERT_DXBC(false);
        }
        shader_info->samplerMap[RangeID] = {
          .range =
            {.range_id = RangeID,
             .lower_bound = LB,
             .size = RangeSize,
             .space = Inst.m_SamplerDecl.Space},
          .arg_index = 0, // set it later
        };
        // FIXME: SamplerMode ignored?
        break;
      }
      case D3D10_SB_OPCODE_DCL_RESOURCE:
      case D3D11_SB_OPCODE_DCL_RESOURCE_RAW:
      case D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED: {
        // Root signature bindings.
        unsigned RangeID = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        unsigned LB, RangeSize;
        if (sm_ver_5_1_) {
          LB = Inst.m_Operands[0].m_Index[1].m_RegIndex;
          RangeSize = Inst.m_Operands[0].m_Index[2].m_RegIndex != UINT_MAX
                        ? Inst.m_Operands[0].m_Index[2].m_RegIndex - LB + 1
                        : UINT_MAX;
        } else {
          LB = RangeID;
          RangeSize = 1;
        }
        ShaderResourceViewInfo srv;
        srv.range = {
          .range_id = RangeID,
          .lower_bound = LB,
          .size = RangeSize,
          .space = 0,
        };
        switch (Inst.OpCode()) {
        case D3D10_SB_OPCODE_DCL_RESOURCE: {
          srv.range.space = (Inst.m_ResourceDecl.Space);
          srv.resource_type =
            to_shader_resource_type(Inst.m_ResourceDecl.Dimension);
          srv.scaler_type =
            to_shader_scaler_type(Inst.m_ResourceDecl.ReturnType[0]);
          srv.strucure_stride = -1;
          // Inst.m_ResourceDecl.SampleCount
          break;
        }
        case D3D11_SB_OPCODE_DCL_RESOURCE_RAW: {
          srv.resource_type = ResourceType::NonApplicable;
          srv.range.space = Inst.m_RawSRVDecl.Space;
          srv.scaler_type = ScalerDataType::Uint;
          srv.strucure_stride = 0;
          break;
        }
        case D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED: {
          srv.resource_type = ResourceType::NonApplicable;
          srv.range.space = (Inst.m_StructuredSRVDecl.Space);
          srv.scaler_type = ScalerDataType::Uint;
          srv.strucure_stride = Inst.m_StructuredSRVDecl.ByteStride;
          break;
        }
        default:;
        }

        shader_info->srvMap[RangeID] = srv;

        break;
      }
      case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED:
      case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW:
      case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
        // Root signature bindings.
        unsigned RangeID = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        unsigned LB, RangeSize;
        if (sm_ver_5_1_) {
          LB = Inst.m_Operands[0].m_Index[1].m_RegIndex;
          RangeSize = Inst.m_Operands[0].m_Index[2].m_RegIndex != UINT_MAX
                        ? Inst.m_Operands[0].m_Index[2].m_RegIndex - LB + 1
                        : UINT_MAX;
        } else {
          LB = RangeID;
          RangeSize = 1;
        }

        UnorderedAccessViewInfo uav;
        uav.range = {
          .range_id = RangeID,
          .lower_bound = LB,
          .size = RangeSize,
          .space = 0,
        };
        unsigned Flags = 0;
        switch (Inst.OpCode()) {
        case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED: {
          uav.range.space = (Inst.m_TypedUAVDecl.Space);
          Flags = Inst.m_TypedUAVDecl.Flags;
          uav.resource_type =
            to_shader_resource_type(Inst.m_TypedUAVDecl.Dimension);
          uav.scaler_type =
            to_shader_scaler_type(Inst.m_TypedUAVDecl.ReturnType[0]);
          uav.strucure_stride = -1;
          break;
        }
        case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW: {
          uav.range.space = (Inst.m_RawUAVDecl.Space);
          uav.resource_type = ResourceType::NonApplicable;
          Flags = Inst.m_RawUAVDecl.Flags;
          uav.scaler_type = ScalerDataType::Uint;
          uav.strucure_stride = 0;
          break;
        }
        case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
          uav.range.space = (Inst.m_StructuredUAVDecl.Space);
          uav.resource_type = ResourceType::NonApplicable;
          Flags = Inst.m_StructuredUAVDecl.Flags;
          uav.scaler_type = ScalerDataType::Uint;
          uav.strucure_stride = Inst.m_StructuredUAVDecl.ByteStride;
          break;
        }
        default:;
        }

        uav.global_coherent =
          ((Flags & D3D11_SB_GLOBALLY_COHERENT_ACCESS) != 0);
        uav.rasterizer_order =
          ((Flags & D3D11_SB_RASTERIZER_ORDERED_ACCESS) != 0);

        shader_info->uavMap[RangeID] = uav;
        break;
      }
      case D3D10_SB_OPCODE_DCL_TEMPS: {
        shader_info->tempRegisterCount = Inst.m_TempsDecl.NumTemps;
        break;
      }
      case D3D10_SB_OPCODE_DCL_INDEXABLE_TEMP: {
        shader_info->indexableTempRegisterCounts[Inst.m_IndexableTempDecl
                                                   .IndexableTempNumber] =
          std::make_pair(
            Inst.m_IndexableTempDecl.NumRegisters,
            Inst.m_IndexableTempDecl.Mask >> 4
          );
        break;
      }
      case D3D11_SB_OPCODE_DCL_THREAD_GROUP: {
        sm50_shader->threadgroup_size[0] = Inst.m_ThreadGroupDecl.x;
        sm50_shader->threadgroup_size[1] = Inst.m_ThreadGroupDecl.y;
        sm50_shader->threadgroup_size[2] = Inst.m_ThreadGroupDecl.z;
        func_signature.UseMaxWorkgroupSize(
          Inst.m_ThreadGroupDecl.x * Inst.m_ThreadGroupDecl.y *
          Inst.m_ThreadGroupDecl.z
        );
        break;
      }
      case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
      case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
        ThreadgroupBufferInfo tgsm;
        if (Inst.OpCode() ==
            D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW) {
          tgsm.stride = 0;
          tgsm.size = Inst.m_RawTGSMDecl.ByteCount;
          tgsm.size_in_uint = tgsm.size / 4;
          tgsm.structured = false;
          assert(
            (Inst.m_RawTGSMDecl.ByteCount & 0b11) == 0
          ); // is multiple of 4
        } else {
          tgsm.stride = Inst.m_StructuredTGSMDecl.StructByteStride;
          tgsm.size = Inst.m_StructuredTGSMDecl.StructCount;
          tgsm.size_in_uint = tgsm.stride * tgsm.size / 4;
          tgsm.structured = true;
          assert(
            (Inst.m_StructuredTGSMDecl.StructByteStride & 0b11) == 0
          ); // is multiple of 4
        }

        shader_info->tgsmMap[Inst.m_Operands[0].m_Index[0].m_RegIndex] = tgsm;
        break;
      }
      case D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS: {
        if (Inst.m_GlobalFlagsDecl.Flags &
            D3D11_SB_GLOBAL_FLAG_FORCE_EARLY_DEPTH_STENCIL) {
          func_signature.UseEarlyFragmentTests();
        }
        if (Inst.m_GlobalFlagsDecl.Flags &
            D3D11_1_SB_GLOBAL_FLAG_SKIP_OPTIMIZATION) {
          shader_info->skipOptimization = true;
        }
        if ((Inst.m_GlobalFlagsDecl.Flags &
             D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED) == 0) {
          shader_info->refactoringAllowed = false;
        }
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT_SIV: {
        assert(0 && "dcl_input_siv should not happen for now");
        // because we don't support hull/domain/geometry
        // and pixel shader has its own dcl_input_ps
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT_SGV: {
        unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
        // auto MinPrecision = Inst.m_Operands[0].m_MinPrecision; // not used
        auto sgv = Inst.m_InputDeclSGV.Name;
        switch (sgv) {
        case D3D10_SB_NAME_VERTEX_ID: {
          auto assigned_index = func_signature.DefineInput(InputVertexID{});
          auto assigned_index_base =
            func_signature.DefineInput(InputBaseVertex{});
          prelogue_.push_back([=](IREffect &prelogue) {
            prelogue << make_effect_bind([=](struct context ctx) {
              auto vertex_id = ctx.function->getArg(assigned_index);
              auto base_vertex = ctx.function->getArg(assigned_index_base);
              auto const_index =
                llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, reg, false});
              return store_at_vec4_array_masked(
                ctx.resource.input.ptr_int4, const_index,
                ctx.builder.CreateSub(vertex_id, base_vertex), mask
              );
            });
          });
          break;
        }
        case D3D10_SB_NAME_INSTANCE_ID: {
          auto assigned_index = func_signature.DefineInput(InputInstanceID{});
          auto assigned_index_base =
            func_signature.DefineInput(InputBaseInstance{});
          prelogue_.push_back([=](IREffect &prelogue) {
            // and perform side effect here
            prelogue << make_effect_bind([=](struct context ctx) {
              auto instance_id = ctx.function->getArg(assigned_index);
              auto base_instance = ctx.function->getArg(assigned_index_base);
              auto const_index =
                llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, reg, false});
              return store_at_vec4_array_masked(
                ctx.resource.input.ptr_int4, const_index,
                ctx.builder.CreateSub(instance_id, base_instance), mask
              );
            });
          });
          break;
        }
        default:
          assert(0 && "Unexpected/unhandled input system value");
          break;
        }
        max_input_register = std::max(reg + 1, max_input_register);
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT: {
        D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;

        switch (RegType) {
        case D3D11_SB_OPERAND_TYPE_INPUT_COVERAGE_MASK: {
          auto assigned_index =
            func_signature.DefineInput(InputInputCoverage{});
          prelogue_.push_back([=](IREffect &prelogue) {
            prelogue << make_effect([=](struct context ctx) {
              auto attr = ctx.function->getArg(assigned_index);
              ctx.resource.coverage_mask_arg = attr;
              return std::monostate{};
            });
          });
          break;
        }

        case D3D11_SB_OPERAND_TYPE_INNER_COVERAGE:
          assert(0);
          break;

        case D3D11_SB_OPERAND_TYPE_CYCLE_COUNTER:
          break; // ignore it atm
        case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID: {
          auto assigned_index =
            func_signature.DefineInput(InputThreadPositionInGrid{});
          prelogue_.push_back([=](IREffect &prelogue) {
            prelogue << make_effect([=](struct context ctx) {
              auto attr = ctx.function->getArg(assigned_index);
              ctx.resource.thread_id_arg = attr;
              return std::monostate{};
            });
          });
          break;
        }
        case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_GROUP_ID: {
          auto assigned_index =
            func_signature.DefineInput(InputThreadgroupPositionInGrid{});
          prelogue_.push_back([=](IREffect &prelogue) {
            prelogue << make_effect([=](struct context ctx) {
              auto attr = ctx.function->getArg(assigned_index);
              ctx.resource.thread_group_id_arg = attr;
              return std::monostate{};
            });
          });
          break;
        }
        case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP: {
          auto assigned_index =
            func_signature.DefineInput(InputThreadPositionInThreadgroup{});
          prelogue_.push_back([=](IREffect &prelogue) {
            prelogue << make_effect([=](struct context ctx) {
              auto attr = ctx.function->getArg(assigned_index);
              ctx.resource.thread_id_in_group_arg = attr;
              return std::monostate{};
            });
          });
          break;
        }
        case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED: {
          auto assigned_index =
            func_signature.DefineInput(InputThreadIndexInThreadgroup{});
          prelogue_.push_back([=](IREffect &prelogue) {
            prelogue << make_effect([=](struct context ctx) {
              auto attr = ctx.function->getArg(assigned_index);
              ctx.resource.thread_id_in_group_flat_arg = attr;
              return std::monostate{};
            });
          });
          break;
        }
        case D3D11_SB_OPERAND_TYPE_INPUT_DOMAIN_POINT:
        case D3D11_SB_OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID:
        case D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID:
        case D3D11_SB_OPERAND_TYPE_INPUT_FORK_INSTANCE_ID:
        case D3D11_SB_OPERAND_TYPE_INPUT_JOIN_INSTANCE_ID:
        case D3D11_SB_OPERAND_TYPE_INPUT_GS_INSTANCE_ID:
          assert(0 && "unimplemented input registers");
          break;

        default: {
          unsigned reg = 0;
          switch (Inst.m_Operands[0].m_IndexDimension) {
          case D3D10_SB_OPERAND_INDEX_1D:
            reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
            break;
          case D3D10_SB_OPERAND_INDEX_2D:
            assert(0 && "Hull/Domain shader not supported yet");
            break;
          default:
            assert(0 && "there should no other index dimensions");
          }

          if (RegType == D3D10_SB_OPERAND_TYPE_INPUT) {
            auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
            auto sig = findInputElement([=](Signature &sig) {
              return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
            });
            auto assigned_index = func_signature.DefineInput(InputVertexStageIn{
              .attribute = reg,
              .type = (InputAttributeComponentType)sig.componentType(),
              .name = sig.fullSemanticString()
            });
            prelogue_.push_back([=](IREffect &prelogue) {
              prelogue << init_input_reg(assigned_index, reg, mask);
            });
          } else {
            assert(0 && "Unknown input register type");
          }
          max_input_register = std::max(reg + 1, max_input_register);
          break;
        }
        }
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT_PS_SIV: {
        unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
        auto siv = Inst.m_InputPSDeclSIV.Name;
        auto interpolation =
          to_air_interpolation(Inst.m_InputPSDeclSIV.InterpolationMode);
        uint32_t assigned_index;
        switch (siv) {
        case D3D10_SB_NAME_POSITION:
          // assert(
          //   interpolation == Interpolation::center_no_perspective ||
          //   // in case it's per-sample, FIXME: will this cause problem?
          //   interpolation == Interpolation::sample_no_perspective
          // );
          assigned_index = func_signature.DefineInput(
            // the only supported interpolation for [[position]]
            InputPosition{.interpolation = interpolation}
          );
          break;
        case D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX:
          assert(interpolation == Interpolation::flat);
          assigned_index =
            func_signature.DefineInput(InputRenderTargetArrayIndex{});
          break;
        case D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX:
          assert(interpolation == Interpolation::flat);
          assigned_index =
            func_signature.DefineInput(InputViewportArrayIndex{});
          break;
        default:
          assert(0 && "Unexpected/unhandled input system value");
          break;
        }
        prelogue_.push_back([=](IREffect &prelogue) {
          prelogue << init_input_reg(assigned_index, reg, mask, siv == D3D10_SB_NAME_POSITION);
        });
        max_input_register = std::max(reg + 1, max_input_register);
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT_PS_SGV: {
        unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
        auto siv = Inst.m_InputPSDeclSIV.Name;
        auto interpolation =
          to_air_interpolation(Inst.m_InputPSDeclSGV.InterpolationMode);
        uint32_t assigned_index;
        switch (siv) {
        case microsoft::D3D10_SB_NAME_IS_FRONT_FACE:
          assert(interpolation == Interpolation::flat);
          assigned_index = func_signature.DefineInput(InputFrontFacing{});
          break;
        case microsoft::D3D10_SB_NAME_SAMPLE_INDEX:
          assert(interpolation == Interpolation::flat);
          assigned_index = func_signature.DefineInput(InputSampleIndex{});
          break;
        case microsoft::D3D10_SB_NAME_PRIMITIVE_ID:
          assigned_index = 
            func_signature.DefineInput(InputPrimitiveID{});
          break;
        default:
          assert(0 && "Unexpected/unhandled input system value");
          break;
        }
        prelogue_.push_back([=](IREffect &prelogue) {
          prelogue << init_input_reg(assigned_index, reg, mask);
        });
        max_input_register = std::max(reg + 1, max_input_register);
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT_PS: {
        unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
        auto interpolation =
          to_air_interpolation(Inst.m_InputPSDecl.InterpolationMode);
        auto sig = findInputElement([=](Signature sig) {
          return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
        });
        auto name = sig.fullSemanticString();
        auto assigned_index = func_signature.DefineInput(InputFragmentStageIn{
          .user = name,
          .type = to_msl_type(sig.componentType()),
          .interpolation = interpolation
        });
        prelogue_.push_back([=](IREffect &prelogue) {
          prelogue << init_input_reg(assigned_index, reg, mask);
        });
        max_input_register = std::max(reg + 1, max_input_register);
        break;
      }
      case D3D10_SB_OPCODE_DCL_OUTPUT_SGV: {
        assert(0 && "dcl_output_sgv should not happen for now");
        // only GS PrimitiveID uses this, but we don't support GS for now
        break;
      }
      case D3D10_SB_OPCODE_DCL_OUTPUT_SIV: {
        unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
        auto siv = Inst.m_OutputDeclSIV.Name;
        switch (siv) {
        case D3D10_SB_NAME_CLIP_DISTANCE: {
          assert(0 && "Should be handled separately"); // becuase it can defined
                                                       // multiple times
          break;
        }
        case D3D10_SB_NAME_CULL_DISTANCE:
          assert(0 && "Metal doesn't support shader output: cull distance");
          break;
        case D3D10_SB_NAME_POSITION: {
          auto assigned_index =
            func_signature.DefineOutput(OutputPosition{.type = msl_float4});
          epilogue_.push_back([=](IRValue &epilogue) {
            epilogue >> pop_output_reg(reg, mask, assigned_index);
          });
          break;
        }
        case D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX:
        case D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX:
        default:
          assert(0 && "Unexpected/unhandled input system value");
          break;
        }
        max_output_register = std::max(reg + 1, max_output_register);
        break;
      }
      case D3D10_SB_OPCODE_DCL_OUTPUT: {
        D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;
        switch (RegType) {
        case D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH:
        case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
        case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL: {
          prelogue_.push_back([=](IREffect &prelogue) {
            prelogue << make_effect([](struct context ctx) -> std::monostate {
              assert(
                ctx.resource.depth_output_reg == nullptr &&
                "otherwise oDepth is defined twice"
              );
              ctx.resource.depth_output_reg =
                ctx.builder.CreateAlloca(ctx.types._float);
              return {};
            });
          });
          auto assigned_index = func_signature.DefineOutput(OutputDepth{
            .depth_argument =
              RegType == D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL
                ? DepthArgument::greater
              : RegType == D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL
                ? DepthArgument::less
                : DepthArgument::any
          });
          epilogue_.push_back([=](IRValue &epilogue) {
            epilogue >> [=](pvalue v) {
              return make_irvalue([=](struct context ctx) {
                return ctx.builder.CreateInsertValue(
                  v,
                  ctx.builder.CreateLoad(
                    ctx.types._float,
                    ctx.builder.CreateConstInBoundsGEP1_32(
                      ctx.types._float, ctx.resource.depth_output_reg, 0
                    )
                  ),
                  {assigned_index}
                );
              });
            };
          });
          break;
        }
        case D3D11_SB_OPERAND_TYPE_OUTPUT_STENCIL_REF:  {
          assert(0 && "todo");
          break;
        }
        case D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK: {
          prelogue_.push_back([=](IREffect &prelogue) {
            prelogue << make_effect([](struct context ctx) -> std::monostate {
              assert(
                ctx.resource.coverage_mask_reg == nullptr &&
                "otherwise oMask is defined twice"
              );
              ctx.resource.coverage_mask_reg =
                ctx.builder.CreateAlloca(ctx.types._int);
              return {};
            });
          });
          auto assigned_index = func_signature.DefineOutput(OutputCoverageMask {});
          epilogue_.push_back([=](IRValue &epilogue) {
            epilogue >> [=](pvalue v) {
              return make_irvalue([=](struct context ctx) {
                auto odepth = ctx.builder.CreateLoad(
                    ctx.types._int,
                    ctx.builder.CreateConstInBoundsGEP1_32(
                      ctx.types._int, ctx.resource.coverage_mask_reg, 0
                    )
                  );
                return ctx.builder.CreateInsertValue(
                  v,
                  ctx.pso_sample_mask != 0xffffffff ? 
                    ctx.builder.CreateAnd(odepth, ctx.pso_sample_mask)
                    : odepth,
                  {assigned_index}
                );
              });
            };
          });
          break;
        }

        default: {
          // normal output register
          auto reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
          auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
          auto sig = findOutputElement([=](Signature sig) {
            return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
          });
          uint32_t assigned_index;
          if (sm50_shader->shader_type == D3D10_SB_PIXEL_SHADER) {
            assigned_index = func_signature.DefineOutput(OutputRenderTarget{
              .index = reg,
              .type = to_msl_type(sig.componentType()),
            });
            epilogue_.push_back([=](IRValue &epilogue) {
              epilogue >> pop_output_reg(reg, mask, assigned_index);
            });
          } else {
            assigned_index = func_signature.DefineOutput(OutputVertex{
              .user = sig.fullSemanticString(),
              .type = to_msl_type(sig.componentType()),
            });
            epilogue_.push_back([=](IRValue &epilogue) {
              epilogue >> pop_output_reg(reg, mask, assigned_index);
            });
          }
          max_output_register = std::max(reg + 1, max_output_register);
          break;
        }
        }
        break;
      }
      case D3D10_SB_OPCODE_CUSTOMDATA: {
        if (Inst.m_CustomData.Type ==
            D3D10_SB_CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER) {
          // must be list of 4-tuples
          unsigned size_in_vec4 = Inst.m_CustomData.DataSizeInBytes >> 4;
          DXASSERT_DXBC(Inst.m_CustomData.DataSizeInBytes == size_in_vec4 * 16);
          shader_info->immConstantBufferData.assign(
            (std::array<uint32_t, 4> *)Inst.m_CustomData.pData,
            ((std::array<uint32_t, 4> *)Inst.m_CustomData.pData) + size_in_vec4
          );
        }
        break;
      }
      case D3D10_SB_OPCODE_DCL_INDEX_RANGE:
        break; // ignore, and it turns out backend compiler can handle alloca
      case D3D10_SB_OPCODE_DCL_GS_INPUT_PRIMITIVE:
      case D3D10_SB_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:
      case D3D10_SB_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
      case D3D11_SB_OPCODE_DCL_GS_INSTANCE_COUNT:
      case D3D11_SB_OPCODE_DCL_STREAM:
      case D3D11_SB_OPCODE_DCL_INTERFACE:
      case D3D11_SB_OPCODE_DCL_FUNCTION_TABLE:
      case D3D11_SB_OPCODE_DCL_FUNCTION_BODY:
      case D3D10_SB_OPCODE_LABEL:
      case D3D11_SB_OPCODE_DCL_TESS_PARTITIONING:
      case D3D11_SB_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE:
      case D3D11_SB_OPCODE_DCL_TESS_DOMAIN:
      case D3D11_SB_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT:
      case D3D11_SB_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT:
      case D3D11_SB_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
      case D3D11_SB_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:
      case D3D11_SB_OPCODE_DCL_HS_MAX_TESSFACTOR: {
        // ignore atm
        break;
      }
#pragma endregion
      default: {
        // insert instruction into BasicBlock
        ctx->instructions.push_back(readInstruction(Inst, *shader_info));
        break;
      }
      }
    }
    assert(0 && "Unexpected end of shader instructions.");
  };

  auto entry = std::make_shared<BasicBlock>("entrybb");
  auto return_point = std::make_shared<BasicBlock>("returnbb");
  return_point->target = BasicBlockReturn{};
  auto _ = readControlFlow(
    entry, null_bb, null_bb, null_bb, return_point, null_switch_context
  );
  assert(_.get() == return_point.get());

  sm50_shader->entry = entry;

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
    sampler.arg_index = binding_table.DefineSampler(
      "s" + std::to_string(range_id),
      GetArgumentIndex(SM50BindingType::Sampler, range_id)
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
    if (srv.resource_type != ResourceType::NonApplicable) {
      // TODO: abstract SM 5.0 binding
      auto access = srv.sampled ? MemoryAccess::sample : MemoryAccess::read;
      auto texture_kind = to_air_resource_type(srv.resource_type, srv.compared);
      auto scaler_type = to_air_scaler_type(srv.scaler_type);
      srv.arg_index = binding_table.DefineTexture(
        "t" + std::to_string(range_id), texture_kind, access, scaler_type,
        GetArgumentIndex(SM50BindingType::SRV, range_id)
      );
    } else {
      auto attr_index = GetArgumentIndex(SM50BindingType::SRV, range_id);
      srv.arg_index = binding_table.DefineBuffer(
        "t" + std::to_string(range_id),
        AddressSpace::device, // it needs to be `device` since `constant` has
                              // size and alignment restriction
        MemoryAccess::read, msl_uint, attr_index
      );
      srv.arg_size_index = binding_table.DefineInteger32(
        "ct" + std::to_string(range_id), attr_index + 1
      );
    }
    MTL_SM50_SHADER_ARGUMENT_FLAG flags = (MTL_SM50_SHADER_ARGUMENT_FLAG)0;
    if (srv.resource_type == ResourceType::NonApplicable) {
      flags |= MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH |
               MTL_SM50_SHADER_ARGUMENT_BUFFER;
    } else {
      flags |= MTL_SM50_SHADER_ARGUMENT_TEXTURE;
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
      binding_srv_hi_mask |= (1 << (range_id >> 6));
    } else {
      binding_srv_lo_mask |= (1 << range_id);
    }
  }
  for (auto &[range_id, uav] : shader_info->uavMap) {
    auto access =
      uav.written ? (uav.read ? MemoryAccess::read_write : MemoryAccess::write)
                  : MemoryAccess::read;
    if (uav.resource_type != ResourceType::NonApplicable) {
      auto texture_kind = to_air_resource_type(uav.resource_type);
      auto scaler_type = to_air_scaler_type(uav.scaler_type);
      uav.arg_index = binding_table.DefineTexture(
        "u" + std::to_string(range_id), texture_kind, access, scaler_type,
        GetArgumentIndex(SM50BindingType::UAV, range_id),
        uav.rasterizer_order ? std::optional(1) : std::nullopt
      );
    } else {
      auto attr_index = GetArgumentIndex(SM50BindingType::UAV, range_id);
      uav.arg_index = binding_table.DefineBuffer(
        "u" + std::to_string(range_id), AddressSpace::device, access, msl_uint,
        attr_index, uav.rasterizer_order ? std::optional(1) : std::nullopt
      );
      uav.arg_size_index = binding_table.DefineInteger32(
        "cu" + std::to_string(range_id), attr_index + 1
      );
      if (uav.with_counter) {
        uav.arg_counter_index = binding_table.DefineBuffer(
          "counter" + std::to_string(range_id), AddressSpace::device,
          MemoryAccess::read_write, msl_uint, attr_index + 2,
          uav.rasterizer_order ? std::optional(1) : std::nullopt
        );
      }
    }
    MTL_SM50_SHADER_ARGUMENT_FLAG flags = (MTL_SM50_SHADER_ARGUMENT_FLAG)0;
    if (uav.resource_type == ResourceType::NonApplicable) {
      flags |= MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH |
               MTL_SM50_SHADER_ARGUMENT_BUFFER;
    } else {
      flags |= MTL_SM50_SHADER_ARGUMENT_TEXTURE;
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
    binding_uav_mask |= (1 << range_id);
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
    pRefl->ConstantBuffers = sm50_shader->args_reflection_cbuffer.data();
    pRefl->NumArguments = sm50_shader->args_reflection.size();
    pRefl->Arguments = sm50_shader->args_reflection.data();
    if (sm50_shader->shader_type == microsoft::D3D11_SB_COMPUTE_SHADER) {
      pRefl->ThreadgroupSize[0] = sm50_shader->threadgroup_size[0];
      pRefl->ThreadgroupSize[1] = sm50_shader->threadgroup_size[1];
      pRefl->ThreadgroupSize[2] = sm50_shader->threadgroup_size[2];
    }
  }

  *ppShader = (SM50Shader *)sm50_shader;
  return 0;
};

void SM50Destroy(SM50Shader *pShader) { delete (SM50ShaderInternal *)pShader; }

ABRT_HANDLE_INIT

int SM50Compile(
  SM50Shader *pShader, SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs,
  const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
) {
  ABRT_HANDLE_RETURN(42)

  using namespace llvm;
  using namespace dxmt;

  if (ppError) {
    *ppError = nullptr;
  }
  auto errorObj = new SM50ErrorInternal();
  llvm::raw_svector_ostream errorOut(errorObj->buf);
  if (ppBitcode == nullptr) {
    errorOut << "ppBitcode can not be null\0";
    *ppError = (SM50Error *)errorObj;
    return 1;
  }

  // pArgs is ignored for now
  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  auto &shader_info = ((SM50ShaderInternal *)pShader)->shader_info;
  auto shader_type = ((SM50ShaderInternal *)pShader)->shader_type;

  auto pModule = std::make_unique<Module>("shader.air", context);
  initializeModule(
    *pModule,
    {.enableFastMath =
       (!shader_info.skipOptimization && shader_info.refactoringAllowed &&
        // this is by design: vertex functions are usually not the
        // bottle-neck of pipeline, and precise calculation on pixel can reduce
        // flickering
        shader_type != microsoft::D3D10_SB_VERTEX_SHADER)}
  );

  if (auto err = dxmt::dxbc::convertDXBC(
        pShader, FunctionName, context, *pModule, pArgs
      )) {
    llvm::handleAllErrors(std::move(err), [&](const UnsupportedFeature &u) {
      errorOut << u.msg;
    });
    *ppError = (SM50Error *)errorObj;
    return 1;
  }

  if (!shader_info.skipOptimization) {
    runOptimizationPasses(*pModule, OptimizationLevel::O2);
  }

  // pModule->print(outs(), nullptr);

  // Serialize AIR
  auto compiled = new SM50CompiledBitcodeInternal();

  raw_svector_ostream OS(compiled->vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  pModule.reset();

  *ppBitcode = (SM50CompiledBitcode *)compiled;
  return 0;
}

void SM50GetCompiledBitcode(
  SM50CompiledBitcode *pBitcode, MTL_SHADER_BITCODE *pData
) {
  auto pBitcodeInternal = (SM50CompiledBitcodeInternal *)pBitcode;
  pData->Data = pBitcodeInternal->vec.data();
  pData->Size = pBitcodeInternal->vec.size();
}

void SM50DestroyBitcode(SM50CompiledBitcode *pBitcode) {
  auto pBitcodeInternal = (SM50CompiledBitcodeInternal *)pBitcode;
  delete pBitcodeInternal;
}

const char *SM50GetErrorMesssage(SM50Error *pError) {
  auto pInternal = (SM50ErrorInternal *)pError;
  if (*pInternal->buf.end() != '\0') {
    // ensure it returns a null terminated str
    pInternal->buf.push_back('\0');
  }
  return pInternal->buf.data();
}

void SM50FreeError(SM50Error *pError) {
  if (pError == nullptr)
    return;
  auto pInternal = (SM50ErrorInternal *)pError;
  delete pInternal;
}
