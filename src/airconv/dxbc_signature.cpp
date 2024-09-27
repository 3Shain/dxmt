#include "dxbc_signature.hpp"
#include "dxbc_converter.hpp"

using namespace microsoft;
using namespace dxmt::air;
using namespace dxmt::shader::common;

namespace dxmt::dxbc {

inline air::Interpolation
to_air_interpolation(microsoft::D3D10_SB_INTERPOLATION_MODE mode) {
  using namespace microsoft;
  switch (mode) {
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE:
    return air::Interpolation::sample_no_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_SAMPLE:
    return air::Interpolation::sample_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID:
    return air::Interpolation::centroid_no_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_CENTROID:
    return air::Interpolation::centroid_perspective;
  case D3D10_SB_INTERPOLATION_CONSTANT:
    return air::Interpolation::flat;
  case D3D10_SB_INTERPOLATION_LINEAR:
    return air::Interpolation::center_perspective;
  case D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE:
    return air::Interpolation::center_no_perspective;
  default:
    assert(0 && "Unexpected D3D10_SB_INTERPOLATION_MODE");
  }
}

void handle_signature_vs(
  CSignatureParser &inputParser, CSignatureParser &outputParser,
  D3D10ShaderBinary::CInstruction &Inst, SM50ShaderInternal *sm50_shader,
  uint32_t phase_unused
) {
  uint32_t &max_input_register = sm50_shader->max_input_register;
  uint32_t &max_output_register = sm50_shader->max_output_register;
  auto &input_prelogue_ = sm50_shader->input_prelogue_;
  auto &epilogue_ = sm50_shader->epilogue_;
  auto &func_signature = sm50_shader->func_signature;

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

  switch (Inst.m_OpCode) {
  case D3D10_SB_OPCODE_DCL_INPUT_SGV: {
    unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
    auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
    // auto MinPrecision = Inst.m_Operands[0].m_MinPrecision; // not used
    auto sgv = Inst.m_InputDeclSGV.Name;
    switch (sgv) {
    case D3D10_SB_NAME_VERTEX_ID: {
      input_prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
        prelogue << make_effect_bind([=](struct context ctx) {
          return store_at_vec4_array_masked(
            ctx.resource.input.ptr_int4, ctx.builder.getInt32(reg),
            ctx.resource.vertex_id, mask
          );
        });
      });
      break;
    }
    case D3D10_SB_NAME_INSTANCE_ID: {
      input_prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
        prelogue << make_effect_bind([=](struct context ctx) {
          return store_at_vec4_array_masked(
            ctx.resource.input.ptr_int4, ctx.builder.getInt32(reg),
            ctx.resource.instance_id, mask
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
    case D3D10_SB_OPERAND_TYPE_INPUT: {
      unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
      assert(Inst.m_Operands[0].m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);

      auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
      auto sig = findInputElement([=](Signature &sig) {
        return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
      });
      input_prelogue_.push_back(
        [=, type = (InputAttributeComponentType)sig.componentType(),
         name = sig.fullSemanticString()](
          IREffect &prelogue, auto func_signature,
          SM50_SHADER_IA_INPUT_LAYOUT_DATA *ia_layout
        ) {
          if (ia_layout) {
            for (unsigned i = 0; i < ia_layout->num_elements; i++) {
              if (ia_layout->elements[i].reg == reg) {
                prelogue << pull_vertex_input(
                  func_signature, reg, mask, ia_layout->elements[i]
                );
                break;
              }
            }
          } else {
            auto assigned_index = func_signature->DefineInput(
              InputVertexStageIn{.attribute = reg, .type = type, .name = name}
            );
            prelogue << init_input_reg(assigned_index, reg, mask);
          }
        }
      );
      max_input_register = std::max(reg + 1, max_input_register);
      break;
    }
    default:
      assert(0 && "unhandled vertex shader input");
      break;
    }
    break;
  }
  case D3D10_SB_OPCODE_DCL_OUTPUT_SIV: {
    unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
    auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
    auto siv = Inst.m_OutputDeclSIV.Name;
    switch (siv) {
    case D3D10_SB_NAME_CLIP_DISTANCE: {
      for (unsigned i = 0; i < 4; i++) {
        if (mask & (1 << i)) {
          sm50_shader->clip_distance_scalars.push_back(
            {.component = (uint8_t)(i), .reg = (uint8_t)reg}
          );
        }
      }
      break;
    }
    case D3D10_SB_NAME_CULL_DISTANCE:
      assert(0 && "Metal doesn't support shader output: cull distance");
      break;
    case D3D10_SB_NAME_POSITION: {
      auto assigned_index =
        func_signature.DefineOutput(OutputPosition{.type = msl_float4});
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_reg(reg, mask, assigned_index);
      });
      break;
    }
    case D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX: {
      auto assigned_index =
        func_signature.DefineOutput(OutputRenderTargetArrayIndex{});
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_reg(reg, mask, assigned_index);
      });
      break;
    }
    case D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX: {
      auto assigned_index =
        func_signature.DefineOutput(OutputViewportArrayIndex{});
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_reg(reg, mask, assigned_index);
      });
      break;
    }
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
    case D3D10_SB_OPERAND_TYPE_OUTPUT: {
      // normal output register
      auto reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
      auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
      auto sig = findOutputElement([=](Signature sig) {
        return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
      });
      uint32_t assigned_index = func_signature.DefineOutput(OutputVertex{
        .user = sig.fullSemanticString(),
        .type = to_msl_type(sig.componentType()),
      });
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_reg(reg, mask, assigned_index);
      });
      max_output_register = std::max(reg + 1, max_output_register);
      break;
    }
    default:
      assert(0 && "unhandled vertex shader output");
      break;
    }
    break;
  }
  default:
    assert(0);
    break;
  }
};

void handle_signature_ps(
  CSignatureParser &inputParser, CSignatureParser &outputParser,
  D3D10ShaderBinary::CInstruction &Inst, SM50ShaderInternal *sm50_shader,
  uint32_t phase_unused
) {
  uint32_t &max_input_register = sm50_shader->max_input_register;
  uint32_t &max_output_register = sm50_shader->max_output_register;
  auto &prelogue_ = sm50_shader->input_prelogue_;
  auto &epilogue_ = sm50_shader->epilogue_;
  auto &func_signature = sm50_shader->func_signature;

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

  switch (Inst.m_OpCode) {
  case D3D10_SB_OPCODE_DCL_INPUT_SIV: {
    assert(0);
    break;
  }
  case D3D10_SB_OPCODE_DCL_INPUT_SGV:
    assert(0);
    break;
  case D3D10_SB_OPCODE_DCL_INPUT: {
    D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;

    switch (RegType) {
    case D3D11_SB_OPERAND_TYPE_INPUT_COVERAGE_MASK: {
      auto assigned_index = func_signature.DefineInput(InputInputCoverage{});
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
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
    default:
      assert(0 && "unimplemented vertex input");
      break;
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
      assigned_index = func_signature.DefineInput(InputViewportArrayIndex{});
      break;
    default:
      assert(0 && "Unexpected/unhandled input system value");
      break;
    }
    prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
      prelogue << init_input_reg(
        assigned_index, reg, mask, siv == D3D10_SB_NAME_POSITION
      );
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
      assigned_index = func_signature.DefineInput(InputPrimitiveID{});
      break;
    default:
      assert(0 && "Unexpected/unhandled input system value");
      break;
    }
    prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
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
    prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
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
    // auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
    auto siv = Inst.m_OutputDeclSIV.Name;
    switch (siv) {
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
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
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
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
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
    case D3D11_SB_OPERAND_TYPE_OUTPUT_STENCIL_REF: {
      assert(0 && "todo");
      break;
    }
    case D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK: {
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
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
      auto assigned_index = func_signature.DefineOutput(OutputCoverageMask{});
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
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
              ctx.pso_sample_mask != 0xffffffff
                ? ctx.builder.CreateAnd(odepth, ctx.pso_sample_mask)
                : odepth,
              {assigned_index}
            );
          });
        };
      });
      break;
    }

    case D3D10_SB_OPERAND_TYPE_OUTPUT: {
      // normal output register
      auto reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
      auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
      auto sig = findOutputElement([=](Signature sig) {
        return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
      });
      auto type = sig.componentType();
      epilogue_.push_back(
        [=](IRValue &epilogue, auto func_signature, bool dual_source_belnding) {
          uint32_t assigned_index;
          if (dual_source_belnding) {
            if (reg > 1 || reg < 0)
              return;
            assigned_index = func_signature->DefineOutput(OutputRenderTarget{
              .dual_source_blending = true,
              .index = reg,
              .type = to_msl_type(type),
            });
          } else {
            assigned_index = func_signature->DefineOutput(OutputRenderTarget{
              .dual_source_blending = false,
              .index = reg,
              .type = to_msl_type(type),
            });
          }
          epilogue >> pop_output_reg(reg, mask, assigned_index);
        }
      );
      max_output_register = std::max(reg + 1, max_output_register);
      break;
    }
    default:
      assert(0 && "unhandled pixel shader output");
      break;
    }
    break;
  }
  default:
    assert(0 && "unhandled pixel shader output");
    break;
  }
};

void handle_signature_hs(
  CSignatureParser &inputParser, CSignatureParser &outputParser,
  D3D10ShaderBinary::CInstruction &Inst, SM50ShaderInternal *sm50_shader,
  uint32_t phase
) {
  uint32_t &max_input_register = sm50_shader->max_input_register;
  uint32_t &max_output_register = sm50_shader->max_output_register;
  uint32_t &max_patch_constant_output_register =
    sm50_shader->max_patch_constant_output_register;
  auto &epilogue_ = sm50_shader->epilogue_;

  switch (Inst.m_OpCode) {
  case D3D10_SB_OPCODE_DCL_INPUT_SGV: {
    unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
    // auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
    // auto MinPrecision = Inst.m_Operands[0].m_MinPrecision; // not used
    auto sgv = Inst.m_InputDeclSGV.Name;
    switch (sgv) {
    default:
      assert(0 && "Unexpected/unhandled hull shader sgv");
      break;
    }
    max_input_register = std::max(reg + 1, max_input_register);
    break;
  }
  case D3D10_SB_OPCODE_DCL_INPUT: {
    D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;

    switch (RegType) {
    case D3D11_SB_OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID:
    case D3D11_SB_OPERAND_TYPE_INPUT_FORK_INSTANCE_ID:
    case D3D11_SB_OPERAND_TYPE_INPUT_JOIN_INSTANCE_ID:
    case D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID: {
      break;
    }
    case D3D10_SB_OPERAND_TYPE_INPUT: {
      break;
    }
    case D3D11_SB_OPERAND_TYPE_INPUT_PATCH_CONSTANT: {
      break;
    }
    case D3D11_SB_OPERAND_TYPE_INPUT_CONTROL_POINT: {
      break;
    }
    case D3D11_SB_OPERAND_TYPE_OUTPUT_CONTROL_POINT: {
      sm50_shader->shader_info.output_control_point_read = true;
      break;
    }

    default:
      assert(0 && "unhandled hull shader input");
      break;
    }
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
    case D3D11_SB_NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR:
    case D3D11_SB_NAME_FINAL_QUAD_V_EQ_0_EDGE_TESSFACTOR:
    case D3D11_SB_NAME_FINAL_QUAD_U_EQ_1_EDGE_TESSFACTOR:
    case D3D11_SB_NAME_FINAL_QUAD_V_EQ_1_EDGE_TESSFACTOR:
    case D3D11_SB_NAME_FINAL_QUAD_U_INSIDE_TESSFACTOR:
    case D3D11_SB_NAME_FINAL_QUAD_V_INSIDE_TESSFACTOR: {
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_tess_factor(
                      reg, mask,
                      (siv - D3D11_SB_NAME_FINAL_QUAD_U_EQ_0_EDGE_TESSFACTOR), 6
                    );
      });
      break;
    }
    case D3D11_SB_NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR:
    case D3D11_SB_NAME_FINAL_TRI_V_EQ_0_EDGE_TESSFACTOR:
    case D3D11_SB_NAME_FINAL_TRI_W_EQ_0_EDGE_TESSFACTOR:
    case D3D11_SB_NAME_FINAL_TRI_INSIDE_TESSFACTOR: {
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >>
          pop_output_tess_factor(
            reg, mask, (siv - D3D11_SB_NAME_FINAL_TRI_U_EQ_0_EDGE_TESSFACTOR), 4
          );
      });
      break;
    }
    default:
      assert(0 && "Unexpected/unhandled hull shader output siv");
      break;
    }
    if (phase != ~0u) {
      max_patch_constant_output_register =
        std::max(reg + 1, max_patch_constant_output_register);
    } else {
      max_output_register = std::max(reg + 1, max_output_register);
    }
    break;
  }
  case D3D10_SB_OPCODE_DCL_OUTPUT: {
    D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;
    switch (RegType) {
    case D3D10_SB_OPERAND_TYPE_OUTPUT: {
      // normal output register
      auto reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
      auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
      if (phase != ~0u) {
        assert(
          Inst.m_Operands[0].m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D
        );
        for (unsigned i = 0; i < 4; i++) {
          if (mask & (1 << i)) {
            sm50_shader->patch_constant_scalars.push_back(
              {.component = (uint8_t)(i), .reg = (uint8_t)reg}
            );
          }
        }
        max_patch_constant_output_register =
          std::max(reg + 1, max_patch_constant_output_register);
      } else {
        max_output_register = std::max(reg + 1, max_output_register);
      }
      break;
    }
    default:
      assert(0 && "unhandled hull shader output");
      break;
    }
    break;
  }
  default:
    assert(0);
    break;
  }
};

void handle_signature_ds(
  CSignatureParser &inputParser, CSignatureParser &outputParser,
  D3D10ShaderBinary::CInstruction &Inst, SM50ShaderInternal *sm50_shader,
  uint32_t phase_unused
) {
  uint32_t &max_input_register = sm50_shader->max_input_register;
  uint32_t &max_output_register = sm50_shader->max_output_register;
  auto &prelogue_ = sm50_shader->input_prelogue_;
  auto &epilogue_ = sm50_shader->epilogue_;
  auto &func_signature = sm50_shader->func_signature;

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

  switch (Inst.m_OpCode) {
  case D3D10_SB_OPCODE_DCL_INPUT_SIV: {
    assert(0 && "dcl_input_siv should not happen for now");
    // because we don't support hull/domain/geometry
    // and pixel shader has its own dcl_input_ps
    break;
  }
  case D3D10_SB_OPCODE_DCL_INPUT_SGV: {
    assert(0);
    break;
  }
  case D3D10_SB_OPCODE_DCL_INPUT: {
    D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;

    switch (RegType) {
    case D3D11_SB_OPERAND_TYPE_INPUT_DOMAIN_POINT: {
      auto assigned_index = func_signature.DefineInput(
        InputPositionInPatch{.patch = func_signature.GetPatchType()}
      );
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
        prelogue << make_effect([=](struct context ctx) {
          auto attr = ctx.function->getArg(assigned_index);
          ctx.resource.domain = attr;
          return std::monostate{};
        });
      });
      break;
    }
    case D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID: {
      auto assigned_index = func_signature.DefineInput(InputPatchID{});
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
        prelogue << make_effect([=](struct context ctx) {
          auto attr = ctx.function->getArg(assigned_index);
          ctx.resource.patch_id = attr;
          return std::monostate{};
        });
      });
      break;
    }
    case D3D10_SB_OPERAND_TYPE_INPUT:
      assert(0);
      break;
    case D3D11_SB_OPERAND_TYPE_INPUT_PATCH_CONSTANT: {
      unsigned reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
      assert(Inst.m_Operands[0].m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
      max_input_register = std::max(reg + 1, max_input_register);
      break;
    }
    case D3D11_SB_OPERAND_TYPE_INPUT_CONTROL_POINT:
      break;
    default:
      assert(0 && "unhandeld domain shader input");
      break;
    }
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
      for (unsigned i = 0; i < 4; i++) {
        if (mask & (1 << i)) {
          sm50_shader->clip_distance_scalars.push_back(
            {.component = (uint8_t)(i), .reg = (uint8_t)reg}
          );
        }
      }
      break;
    }
    case D3D10_SB_NAME_CULL_DISTANCE:
      assert(0 && "Metal doesn't support shader output: cull distance");
      break;
    case D3D10_SB_NAME_POSITION: {
      auto assigned_index =
        func_signature.DefineOutput(OutputPosition{.type = msl_float4});
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_reg(reg, mask, assigned_index);
      });
      break;
    }
    case D3D10_SB_NAME_RENDER_TARGET_ARRAY_INDEX: {
      auto assigned_index =
        func_signature.DefineOutput(OutputRenderTargetArrayIndex{});
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_reg(reg, mask, assigned_index);
      });
      break;
    }
    case D3D10_SB_NAME_VIEWPORT_ARRAY_INDEX: {
      auto assigned_index =
        func_signature.DefineOutput(OutputViewportArrayIndex{});
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_reg(reg, mask, assigned_index);
      });
      break;
    }
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
    case D3D10_SB_OPERAND_TYPE_OUTPUT: {
      // normal output register
      auto reg = Inst.m_Operands[0].m_Index[0].m_RegIndex;
      auto mask = Inst.m_Operands[0].m_WriteMask >> 4;
      auto sig = findOutputElement([=](Signature sig) {
        return (sig.reg() == reg) && ((sig.mask() & mask) != 0);
      });
      uint32_t assigned_index = func_signature.DefineOutput(OutputVertex{
        .user = sig.fullSemanticString(),
        .type = to_msl_type(sig.componentType()),
      });
      epilogue_.push_back([=](IRValue &epilogue, auto, auto) {
        epilogue >> pop_output_reg(reg, mask, assigned_index);
      });
      max_output_register = std::max(reg + 1, max_output_register);
      break;
    }
    default:
      assert(0 && "unhandled domain shader output");
      break;
    }
    break;
  }
  default:
    assert(0);
    break;
  }
};

void handle_signature_cs(
  CSignatureParser &inputParser_unused, CSignatureParser &outputParser_unused,
  D3D10ShaderBinary::CInstruction &Inst, SM50ShaderInternal *sm50_shader,
  uint32_t phase_unused
) {
  auto &prelogue_ = sm50_shader->input_prelogue_;
  auto &func_signature = sm50_shader->func_signature;

  switch (Inst.m_OpCode) {
  case D3D10_SB_OPCODE_DCL_INPUT_SGV: {
    assert(0 && "unhandled compute shader sgv");
    break;
  }
  case D3D10_SB_OPCODE_DCL_INPUT: {
    D3D10_SB_OPERAND_TYPE RegType = Inst.m_Operands[0].m_Type;

    switch (RegType) {
    case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID: {
      auto assigned_index =
        func_signature.DefineInput(InputThreadPositionInGrid{});
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
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
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
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
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
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
      prelogue_.push_back([=](IREffect &prelogue, auto, auto) {
        prelogue << make_effect([=](struct context ctx) {
          auto attr = ctx.function->getArg(assigned_index);
          ctx.resource.thread_id_in_group_flat_arg = attr;
          return std::monostate{};
        });
      });
      break;
    }
    default:
      assert(0 && "unhandled compute shader input");
      break;
    }
    break;
  }
  default:
    assert(0);
    break;
  }
};

void handle_signature(
  microsoft::CSignatureParser &inputParser,
  microsoft::CSignatureParser &outputParser,
  microsoft::D3D10ShaderBinary::CInstruction &Inst, SM50Shader *sm50_shader,
  uint32_t phase
) {
  auto shader = (SM50ShaderInternal *)sm50_shader;
  switch (shader->shader_type) {
  case D3D10_SB_PIXEL_SHADER:
    return handle_signature_ps(inputParser, outputParser, Inst, shader, phase);
  case D3D10_SB_VERTEX_SHADER:
    return handle_signature_vs(inputParser, outputParser, Inst, shader, phase);
  case D3D11_SB_HULL_SHADER:
    return handle_signature_hs(inputParser, outputParser, Inst, shader, phase);
  case D3D11_SB_DOMAIN_SHADER:
    return handle_signature_ds(inputParser, outputParser, Inst, shader, phase);
  case D3D11_SB_COMPUTE_SHADER:
    return handle_signature_cs(inputParser, outputParser, Inst, shader, phase);
  default:
    assert(0);
    break;
  }
}

}; // namespace dxmt::dxbc