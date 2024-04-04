#include "dxbc_converter.hpp"
#include "DXBCParser/DXBCUtils.h"
#include "DXBCParser/ShaderBinary.h"
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "air_signature.hpp"
#include "air_type.hpp"
#include "dxbc_constants.hpp"
#include "dxbc_signature.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/FMF.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include <bit>
#include <string>

/* separated implementation details */
#include "dxbc_converter_inc.hpp"
#include "dxbc_converter_instruction_inc.hpp"
#include "shader_common.hpp"

namespace dxmt::dxbc {

Reflection convertDXBC(
  const void *dxbc, uint32_t dxbcSize, llvm::LLVMContext &context,
  llvm::Module &module
) {
  using namespace microsoft;
  CDXBCParser DXBCParser;
  assert((DXBCParser.ReadDXBC(dxbc, dxbcSize) == S_OK) && "invalid dxbc blob");

  UINT codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShaderEx);
  if (codeBlobIdx == DXBC_BLOB_NOT_FOUND) {
    codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShader);
  }
  assert((codeBlobIdx != DXBC_BLOB_NOT_FOUND) && "invalid dxbc blob");
  LPCVOID codeBlob = DXBCParser.GetBlob(codeBlobIdx);

  CShaderToken *ShaderCode = (CShaderToken *)(BYTE *)codeBlob;
  // 1. Collect information about the shader.
  D3D10ShaderBinary::CShaderCodeParser CodeParser(ShaderCode);
  CSignatureParser inputParser;
  // TODO: throw if failed
  DXBCGetInputSignature(dxbc, &inputParser);
  CSignatureParser outputParser;
  DXBCGetOutputSignature(dxbc, &outputParser);

  auto findInputElement = [&](auto matcher) -> dxbc::Signature {
    const D3D11_SIGNATURE_PARAMETER *parameters;
    inputParser.GetParameters(&parameters);
    for (unsigned i = 0; i < inputParser.GetNumParameters(); i++) {
      auto sig = dxbc::Signature(parameters[i]);
      if (matcher(sig)) {
        return sig;
      }
    }
    assert(inputParser.GetNumParameters());
    assert(0 && "try to access an undefined input");
  };
  auto findOutputElement = [&](auto matcher) -> dxbc::Signature {
    const D3D11_SIGNATURE_PARAMETER *parameters;
    outputParser.GetParameters(&parameters);
    for (unsigned i = 0; i < outputParser.GetNumParameters(); i++) {
      auto sig = dxbc::Signature(parameters[i]);
      if (matcher(sig)) {
        return sig;
      }
    }
    assert(0 && "try to access an undefined output");
  };

  bool sm_ver_5_1_ = CodeParser.ShaderMajorVersion() == 5 &&
                     CodeParser.ShaderMinorVersion() >= 1;
  auto shader_type = CodeParser.ShaderType();

  air::ArgumentBufferBuilder binding_table;
  air::FunctionSignatureBuilder func_signature;
  uint32_t binding_table_index = 0;
  uint32_t max_input_register = 0;
  uint32_t max_output_register = 0;

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

  auto shader_info = std::make_shared<ShaderInfo>();
  IREffect prelogue([](auto) { return std::monostate(); });
  IRValue epilogue([](struct context ctx) {
    auto retTy = ctx.function->getReturnType();
    if (retTy->isVoidTy()) {
      return (pvalue) nullptr;
    }
    return (pvalue)llvm::UndefValue::get(retTy);
  });
  io_binding_map resource_map;

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
        auto true_ = std::make_shared<BasicBlock>();
        auto alternative_ = std::make_shared<BasicBlock>();
        // alternative_ might be the block after ENDIF, but ELSE is possible
        ctx->target = BasicBlockConditionalBranch{
          readCondition(Inst, 0), true_, alternative_
        };
        auto after_endif = readControlFlow(
          ctx, alternative_, continue_point, break_point, return_point,
          null_switch_context
        ); // read till ENDIF
        // scope end
        return readControlFlow(
          after_endif, null_bb, continue_point, break_point, return_point,
          switch_context
        );
      }
      case D3D10_SB_OPCODE_ELSE: {
        assert(block_after_endif.get() && "");
        auto real_exit = std::make_shared<BasicBlock>();
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
        auto loop_entrance = std::make_shared<BasicBlock>();
        auto after_endloop = std::make_shared<BasicBlock>();
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
        auto after_break = std::make_shared<BasicBlock>();
        return readControlFlow(
          after_break, block_after_endif, continue_point, break_point,
          return_point, switch_context
        ); // ?
      }
      case D3D10_SB_OPCODE_BREAKC: {
        auto after_break = std::make_shared<BasicBlock>();
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
        auto after_continue = std::make_shared<BasicBlock>();
        return readControlFlow(
          after_continue, block_after_endif, continue_point, break_point,
          return_point,
          switch_context
        ); // ?
      }
      case D3D10_SB_OPCODE_CONTINUEC: {
        auto after_continue = std::make_shared<BasicBlock>();
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
        auto after_endswitch = std::make_shared<BasicBlock>();
        // scope start: switch
        auto local_switch_context = std::make_shared<BasicBlockSwitch>();
        auto empty_body =
          std::make_shared<BasicBlock>(); // it will unconditional jump to
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
        auto case_body = std::make_shared<BasicBlock>();
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
        auto case_body = std::make_shared<BasicBlock>();
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
        auto after_ret = std::make_shared<BasicBlock>();
        // if it's inside a scope, then return is not the end
        return readControlFlow(
          after_ret, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_RETC: {
        auto after_retc = std::make_shared<BasicBlock>();
        ctx->target = BasicBlockConditionalBranch{
          readCondition(Inst, 0), return_point, after_retc
        };
        return readControlFlow(
          after_retc, block_after_endif, continue_point, break_point,
          return_point, switch_context
        );
      }
      case D3D10_SB_OPCODE_DISCARD: {
        auto fulfilled_ = std::make_shared<BasicBlock>();
        auto otherwise_ = std::make_shared<BasicBlock>();
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
          .size_in_vec4 = CBufferSize
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
        ShaderResourceViewInfo srv{
          .range =
            {
              .range_id = RangeID,
              .lower_bound = LB,
              .size = RangeSize,
            }
        };
        switch (Inst.OpCode()) {
        case D3D10_SB_OPCODE_DCL_RESOURCE: {
          srv.range.space = (Inst.m_ResourceDecl.Space);
          srv.resource_type =
            to_shader_resource_type(Inst.m_ResourceDecl.Dimension);
          srv.scaler_type =
            to_shader_scaler_type(Inst.m_ResourceDecl.ReturnType[0]);
          // Inst.m_ResourceDecl.SampleCount
          break;
        }
        case D3D11_SB_OPCODE_DCL_RESOURCE_RAW: {
          srv.range.space = (Inst.m_RawSRVDecl.Space);
          srv.scaler_type = shader::common::ScalerDataType::Uint;
          break;
        }
        case D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED: {
          srv.range.space = (Inst.m_StructuredSRVDecl.Space);
          srv.scaler_type = shader::common::ScalerDataType::Uint;
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

        UnorderedAccessViewInfo uav{
          .range =
            {
              .range_id = RangeID,
              .lower_bound = LB,
              .size = RangeSize,
            }
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
          break;
        }
        case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW: {
          uav.range.space = (Inst.m_RawUAVDecl.Space);
          // R.SetKind(DxilResource::Kind::RawBuffer);
          Flags = Inst.m_RawUAVDecl.Flags;
          uav.scaler_type = shader::common::ScalerDataType::Uint;
          break;
        }
        case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
          uav.range.space = (Inst.m_StructuredUAVDecl.Space);
          // R.SetKind(DxilResource::Kind::StructuredBuffer);
          Flags = Inst.m_StructuredUAVDecl.Flags;
          uav.scaler_type = shader::common::ScalerDataType::Uint;
          uav.strucure_stride = Inst.m_StructuredUAVDecl.ByteStride;
          break;
        }
        default:;
        }

        uav.global_coherent =
          ((Flags & D3D11_SB_GLOBALLY_COHERENT_ACCESS) != 0);
        uav.with_counter =
          ((Flags & D3D11_SB_UAV_HAS_ORDER_PRESERVING_COUNTER) != 0);
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
        break;
      }
      case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
      case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
        ThreadgroupBufferInfo tgsm;
        if (Inst.OpCode() == D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW) {
          tgsm.stride = 1;
          tgsm.size = Inst.m_RawTGSMDecl.ByteCount;
          tgsm.size_in_uint = tgsm.size / 4;
          tgsm.structured = false;
        } else {
          tgsm.stride = Inst.m_StructuredTGSMDecl.StructByteStride;
          tgsm.size = Inst.m_StructuredTGSMDecl.StructCount;
          tgsm.size_in_uint = tgsm.stride * tgsm.size / 4;
          tgsm.structured = true;
        }

        shader_info->tgsmMap[Inst.m_Operands[0].m_Index[0].m_RegIndex] = tgsm;
        break;
      }
      case D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS: {
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
          // auto name = "vertexId";
          auto assigned_index =
            func_signature.DefineInput(air::InputVertexID{});
          // and perform side effect here
          prelogue << init_input_reg(assigned_index, reg, mask);
          break;
        }
        case D3D10_SB_NAME_PRIMITIVE_ID:
        case D3D10_SB_NAME_INSTANCE_ID:
        case D3D10_SB_NAME_IS_FRONT_FACE:
        case D3D10_SB_NAME_SAMPLE_INDEX:
        default:
          assert(0 && "Unexpected/unhandled input system value");
          break;
        }
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT: {
        D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;

        switch (RegType) {
        case D3D11_SB_OPERAND_TYPE_INPUT_COVERAGE_MASK:
          assert(0);
          break;

        case D3D11_SB_OPERAND_TYPE_INNER_COVERAGE:
          assert(0);
          break;

        case D3D11_SB_OPERAND_TYPE_CYCLE_COUNTER:
          break; // ignore it atm
        case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID:
        case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_GROUP_ID:
        case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP:
        case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
          break;
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
            auto assigned_index =
              func_signature.DefineInput(air::InputVertexStageIn{
                .attribute = reg,
                .type = sig.componentType() == RegisterComponentType::Float
                          ? air::msl_float4
                          : air::msl_int4,
                .name = sig.fullSemanticString()
              });
            prelogue << init_input_reg(assigned_index, reg, mask);
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
        // auto interpolation =
        //   to_air_interpolation(Inst.m_InputPSDeclSIV.InterpolationMode);
        uint32_t assigned_index;
        switch (siv) {
        case D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX:
          assigned_index =
            func_signature.DefineInput(air::InputRenderTargetArrayIndex{});
          break;
        case D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX:
          assigned_index =
            func_signature.DefineInput(air::InputViewportArrayIndex{});
          break;
        default:
          assert(0 && "Unexpected/unhandled input system value");
          break;
        }
        prelogue << init_input_reg(assigned_index, reg, mask);
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT_PS_SGV: {
        unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
        auto siv = Inst.m_InputPSDeclSGV.Name;
        auto interpolation =
          to_air_interpolation(Inst.m_InputPSDeclSGV.InterpolationMode);
        switch (siv) {
        case D3D10_SB_NAME_POSITION: {
          assert(
            interpolation == air::Interpolation::sample_no_perspective
          ); // the only supported interpolation for [[position]]
          auto assigned_index = func_signature.DefineInput(
            air::InputPosition{.interpolation = interpolation}
          );
          prelogue << init_input_reg(assigned_index, reg, mask);
          break;
        }
        default:
          assert(0 && "Unexpected/unhandled input system value");
          break;
        }
        // prelogue << init_input_reg(assigned_index, reg, mask);
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT_PS: {
        unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
        auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
        auto interpolation =
          to_air_interpolation(Inst.m_InputPSDecl.InterpolationMode);
        auto sig = findInputElement([=](dxbc::Signature sig) {
          return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
        });
        auto name = sig.fullSemanticString();
        auto assigned_index =
          func_signature.DefineInput(air::InputFragmentStageIn{
            .user = name,
            .type = sig.componentType() == RegisterComponentType::Float
                      ? air::msl_float4
                      : air::msl_int4,
            .interpolation = interpolation
          });
        prelogue << init_input_reg(assigned_index, reg, mask);
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
          auto assigned_index = func_signature.DefineOutput(
            air::OutputPosition{.type = air::msl_float4}
          );
          max_output_register = std::max(reg + 1, max_output_register);
          epilogue >> pop_output_reg(reg, mask, assigned_index);
          break;
        }
        case D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX:
        case D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX:
        default:
          assert(0 && "Unexpected/unhandled input system value");
          break;
        }
        break;
      }
      case D3D10_SB_OPCODE_DCL_OUTPUT: {
        D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;
        switch (RegType) {
        case D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH: {
          break;
        }
        case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL: {
          break;
        }
        case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL: {
          break;
        }
        case D3D11_SB_OPERAND_TYPE_OUTPUT_STENCIL_REF:
        case D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK: {
          break;
        }

        default: {
          // normal output register
          auto reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
          auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
          auto sig = findOutputElement([=](dxbc::Signature sig) {
            return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
          });
          uint32_t assigned_index;
          if (shader_type == D3D10_SB_PIXEL_SHADER) {
            assigned_index =
              func_signature.DefineOutput(air::OutputRenderTarget{
                .index = reg,
                .type = sig.componentType() == RegisterComponentType::Float
                          ? air::msl_float4
                          : air::msl_int4
              });
            epilogue >> pop_output_reg(reg, mask, assigned_index);
          } else {
            assigned_index = func_signature.DefineOutput(air::OutputVertex{
              .user = sig.fullSemanticString(),
              .type = sig.componentType() == RegisterComponentType::Float
                        ? air::msl_float4
                        : air::msl_int4,
            });
            epilogue >> pop_output_reg(reg, mask, assigned_index);
          }
          max_output_register = std::max(reg + 1, max_output_register);
          break;
        }
        }
        break;
      }
      case D3D10_SB_OPCODE_CUSTOMDATA: {
        if (Inst.m_CustomData.Type == D3D10_SB_CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER) {
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

  auto entry = std::make_shared<BasicBlock>();
  auto return_point = std::make_shared<BasicBlock>();
  return_point->target = BasicBlockReturn{};
  auto _ = readControlFlow(
    entry, null_bb, null_bb, null_bb, return_point, null_switch_context
  );
  assert(_.get() == return_point.get());

  // post convert
  for (auto &[range_id, cbv] : shader_info->cbufferMap) {
    // TODO: abstract SM 5.0 binding
    auto index = binding_table.DefineBuffer(
      "cb" + std::to_string(range_id), air::AddressSpace::constant,
      air::MemoryAccess::read, air::msl_uint4
    );
    resource_map.cb_range_map[range_id] = [=, &binding_table_index](pvalue) {
      // ignore index in SM 5.0
      return get_item_in_argbuf_binding_table(binding_table_index, index);
    };
  }
  for (auto &[range_id, sampler] : shader_info->samplerMap) {
    // TODO: abstract SM 5.0 binding
    auto index = binding_table.DefineSampler(
      "s" + std::to_string(range_id), range_id + 16
    );
    resource_map.sampler_range_map[range_id] = [=,
                                                &binding_table_index](pvalue) {
      // ignore index in SM 5.0
      return get_item_in_argbuf_binding_table(binding_table_index, index);
    };
  }
  for (auto &[range_id, srv] : shader_info->srvMap) {
    // TODO: abstract SM 5.0 binding
    auto access =
      srv.sampled ? air::MemoryAccess::sample : air::MemoryAccess::read;
    auto texture_kind =
      air::to_air_resource_type(srv.resource_type, srv.compared);
    auto scaler_type = air::to_air_scaler_type(srv.scaler_type);
    auto index = binding_table.DefineTexture(
      "t" + std::to_string(range_id), texture_kind, access, scaler_type,
      range_id + 128
    );
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
  }
  for (auto &[range_id, uav] : shader_info->uavMap) {
    auto texture_kind = air::to_air_resource_type(uav.resource_type);
    auto scaler_type = air::to_air_scaler_type(uav.scaler_type);
    auto access = uav.written ? (uav.read ? air::MemoryAccess::read_write
                                          : air::MemoryAccess::write)
                              : air::MemoryAccess::read;
    auto index = binding_table.DefineTexture(
      "u" + std::to_string(range_id), texture_kind, access, scaler_type
    );
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
    if (uav.with_counter) {
      //
    }
  }
  air::AirType types(context);
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
      module, type, const_data, llvm::GlobalValue::InternalLinkage, nullptr,
      "icb", nullptr, llvm::GlobalValue::NotThreadLocal, 2
    );
    icb->setAlignment(llvm::Align(16));
    resource_map.icb = icb;
  }

  if (!binding_table.empty()) {
    auto [type, metadata] = binding_table.Build(context, module);
    binding_table_index =
      func_signature.DefineInput(air::ArgumentBindingIndirectBuffer{
        .location_index = 30,
        .array_size = 1,
        .memory_access = air::MemoryAccess::read,
        .address_space = air::AddressSpace::constant,
        .struct_type = type,
        .struct_type_info = metadata,
        .arg_name = "binding_table",
      });
  }
  auto [function, function_metadata] =
    func_signature.CreateFunction("shader_main", context, module);

  auto entry_bb = llvm::BasicBlock::Create(context, "entry", function);
  auto epilogue_bb = llvm::BasicBlock::Create(context, "epilogue", function);
  llvm::IRBuilder<> builder(entry_bb);
  builder.getFastMathFlags().setFast(true);
  resource_map.input_register_file =
    builder.CreateAlloca(llvm::ArrayType::get(types._int4, max_input_register));
  resource_map.input_register_file_float = builder.CreateBitCast(
    resource_map.input_register_file,
    llvm::ArrayType::get(types._float4, max_input_register)->getPointerTo()
  );
  resource_map.output_register_file =
    builder.CreateAlloca(llvm::ArrayType::get(types._int4, max_output_register)
    );
  resource_map.output_register_file_float = builder.CreateBitCast(
    resource_map.output_register_file,
    llvm::ArrayType::get(types._float4, max_output_register)->getPointerTo()
  );
  resource_map.temp_register_file = builder.CreateAlloca(
    llvm::ArrayType::get(types._int4, shader_info->tempRegisterCount)
  );
  resource_map.temp_register_file_float = builder.CreateBitCast(
    resource_map.temp_register_file,
    llvm::ArrayType::get(types._float4, shader_info->tempRegisterCount)
      ->getPointerTo()
  );
  for (auto &[idx, info] : shader_info->indexableTempRegisterCounts) {
    auto &[numRegisters, mask] = info;
    auto channel_count = std::bit_width(mask);
    resource_map.indexable_temp_map[idx] =
      builder.CreateAlloca(llvm::ArrayType::get(
        llvm::FixedVectorType::get(types._int, channel_count), numRegisters
      ));
  }

  struct context ctx {
    .builder = builder, .llvm = context, .module = module, .function = function,
    .resource = resource_map, .types = types
  };
  // then we can start build ... real IR code (visit all basicblocks)
  prelogue.build(ctx);
  auto real_entry = convertBasicBlocks(entry, ctx, epilogue_bb);
  builder.CreateBr(real_entry);

  builder.SetInsertPoint(epilogue_bb);
  auto value = epilogue.build(ctx);
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
  return {.has_binding_map = !binding_table.empty()};
};
} // namespace dxmt::dxbc