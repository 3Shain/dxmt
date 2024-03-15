#include "dxbc_analyzer.hpp"
#include "DXBCParser/DXBCUtils.h"
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Metadata.h"
#include <cassert>
#include <tuple>
#include <vector>

#include "dxbc_signature.hpp"
#include "dxbc_utils.h" // TODO: get rid of this. it's confusing

#include "dxbc_instruction.hpp"

using namespace llvm;
using namespace dxmt::air;

namespace dxmt::dxbc {

void DxbcAnalyzer::AnalyzeShader(D3D10ShaderBinary::CShaderCodeParser &Parser,
                                 const SignatureFinder &findInputSignature,
                                 const SignatureFinder &findOutputSignature) {

  ShaderType = Parser.ShaderType();
  m_DxbcMajor = Parser.ShaderMajorVersion();
  m_DxbcMinor = Parser.ShaderMinorVersion();

  D3D10ShaderBinary::CInstruction Inst;
  while (!Parser.EndOfShader()) {

    Parser.ParseInstruction(&Inst);
    dxbc::Instruciton inst(Inst);

    switch (Inst.OpCode()) {
      // why so fucking long enum name, it hurts my eyes.
      // because it's C enum. I miss my namespace.
    case D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER: {
      bindingDeclare->DeclareConstantBuffer(inst.dclBindingRegister(),
                                            inst.dclConstSize());
      break;
    }
    case D3D10_SB_OPCODE_DCL_SAMPLER: {
      bindingDeclare->DeclareSampler(inst.dclBindingRegister());
      break;
    }

    case D3D10_SB_OPCODE_DCL_RESOURCE: {
      bindingDeclare->DeclareTexture(inst.dclBindingRegister(),
                                     inst.dclResourceType(),
                                     inst.dclResourceAccessType(), false);
      break;
    }
    case D3D11_SB_OPCODE_DCL_RESOURCE_RAW: {
      bindingDeclare->DeclareBuffer(inst.dclBindingRegister(), true, false);
      break;
    }
    case D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED: {
      bindingDeclare->DeclareBuffer(inst.dclBindingRegister(), true, false);
      // and read stride here!
      break;
    }

    case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED: {
      bindingDeclare->DeclareTexture(inst.dclBindingRegister(),
                                     inst.dclResourceType(),
                                     inst.dclResourceAccessType(), true);
      break;
    }
    case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW: {
      bindingDeclare->DeclareBuffer(inst.dclBindingRegister(), true, false);
      break;
    }
    case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
      bindingDeclare->DeclareBuffer(inst.dclBindingRegister(), true, false);
      // and read stride here!
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT: {
      D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;

      switch (RegType) {
      case D3D11_SB_OPERAND_TYPE_INPUT_COVERAGE_MASK:
        assert(0 && "TODO: fragment: sample_mask");
        break;

      case D3D11_SB_OPERAND_TYPE_INNER_COVERAGE:
        assert(0 && "D3D11_SB_OPERAND_TYPE_INNER_COVERAGE");
        break;

      case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID:
      case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_GROUP_ID:
      case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP:
      case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
        assert(0 && "TODO: kernel function attributes");
        break;
      case D3D11_SB_OPERAND_TYPE_INPUT_DOMAIN_POINT:
      case D3D11_SB_OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID:
      case D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID:
      case D3D11_SB_OPERAND_TYPE_INPUT_FORK_INSTANCE_ID:
      case D3D11_SB_OPERAND_TYPE_INPUT_JOIN_INSTANCE_ID:
      case D3D11_SB_OPERAND_TYPE_INPUT_GS_INSTANCE_ID:
        assert(0);
        break;
      case D3D11_SB_OPERAND_TYPE_CYCLE_COUNTER:
        assert(0);
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
          DXASSERT(false, "there should no other index dimensions");
        }

        if (RegType == D3D10_SB_OPERAND_TYPE_INPUT) {
          auto mask = inst.dclBindingMask();
          auto sig = findInputSignature([=](dxbc::Signature sig) {
            return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
          });
          auto name = sig.fullSemanticString();
          //
          inputs.push_back(
              {types._float4,
               metadata.createUserVertexInput(inputs.size(), reg,
                                              air::EType::float4, name),
               name, [=](auto value, auto context, AirBuilder &builder) {
                 builder.CreateGenericRegisterStore(
                     context.inputRegs, context.tyInputRegs, reg, value, mask);
               }});
        } else {
          assert(0 && "Unknown input register type");
        }
        maxInputRegister = std::max(reg + 1, maxInputRegister);
        break;
      }
      }
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_SGV: {
      unsigned reg = inst.dclBindingRegister();
      auto mask = inst.dclBindingMask();
      auto sgv = Inst.m_InputDeclSGV.Name;
      PrologueHook prologue = [=](auto value, auto context,
                                  AirBuilder &builder) {
        builder.CreateGenericRegisterStore(
            context.inputRegs, context.tyInputRegs, reg, value, mask);
      };
      switch (sgv) {
      case D3D10_SB_NAME_VERTEX_ID: {
        auto name = "vertexId";
        inputs.push_back({
            types._int, // REFACTOR: air::EInput infer? (not sure it's
                        // consistent)
            metadata.createInput(inputs.size(), air::EInput::vertex_id, name),
            name,
            prologue,
        });
        break;
      }
      case D3D10_SB_NAME_PRIMITIVE_ID: {
        assert(IsPS() && "PrimitiveID is allowed only in pixel shader");
        auto name = "primitiveId";
        inputs.push_back({
            types._int,
            metadata.createInput(inputs.size(), air::EInput::primitive_id,
                                 name),
            name,
            prologue,
        });
        break;
      }
      case D3D10_SB_NAME_INSTANCE_ID: {
        auto name = "instanceId";
        inputs.push_back({
            types._int,
            metadata.createInput(inputs.size(), air::EInput::instance_id, name),
            name,
            prologue,
        });
        break;
      }
      case D3D10_SB_NAME_IS_FRONT_FACE: {
        auto name = "isFrontFace";
        inputs.push_back({
            types._bool,
            metadata.createInput(inputs.size(), air::EInput::front_facing,
                                 name),
            name,
            prologue,
        });
        break;
      }
      case D3D10_SB_NAME_SAMPLE_INDEX: {
        auto name = "sampleIndex";
        inputs.push_back({
            types._int,
            metadata.createInput(inputs.size(), air::EInput::sample_id, name),
            name,
            prologue,
        });
        break;
      }
      default:
        assert(0 && "Unexpected/unhandled input system value");
        break;
      }
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_SIV: {
      assert(0 && "dcl_input_siv should not happen for now");
      // because we don't support hull/domain/geometry
      // and pixel shader has its own dcl_input_ps
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_PS: {
      auto reg = inst.dclBindingRegister();
      auto mask = inst.dclBindingMask();
      auto sig = findInputSignature([=](dxbc::Signature sig) {
        return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
      });
      auto name = sig.fullSemanticString();
      //
      inputs.push_back(
          {types._float4,
           metadata.createUserFragmentInput(inputs.size(), name,
                                            inst.interpolation(),
                                            air::EType::float4, name),
           name, [=](auto value, auto context, AirBuilder &builder) {
             builder.CreateGenericRegisterStore(
                 context.inputRegs, context.tyInputRegs, reg, value, mask);
           }});
      maxInputRegister = std::max(reg + 1, maxInputRegister);
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_PS_SGV: {
      // auto MinPrecision = Inst.m_Operands[0].m_MinPrecision; // not used
      auto siv = Inst.m_InputPSDeclSGV.Name;
      auto interpolation = Inst.m_InputPSDeclSGV.InterpolationMode;
      switch (siv) {
      case D3D10_SB_NAME_POSITION: {
        assert(
            interpolation ==
            D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE); // the only supported
                                                          // by metal
        break;
      }
      default:
        assert(0 && "Unexpected/unhandled input system value");
        break;
      }
      break;
    }

    case D3D10_SB_OPCODE_DCL_INPUT_PS_SIV: {
      // auto MinPrecision = Inst.m_Operands[0].m_MinPrecision; // not used
      auto siv = Inst.m_InputPSDeclSIV.Name;
      switch (siv) {
      case D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX:
        break;
      case D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX:
        break;
      default:
        assert(0 && "Unexpected/unhandled input system value");
        break;
      }
      break;
    }

    case D3D10_SB_OPCODE_DCL_OUTPUT: {
      D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;
      switch (RegType) {
      case D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH:
      case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
      case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL: {
        break;
      }
      case D3D11_SB_OPERAND_TYPE_OUTPUT_STENCIL_REF:
      case D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK: {
        break;
      }

      default: {
        // normal output register
        auto reg = inst.dclBindingRegister();
        auto mask = inst.dclBindingMask();
        auto sig = findOutputSignature([=](dxbc::Signature sig) {
          return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
        });
        auto name = sig.fullSemanticString();

        if (IsPS()) {
          outputs.push_back({sig.componentType() == RegisterComponentType::Float
                                 ? types._float4
                                 : types._int4,
                             metadata.createRenderTargetOutput(
                                 reg /* TODO: dual source blending */, 0,
                                 air::EType::float4, name),
                             name, [=](auto context, AirBuilder &builder) {
                               return builder.CreateGenericRegisterLoad(
                                   context.outputRegs, context.tyOutputRegs,
                                   reg, types._float4,
                                   builder.MaskToSwizzle(mask));
                             }});
        } else {
          outputs.push_back(
              {types._float4,
               metadata.createUserVertexOutput(name, air::EType::float4, name),
               name, [=](auto context, AirBuilder &builder) {
                 return builder.CreateGenericRegisterLoad(
                     context.outputRegs, context.tyOutputRegs, reg,
                     types._float4, builder.MaskToSwizzle(mask));
               }});
        }
        maxOutputRegister = std::max(reg + 1, maxOutputRegister);
        break;
      }
      }
      break;
    }

    case D3D10_SB_OPCODE_DCL_OUTPUT_SGV: {
      assert(0 && "dcl_output_sgv should not happen for now");
      // only GS PrimitiveID uses this, but we don't support GS for now
      break;
    }
    case D3D10_SB_OPCODE_DCL_OUTPUT_SIV: {
      auto reg = inst.dclBindingRegister();
      auto mask = inst.dclBindingMask();
      // auto MinPrecision = Inst.m_Operands[0].m_MinPrecision; // not used
      auto siv = Inst.m_OutputDeclSIV.Name;
      EpilogueHook epilogue = [=](auto a, AirBuilder &builder) {
        return builder.CreateGenericRegisterLoad(a.outputRegs, a.tyOutputRegs,
                                                 reg, types._int,
                                                 builder.MaskToSwizzle(mask));
      };
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
        auto name = "position";
        outputs.push_back({types._float4,
                           metadata.createOutput(air::EOutput::position, name),
                           name, [=](auto a, AirBuilder &builder) {
                             return builder.CreateGenericRegisterLoad(
                                 a.outputRegs, a.tyOutputRegs, reg,
                                 types._float4, builder.MaskToSwizzle(mask));
                           }});
        break;
      }
      case D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX: {
        auto name = "renderTargetArrayIndex";
        outputs.push_back({types._int,
                           metadata.createOutput(
                               air::EOutput::render_target_array_index, name),
                           name, epilogue});
        break;
      }
      case D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX: {
        auto name = "viewportArrayIndex";
        outputs.push_back({types._int,
                           metadata.createOutput(
                               air::EOutput::render_target_array_index, name),
                           name, epilogue});
        break;
      }
      default:
        assert(0 && "Unexpected/unhandled input system value");
        break;
      }
      maxOutputRegister = std::max(reg + 1, maxOutputRegister);
      break;
    }

    case D3D10_SB_OPCODE_DCL_TEMPS: {
      tempsCount = Inst.m_TempsDecl.NumTemps;
      break;
    }

    case D3D10_SB_OPCODE_DCL_INDEXABLE_TEMP: {
      auto index = inst.dclBindingRegister();
      auto size = inst.dclConstSize();
      indexableTemps.insert_or_assign(index, size);
      break;
    }

    case D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS: {
      auto flag = inst.dclFlags();
      if (flag & D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED) {
        enableFastMath = true;
      }
      break;
    }

    case D3D11_SB_OPCODE_DCL_THREAD_GROUP: {
      threadgroupSize = {Inst.m_ThreadGroupDecl.x, Inst.m_ThreadGroupDecl.y,
                         Inst.m_ThreadGroupDecl.z};
      break;
    }

    case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
    case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
      DXASSERT(false, "Unhandled declaration (tbd)");
      break;
    }

    case D3D11_SB_OPCODE_GATHER4_C:
    case D3D11_SB_OPCODE_GATHER4_PO_C:
    case D3D10_SB_OPCODE_SAMPLE_C:
    case D3D10_SB_OPCODE_SAMPLE_C_LZ: {
      auto Op = Inst.m_Operands[2];
      DXASSERT_DXBC(Op.OperandType() == D3D10_SB_OPERAND_TYPE_RESOURCE);
      // Op.OperandIndexDimension()
      assert(0);
      break;
    }

    case D3D10_SB_OPCODE_DCL_GS_INPUT_PRIMITIVE:
    case D3D10_SB_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:
    case D3D10_SB_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
    case D3D11_SB_OPCODE_HS_DECLS:
    case D3D11_SB_OPCODE_DCL_STREAM:
    case D3D11_SB_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT:
    case D3D11_SB_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT:
    case D3D11_SB_OPCODE_DCL_TESS_DOMAIN:
    case D3D11_SB_OPCODE_DCL_TESS_PARTITIONING:
    case D3D11_SB_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE:
    case D3D11_SB_OPCODE_DCL_HS_MAX_TESSFACTOR:
    case D3D11_SB_OPCODE_HS_CONTROL_POINT_PHASE:
    case D3D11_SB_OPCODE_HS_FORK_PHASE:
    case D3D11_SB_OPCODE_HS_JOIN_PHASE:
    case D3D11_SB_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT:
    case D3D11_SB_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:
    case D3D11_SB_OPCODE_DCL_GS_INSTANCE_COUNT: {
      DXASSERT(false, "Unsupported declarations");
      break;
    }

    default: // ignore because not interested.
      break;
    };
  }
}

std::tuple<llvm::Type *, llvm::MDNode *> DxbcAnalyzer::GenerateBindingTable() {
  // if no content in generate binding
  if (constantBuffers.size() < 1) {
    return std::make_tuple<llvm::Type *, llvm::MDNode *>(nullptr, nullptr);
  }
  std::vector<Type *> fieldTypes;
  std::vector<Metadata *> structTypeInfo;

  for (auto &cb : constantBuffers) {
    auto cbName = Twine("cb") + Twine(cb.registerIndex);
    structTypeInfo.insert(
        structTypeInfo.end(),
        {metadata.createUnsignedInteger(0), // TODO: increment
         metadata.createUnsignedInteger(8), // sizeof(ptr)
         metadata.createUnsignedInteger(0), // not array : 0
         metadata.createString("uint4"), metadata.createString(cbName),
         metadata.createString("air.indirect_argument"),
         metadata.createBufferBinding(fieldTypes.size(), cb.registerIndex, 1,
                                      air::EBufferAccess::read,
                                      air::EBufferAddrSpace::constant,
                                      air::EType::uint4, cbName.str())});
    // fieldTypes.push_back(types.tyConstantPtr);
    fieldTypes.push_back(PointerType::get(
        types._int4,
        2)); // test above! I need to make sure metal backend uses opaque ptr
  }

  return std::make_tuple(
      StructType::create(context, fieldTypes, "BindingTable", false),
      MDTuple::get(context, structTypeInfo));
}

} // namespace dxmt::dxbc