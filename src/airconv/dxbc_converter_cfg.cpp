#include "DXBCParser/ShaderBinary.h"
#include "dxbc_converter.hpp"
#include "dxbc_instructions.hpp"
#include <memory>
#include <stack>

using namespace microsoft;
using namespace dxmt::shader::common;

namespace dxmt::dxbc {

inline ResourceType
to_shader_resource_type(D3D10_SB_RESOURCE_DIMENSION dim) {
  switch (dim) {
  case D3D10_SB_RESOURCE_DIMENSION_UNKNOWN:
  case D3D10_SB_RESOURCE_DIMENSION_BUFFER:
  case D3D11_SB_RESOURCE_DIMENSION_RAW_BUFFER:
  case D3D11_SB_RESOURCE_DIMENSION_STRUCTURED_BUFFER:
    return ResourceType::TextureBuffer;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURE1D:
    return ResourceType::Texture1D;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D:
    return ResourceType::Texture2D;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMS:
    return ResourceType::Texture2DMultisampled;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURE3D:
    return ResourceType::Texture3D;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBE:
    return ResourceType::TextureCube;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURE1DARRAY:
    return ResourceType::Texture1DArray;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DARRAY:
    return ResourceType::Texture2DArray;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DMSARRAY:
    return ResourceType::Texture2DMultisampledArray;
  case D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBEARRAY:
    return ResourceType::TextureCubeArray;
  }
  assert(0 && "invalid D3D10_SB_RESOURCE_DIMENSION");
};

inline ScalerDataType
to_shader_scaler_type(microsoft::D3D10_SB_RESOURCE_RETURN_TYPE type) {
  switch (type) {

  case microsoft::D3D10_SB_RETURN_TYPE_UNORM:
  case microsoft::D3D10_SB_RETURN_TYPE_SNORM:
  case microsoft::D3D10_SB_RETURN_TYPE_FLOAT:
    return ScalerDataType::Float;
  case microsoft::D3D10_SB_RETURN_TYPE_SINT:
    return ScalerDataType::Int;
  case microsoft::D3D10_SB_RETURN_TYPE_UINT:
    return ScalerDataType::Uint;
  case microsoft::D3D10_SB_RETURN_TYPE_MIXED:
    return ScalerDataType::Uint; // kinda weird but ok
  case microsoft::D3D11_SB_RETURN_TYPE_DOUBLE:
  case microsoft::D3D11_SB_RETURN_TYPE_CONTINUED:
  case microsoft::D3D11_SB_RETURN_TYPE_UNUSED:
    break;
  }
  assert(0 && "invalid D3D10_SB_RESOURCE_RETURN_TYPE");
};

std::vector<std::unique_ptr<BasicBlock>>
read_control_flow(
    D3D10ShaderBinary::CShaderCodeParser &Parser, SM50ShaderInternal *sm50_shader, CSignatureParser &inputParser,
    CSignatureParser5 &outputParser
) {
  using namespace air;

  std::vector<std::unique_ptr<BasicBlock>> all_bb;

  std::map<uint32_t, BasicBlock *> func_entries;

  BasicBlock bb_void("voidbb");

  auto fresh_bb = [&](const char *name) -> BasicBlock * {
    all_bb.push_back(std::make_unique<BasicBlock>(name));
    return all_bb.back().get();
  };

  BasicBlock *bb_current;
  uint32_t phase = ~0u;
  std::stack<BasicBlock *> bb_endif;
  std::stack<BasicBlock *> bb_continue_target;
  std::stack<BasicBlock *> bb_break_target;
  std::stack<BasicBlockSwitch *> switch_ctx;
  std::stack<BasicBlockInstanceBarrier *> instance_ctx;

  BasicBlock *func_return_point = nullptr;
  std::vector<std::pair<BasicBlock *, BasicBlockCall *>> call_targets;

  bb_current = fresh_bb("entrybb");

  auto bb_return = fresh_bb("returnbb");
  bb_return->target = BasicBlockReturn{};

  bool sm_ver_5_1_ = Parser.ShaderMajorVersion() == 5 && Parser.ShaderMinorVersion() >= 1;
  auto &shader_info = sm50_shader->shader_info;
  auto &func_signature = sm50_shader->func_signature;

  while (!Parser.EndOfShader()) {
    D3D10ShaderBinary::CInstruction Inst;
    Parser.ParseInstruction(&Inst);

    // HACK: mark all instructions as precise if refactor is not allowed
    if (!shader_info.refactoringAllowed)
      Inst.SetPreciseMask(0b1111);

    switch (Inst.OpCode()) {

    case D3D10_SB_OPCODE_IF: {
      auto if_true = fresh_bb("if_true");
      auto end_if = fresh_bb("end_if");
      bb_endif.push(end_if);
      bb_current->target = BasicBlockConditionalBranch{readCondition(Inst, 0, phase), if_true, end_if};
      bb_current = if_true;
      break;
    }
    case D3D10_SB_OPCODE_ELSE: {
      auto end_if = fresh_bb("end_if");

      bb_current->target = BasicBlockUnconditionalBranch{end_if};
      bb_current = bb_endif.top();
      bb_endif.pop();

      bb_current->debug_name = "if_alternative"; // previously it is `end_if`
      bb_endif.push(end_if);
      break;
    }
    case D3D10_SB_OPCODE_ENDIF: {
      bb_current->target = BasicBlockUnconditionalBranch{bb_endif.top()};
      bb_current = bb_endif.top();
      bb_endif.pop();
      break;
    }

    case D3D10_SB_OPCODE_LOOP: {
      auto loop_entrace = fresh_bb("loop_entrace");
      auto end_loop = fresh_bb("end_loop");
      bb_continue_target.push(loop_entrace);
      bb_break_target.push(end_loop);

      bb_current->target = BasicBlockUnconditionalBranch{loop_entrace};
      bb_current = loop_entrace;
      break;
    }
    case D3D10_SB_OPCODE_ENDLOOP: {
      bb_current->target = BasicBlockUnconditionalBranch{bb_continue_target.top()};
      bb_current = bb_break_target.top();
      bb_continue_target.pop();
      bb_break_target.pop();
      break;
    }
    case D3D10_SB_OPCODE_BREAK: {
      bb_current->target = BasicBlockUnconditionalBranch{bb_break_target.top()};
      bb_current = &bb_void;
      break;
    }
    case D3D10_SB_OPCODE_CONTINUE: {
      bb_current->target = BasicBlockUnconditionalBranch{bb_continue_target.top()};
      bb_current = &bb_void;
      break;
    }
    case D3D10_SB_OPCODE_BREAKC: {
      auto after_breakc = fresh_bb("after_breakc");
      bb_current->target =
          BasicBlockConditionalBranch{readCondition(Inst, 0, phase), bb_break_target.top(), after_breakc};
      bb_current = after_breakc;
      break;
    }
    case D3D10_SB_OPCODE_CONTINUEC: {
      auto after_continuec = fresh_bb("after_continuec");
      bb_current->target =
          BasicBlockConditionalBranch{readCondition(Inst, 0, phase), bb_continue_target.top(), after_continuec};
      bb_current = after_continuec;
      break;
    }
    case D3D10_SB_OPCODE_SWITCH: {
      auto end_switch = fresh_bb("end_switch");
      bb_break_target.push(end_switch);

      bb_current->target =
          BasicBlockSwitch{readSrcOperand(Inst.Operand(0), phase, OperandDataType::Integer), {}, end_switch};
      switch_ctx.push(&std::get<BasicBlockSwitch>(bb_current->target));
      bb_current = &bb_void;
      break;
    }

    case D3D10_SB_OPCODE_CASE: {
      auto case_body = fresh_bb("switch_case");

      const D3D10ShaderBinary::COperandBase &O = Inst.m_Operands[0];
      DXASSERT_DXBC(O.m_Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 && O.m_NumComponents == D3D10_SB_OPERAND_1_COMPONENT);
      uint32_t case_value = O.m_Value[0];

      switch_ctx.top()->cases.insert({case_value, case_body});
      bb_current->target = BasicBlockUnconditionalBranch{case_body};
      bb_current = case_body;
      break;
    }
    case D3D10_SB_OPCODE_DEFAULT: {
      auto case_default = fresh_bb("case_default");

      switch_ctx.top()->case_default = case_default;
      bb_current->target = BasicBlockUnconditionalBranch{case_default};
      bb_current = case_default;
      break;
    }
    case D3D10_SB_OPCODE_ENDSWITCH: {
      bb_current->target = BasicBlockUnconditionalBranch{bb_break_target.top()};
      bb_current = bb_break_target.top();
      bb_break_target.pop();
      switch_ctx.pop();
      break;
    }

    case D3D10_SB_OPCODE_RET: {
      auto ret_target = func_return_point      ? func_return_point
                        : instance_ctx.empty() ? bb_return
                                               : instance_ctx.top()->sync;
      bb_current->target = BasicBlockUnconditionalBranch{ret_target};
      if (bb_break_target.empty() && bb_endif.empty()) {
        // top-level return
        bb_current = ret_target;
        if (func_return_point) {
          bb_current = bb_return;
          func_return_point = nullptr;
        } else if (!instance_ctx.empty()) {
          // in hull shader, RET is effective for current phase only and we intend to continue execution
          instance_ctx.pop();
        }
      } else {
        bb_current = &bb_void;
      }
      break;
    }
    case D3D10_SB_OPCODE_RETC: {
      auto ret_target = func_return_point      ? func_return_point
                        : instance_ctx.empty() ? bb_return
                                               : instance_ctx.top()->sync;
      auto after_retc = fresh_bb("after_retc");
      bb_current->target = BasicBlockConditionalBranch{readCondition(Inst, 0, phase), ret_target, after_retc};
      bb_current = after_retc;
      break;
    }

    case D3D10_SB_OPCODE_DISCARD: {
      auto fulfilled_ = fresh_bb("discard_fulfilled");
      auto otherwise_ = fresh_bb("discard_otherwise");
      bb_current->target = BasicBlockConditionalBranch{readCondition(Inst, 0, phase), fulfilled_, otherwise_};
      fulfilled_->target = BasicBlockUnconditionalBranch{otherwise_};
      fulfilled_->instructions.push_back(InstPixelDiscard{});
      bb_current = otherwise_;
      break;
    }

    case D3D11_SB_OPCODE_HS_CONTROL_POINT_PHASE: {
      shader_info.no_control_point_phase_passthrough = true;
      auto control_point_active = fresh_bb("control_point_active");
      auto control_point_end = fresh_bb("control_point_end");
      control_point_end->instructions.push_back(InstSync{
          .uav_boundary = InstSync::UAVBoundary::none,
          .tgsm_memory_barrier = true,
          .tgsm_execution_barrier = true,
      });

      auto instance_count = sm50_shader->output_control_point_count;
      bb_current->target = BasicBlockInstanceBarrier{instance_count, control_point_active, control_point_end};
      instance_ctx.push(&std::get<BasicBlockInstanceBarrier>(bb_current->target));
      bb_current = control_point_active;
      break;
    }

    case D3D11_SB_OPCODE_HS_JOIN_PHASE:
    case D3D11_SB_OPCODE_HS_FORK_PHASE: {
      phase++;
      shader_info.phases.push_back(PhaseInfo{});

      auto fork_join_active = fresh_bb("fork_join_active");
      auto fork_join_end = fresh_bb("fork_join_end");
      fork_join_end->instructions.push_back(InstSync{
          .uav_boundary = InstSync::UAVBoundary::none,
          .tgsm_memory_barrier = true,
          .tgsm_execution_barrier = true,
      });

      bb_current->target = BasicBlockInstanceBarrier{1, fork_join_active, fork_join_end};
      instance_ctx.push(&std::get<BasicBlockInstanceBarrier>(bb_current->target));
      bb_current = fork_join_active;
      break;
    }
    case D3D11_SB_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT:
    case D3D11_SB_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT: {
      instance_ctx.top()->instance_count = Inst.m_HSForkPhaseInstanceCountDecl.InstanceCount;
      sm50_shader->hull_maximum_threads_per_patch =
          std::max(sm50_shader->hull_maximum_threads_per_patch, Inst.m_HSForkPhaseInstanceCountDecl.InstanceCount);
      break;
    }
    case D3D10_SB_OPCODE_LABEL: {
      auto func_body = fresh_bb("func_body");
      auto func_end = fresh_bb("func_end");
      func_end->target = BasicBlockReturn{};
      auto func_id = Inst.m_Operands[0].m_Index[0].m_RegIndex;
      func_entries[func_id] = func_body;
      func_return_point = func_end;
      bb_current = func_body;
      break;
    }
    case D3D10_SB_OPCODE_CALL: {
      auto after_call = fresh_bb("after_call");
      auto func_id = Inst.m_Operands[0].m_Index[0].m_RegIndex;
      bb_current->target = BasicBlockCall{func_id, after_call};
      call_targets.push_back({bb_current, &std::get<BasicBlockCall>(bb_current->target)});
      bb_current = after_call;
      break;
    }
    case D3D10_SB_OPCODE_CALLC: {
      auto if_true = fresh_bb("if_true");
      auto after_callc = fresh_bb("after_callc");
      bb_current->target = BasicBlockConditionalBranch{readCondition(Inst, 0, phase), if_true, after_callc};
      call_targets.push_back({bb_current, &std::get<BasicBlockCall>(bb_current->target)});
      auto func_id = Inst.m_Operands[1].m_Index[0].m_RegIndex;
      if_true->target = BasicBlockCall{func_id, after_callc};
      bb_current = after_callc;
      break;
    }

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
      shader_info.cbufferMap[RangeID] = {
          .range =
              {.range_id = RangeID, .lower_bound = LB, .size = RangeSize, .space = Inst.m_ConstantBufferDecl.Space},
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
      shader_info.samplerMap[RangeID] = {
          .range = {.range_id = RangeID, .lower_bound = LB, .size = RangeSize, .space = Inst.m_SamplerDecl.Space},
          // set them later,
          .arg_index = 0,
          .arg_cube_index = 0,
          .arg_metadata_index = 0,
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
        srv.resource_type = to_shader_resource_type(Inst.m_ResourceDecl.Dimension);
        srv.scaler_type = to_shader_scaler_type(Inst.m_ResourceDecl.ReturnType[0]);
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

      shader_info.srvMap[RangeID] = srv;

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
        uav.resource_type = to_shader_resource_type(Inst.m_TypedUAVDecl.Dimension);
        uav.scaler_type = to_shader_scaler_type(Inst.m_TypedUAVDecl.ReturnType[0]);
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

      uav.global_coherent = ((Flags & D3D11_SB_GLOBALLY_COHERENT_ACCESS) != 0);
      uav.rasterizer_order = ((Flags & D3D11_SB_RASTERIZER_ORDERED_ACCESS) != 0);

      shader_info.uavMap[RangeID] = uav;
      break;
    }
    case D3D10_SB_OPCODE_DCL_TEMPS: {
      if (phase != ~0u) {
        assert(shader_info.phases.size() > phase);
        shader_info.phases[phase].tempRegisterCount = Inst.m_TempsDecl.NumTemps;
        break;
      }
      shader_info.tempRegisterCount = Inst.m_TempsDecl.NumTemps;
      break;
    }
    case D3D10_SB_OPCODE_DCL_INDEXABLE_TEMP: {
      if (phase != ~0u) {
        assert(shader_info.phases.size() > phase);
        shader_info.phases[phase].indexableTempRegisterCounts[Inst.m_IndexableTempDecl.IndexableTempNumber] =
            std::make_pair(Inst.m_IndexableTempDecl.NumRegisters, Inst.m_IndexableTempDecl.Mask >> 4);
        break;
      }
      shader_info.indexableTempRegisterCounts[Inst.m_IndexableTempDecl.IndexableTempNumber] =
          std::make_pair(Inst.m_IndexableTempDecl.NumRegisters, Inst.m_IndexableTempDecl.Mask >> 4);
      break;
    }
    case D3D11_SB_OPCODE_DCL_THREAD_GROUP: {
      sm50_shader->threadgroup_size[0] = Inst.m_ThreadGroupDecl.x;
      sm50_shader->threadgroup_size[1] = Inst.m_ThreadGroupDecl.y;
      sm50_shader->threadgroup_size[2] = Inst.m_ThreadGroupDecl.z;
      func_signature.UseMaxWorkgroupSize(
          Inst.m_ThreadGroupDecl.x * Inst.m_ThreadGroupDecl.y * Inst.m_ThreadGroupDecl.z
      );
      break;
    }
    case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW:
    case D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED: {
      ThreadgroupBufferInfo tgsm;
      if (Inst.OpCode() == D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW) {
        tgsm.stride = 0;
        tgsm.size = Inst.m_RawTGSMDecl.ByteCount;
        tgsm.size_in_uint = tgsm.size / 4;
        tgsm.structured = false;
        assert((Inst.m_RawTGSMDecl.ByteCount & 0b11) == 0); // is multiple of 4
      } else {
        tgsm.stride = Inst.m_StructuredTGSMDecl.StructByteStride;
        tgsm.size = Inst.m_StructuredTGSMDecl.StructCount;
        tgsm.size_in_uint = tgsm.stride * tgsm.size / 4;
        tgsm.structured = true;
        assert((Inst.m_StructuredTGSMDecl.StructByteStride & 0b11) == 0); // is multiple of 4
      }

      shader_info.tgsmMap[Inst.m_Operands[0].m_Index[0].m_RegIndex] = tgsm;
      break;
    }
    case D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS: {
      if (Inst.m_GlobalFlagsDecl.Flags & D3D11_SB_GLOBAL_FLAG_FORCE_EARLY_DEPTH_STENCIL) {
        func_signature.UseEarlyFragmentTests();
      }
      if (Inst.m_GlobalFlagsDecl.Flags & D3D11_1_SB_GLOBAL_FLAG_SKIP_OPTIMIZATION) {
        shader_info.skipOptimization = true;
      }
      if ((Inst.m_GlobalFlagsDecl.Flags & D3D10_SB_GLOBAL_FLAG_REFACTORING_ALLOWED) == 0) {
        shader_info.refactoringAllowed = false;
      }
      break;
    }
    case D3D10_SB_OPCODE_DCL_INPUT_SIV:
    case D3D10_SB_OPCODE_DCL_INPUT_SGV:
    case D3D10_SB_OPCODE_DCL_INPUT:
    case D3D10_SB_OPCODE_DCL_INPUT_PS_SIV:
    case D3D10_SB_OPCODE_DCL_INPUT_PS_SGV:
    case D3D10_SB_OPCODE_DCL_INPUT_PS:
    case D3D10_SB_OPCODE_DCL_OUTPUT_SGV:
    case D3D10_SB_OPCODE_DCL_OUTPUT_SIV:
    case D3D10_SB_OPCODE_DCL_OUTPUT: {
      handle_signature(inputParser, outputParser, Inst, sm50_shader, phase);
      break;
    }
    case D3D10_SB_OPCODE_CUSTOMDATA: {
      if (Inst.m_CustomData.Type == D3D10_SB_CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER) {
        // must be list of 4-tuples
        unsigned size_in_vec4 = Inst.m_CustomData.DataSizeInBytes >> 4;
        DXASSERT_DXBC(Inst.m_CustomData.DataSizeInBytes == size_in_vec4 * 16);
        shader_info.immConstantBufferData.assign(
            (std::array<uint32_t, 4> *)Inst.m_CustomData.pData,
            ((std::array<uint32_t, 4> *)Inst.m_CustomData.pData) + size_in_vec4
        );
      }
      break;
    }
    case D3D10_SB_OPCODE_DCL_INDEX_RANGE:
      break; // ignore, and it turns out backend compiler can handle alloca
    case D3D10_SB_OPCODE_DCL_GS_INPUT_PRIMITIVE:
      sm50_shader->gs_input_primitive = Inst.m_InputPrimitiveDecl.Primitive;
      break;
    case D3D10_SB_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY:
      sm50_shader->gs_output_topology = Inst.m_OutputTopologyDecl.Topology;
      break;
    case D3D10_SB_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT:
      sm50_shader->gs_max_vertex_output = Inst.m_GSMaxOutputVertexCountDecl.MaxOutputVertexCount;
      break;
    case D3D11_SB_OPCODE_DCL_GS_INSTANCE_COUNT:
      sm50_shader->gs_instance_count = Inst.m_GSInstanceCountDecl.InstanceCount;
      break;
    case D3D11_SB_OPCODE_DCL_STREAM:
    case D3D11_SB_OPCODE_DCL_INTERFACE:
    case D3D11_SB_OPCODE_DCL_FUNCTION_TABLE:
    case D3D11_SB_OPCODE_DCL_FUNCTION_BODY:
    case D3D11_SB_OPCODE_HS_DECLS:
      // ignore atm
      break;
    case D3D11_SB_OPCODE_DCL_TESS_PARTITIONING: {
      sm50_shader->tessellation_partition = Inst.m_TessellatorPartitioningDecl.TessellatorPartitioning;
      break;
    }
    case D3D11_SB_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE: {
      sm50_shader->tessellator_output_primitive = Inst.m_TessellatorOutputPrimitiveDecl.TessellatorOutputPrimitive;
      break;
    }
    case D3D11_SB_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT: {
      sm50_shader->input_control_point_count = Inst.m_InputControlPointCountDecl.InputControlPointCount;
      sm50_shader->hull_maximum_threads_per_patch = std::max(
          sm50_shader->hull_maximum_threads_per_patch, Inst.m_InputControlPointCountDecl.InputControlPointCount
      );
      break;
    }
    case D3D11_SB_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT: {
      sm50_shader->output_control_point_count = Inst.m_OutputControlPointCountDecl.OutputControlPointCount;
      sm50_shader->hull_maximum_threads_per_patch = std::max(
          sm50_shader->hull_maximum_threads_per_patch, Inst.m_OutputControlPointCountDecl.OutputControlPointCount
      );
      break;
    }
    case D3D11_SB_OPCODE_DCL_TESS_DOMAIN: {
      sm50_shader->tessellation_domain = Inst.m_TessellatorDomainDecl.TessellatorDomain;
      break;
    }
    case D3D11_SB_OPCODE_DCL_HS_MAX_TESSFACTOR: {
      sm50_shader->max_tesselation_factor = Inst.m_HSMaxTessFactorDecl.MaxTessFactor;
      break;
    }
    case D3D10_SB_OPCODE_EMIT: {
      bb_current->instructions.push_back(InstEmit{});
      break;
    }
    case D3D10_SB_OPCODE_EMITTHENCUT: {
      bb_current->instructions.push_back(InstEmit{});
      bb_current->instructions.push_back(InstCut{});
      break;
    }
    case D3D10_SB_OPCODE_CUT: {
      bb_current->instructions.push_back(InstCut{});
      break;
    }
    case D3D11_SB_OPCODE_EMIT_STREAM: {
      assert(Inst.m_Operands[0].m_Value[0] == 0 && "multiple streams not supported");
      bb_current->instructions.push_back(InstEmit{});
      break;
    }
    case D3D11_SB_OPCODE_EMITTHENCUT_STREAM: {
      assert(Inst.m_Operands[0].m_Value[0] == 0 && "multiple streams not supported");
      bb_current->instructions.push_back(InstEmit{});
      bb_current->instructions.push_back(InstCut{});
      break;
    }
    case D3D11_SB_OPCODE_CUT_STREAM: {
      assert(Inst.m_Operands[0].m_Value[0] == 0 && "multiple streams not supported");
      bb_current->instructions.push_back(InstCut{});
      break;
    }
    default:
      bb_current->instructions.push_back(readInstruction(Inst, shader_info, phase));
      break;
    }
  }

  if (bb_current != bb_return) {
    // if fork/join phase reads control point phase output, or control point phase is absent
    // then we need to perform a memcpy from threadgroup memory to payload memory
    // otherwise output is written to payload memory directly
    if (shader_info.output_control_point_read || !shader_info.no_control_point_phase_passthrough) {
      bb_current->target = BasicBlockHullShaderWriteOutput{sm50_shader->output_control_point_count, bb_return};
    } else {
      bb_current->target = BasicBlockUnconditionalBranch{bb_return};
    }
    bb_current = bb_return;
  }

  for (int stack_depth = 32; stack_depth > 0; stack_depth--) {
    if (call_targets.empty())
      break;

    std::vector<std::pair<BasicBlock *, BasicBlockCall *>> new_call_targets;

    for (auto [bb, call] : call_targets) {
      std::unordered_map<BasicBlock *, BasicBlock *> visited;
      std::stack<BasicBlock *> block_to_redirect;

      auto clone = [&](BasicBlock *target) -> BasicBlock * {
        if (visited.contains(target))
          return visited.at(target);
        auto cloned = fresh_bb(target->debug_name.c_str());
        cloned->instructions = target->instructions; // copy instructions
        visited.insert({target, cloned});
        block_to_redirect.push(target);
        return cloned;
      };

      bb->target = BasicBlockUnconditionalBranch{clone(func_entries.at(call->func_id))};

      while (!block_to_redirect.empty()) {
        auto current = block_to_redirect.top();
        block_to_redirect.pop();
        auto cloned = visited[current];
        cloned->target = std::visit(
            patterns{
                [](BasicBlockUndefined) -> BasicBlockTarget { return BasicBlockUndefined{}; },
                [&](BasicBlockReturn ret) -> BasicBlockTarget {
                  return BasicBlockUnconditionalBranch{call->return_point};
                },
                [&](BasicBlockUnconditionalBranch uncond) -> BasicBlockTarget {
                  return BasicBlockUnconditionalBranch{clone(uncond.target)};
                },
                [&](BasicBlockConditionalBranch cond) -> BasicBlockTarget {
                  return BasicBlockConditionalBranch{cond.cond, clone(cond.true_branch), clone(cond.false_branch)};
                },
                [&](BasicBlockSwitch swc) -> BasicBlockTarget {
                  BasicBlockSwitch new_swc;
                  new_swc.value = swc.value;
                  new_swc.case_default = clone(swc.case_default);
                  for (auto [val, case_] : swc.cases) {
                    new_swc.cases.insert({val, clone(case_)});
                  }
                  return new_swc;
                },
                [&](BasicBlockInstanceBarrier instance) -> BasicBlockTarget {
                  return BasicBlockInstanceBarrier{
                      instance.instance_count, clone(instance.active), clone(instance.sync)
                  };
                },
                [&](BasicBlockHullShaderWriteOutput hull_end) -> BasicBlockTarget {
                  return BasicBlockHullShaderWriteOutput{hull_end.instance_count, clone(hull_end.epilogue)};
                },
                [&](BasicBlockCall call) -> BasicBlockTarget {
                  if (stack_depth == 1)
                    return BasicBlockUnconditionalBranch{clone(call.return_point)};
                  return BasicBlockCall{call.func_id, clone(call.return_point)};
                },
            },
            current->target
        );
        std::visit(
            patterns{
                [&](BasicBlockCall &call) { new_call_targets.push_back({cloned, &call}); },
                [](auto) {},
            },
            cloned->target
        );
      }
    }

    call_targets = new_call_targets;
  }

  return all_bb;
}

} // namespace dxmt::dxbc