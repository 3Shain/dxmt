#include "dxbc_instructions.hpp"

#include "DXBCParser/ShaderBinary.h"
#include "dxbc_converter.hpp"
#include "shader_common.hpp"

namespace dxmt::dxbc {

auto readOperandRelativeIndex(
  const microsoft::D3D10ShaderBinary::COperandIndex &OpIndex, uint32_t phase,
  uint32_t offset = 0
) -> OperandIndex {
  using namespace microsoft;
  switch (OpIndex.m_RelRegType) {
  case D3D10_SB_OPERAND_TYPE_TEMP: {
    return IndexByTempComponent{
      .regid = OpIndex.m_RelIndex,
      .phase = phase,
      .component = (uint8_t)OpIndex.m_ComponentName,
      .offset = offset,
    };
  }

  case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP: {
    return IndexByIndexableTempComponent{
      .regfile = OpIndex.m_RelIndex,
      .regid = OpIndex.m_RelIndex1,
      .phase = phase,
      .component = (uint8_t)OpIndex.m_ComponentName,
      .offset = offset,
    };
  }

  default:
    DXASSERT_DXBC(false);
  };
};

auto readOperandIndex(
  const microsoft::D3D10ShaderBinary::COperandIndex &OpIndex,
  const microsoft::D3D10_SB_OPERAND_INDEX_REPRESENTATION indexType,
  uint32_t phase
) -> OperandIndex {
  using namespace microsoft;

  switch (indexType) {
  case D3D10_SB_OPERAND_INDEX_IMMEDIATE32:
    DXASSERT_DXBC(OpIndex.m_RelRegType == D3D10_SB_OPERAND_TYPE_IMMEDIATE32);
    return OpIndex.m_RegIndex;

  case D3D10_SB_OPERAND_INDEX_IMMEDIATE64:
    DXASSERT_DXBC(false);
    break;

  case D3D10_SB_OPERAND_INDEX_RELATIVE: {
    return readOperandRelativeIndex(OpIndex, phase, 0);
  };

  case D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE: {
    return readOperandRelativeIndex(OpIndex, phase, OpIndex.m_RegIndex);
  }

  case D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE:
    DXASSERT_DXBC(false);
    break;

  default:
    DXASSERT_DXBC(false);
    break;
  }
};

auto readDstOperand(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase, OperandDataType write_type
) -> DstOperand {
  using namespace microsoft;
  switch (O.m_Type) {
  case D3D10_SB_OPERAND_TYPE_TEMP: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
    unsigned Reg = O.m_Index[0].m_RegIndex;
    return DstOperandTemp{
      ._ = {.mask = O.m_WriteMask >> 4, .write_type = write_type},
      .regid = Reg,
      .phase = phase,
    };
    break;
  }

  case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D);
    DXASSERT_DXBC(O.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
    unsigned Reg = O.m_Index[0].m_RegIndex;

    return DstOperandIndexableTemp{
      ._ = {.mask = O.m_WriteMask >> 4, .write_type = write_type},
      .regfile = Reg,
      .regindex = readOperandIndex(O.m_Index[1], O.m_IndexType[1], phase),
      .phase = phase,
    };
  }

  case D3D10_SB_OPERAND_TYPE_OUTPUT: {
    unsigned Reg = O.m_Index[0].m_RegIndex;
    if (O.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {
      return DstOperandOutput{
      ._ = {.mask = O.m_WriteMask >> 4, .write_type = write_type},
        .regid = Reg,
        .phase = phase,
      };
    }
    return DstOperandIndexableOutput{
      ._ = {.mask = O.m_WriteMask >> 4, .write_type = write_type},
      .regindex = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
      .phase = phase,
    };
  }

  case D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH:
  case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
  case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL: {
    return DstOperandOutputDepth{
      ._ = {.mask = 1, .write_type = write_type},
    };
  }
  case D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK: {
    return DstOperandOutputCoverageMask{
      ._ = {.mask = 1, .write_type = write_type},
    };
  }
  case D3D11_SB_OPERAND_TYPE_OUTPUT_STENCIL_REF: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_0D);
    // for (unsigned c = 0; c < DXBC::kWidth; c++) {
    //   if (!Mask.IsSet(c))
    //     continue;

    //   // Retrieve signature element.
    //   DXASSERT(m_pSM->IsPS(), "PS has only one output stream.");
    //   const DxilSignatureElement *E =
    //   m_pOutputSignature->GetElement(O.m_Type); CompType DxbcValueType =
    //   E->GetCompType(); Type *pLlvmDxbcValueType =
    //   DxbcValueType.GetLLVMType(m_Ctx);

    //   Value *Args[5];
    //   Args[0] = m_pOP->GetU32Const((unsigned)OP::OpCode::StoreOutput); //
    //   OpCode Args[1] = m_pOP->GetU32Const(E->GetID()); // Output signature
    //   element ID Args[2] = m_pOP->GetU32Const(0);          // Row, relative
    //   to the element Args[3] = m_pOP->GetU8Const(
    //       c - E->GetStartCol()); // Col, relative to the element
    //   Args[4] = MarkPrecise(CastDxbcValue(DstVal[c], ValueType,
    //   DxbcValueType),
    //                         c); // Value
    //   Function *F =
    //       m_pOP->GetOpFunc(OP::OpCode::StoreOutput, pLlvmDxbcValueType);
    //   MarkPrecise(m_pBuilder->CreateCall(F, Args));
    // }
    assert(0 && "Unhandled operand type");
    break;
  }

  case D3D10_SB_OPERAND_TYPE_NULL: {
    return DstOperandNull{
      ._ = {.mask = 0, .write_type = write_type},
    };
  }

  default:
    assert(0 && "Unhandled operand type");
  }
}

auto readSrcOperandSwizzle(const microsoft::D3D10ShaderBinary::COperandBase &O
) -> Swizzle {
  using namespace microsoft;
  switch (O.m_ComponentSelection) {
  case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE:
    return Swizzle{
      .x = O.m_Swizzle[0],
      .y = O.m_Swizzle[1],
      .z = O.m_Swizzle[2],
      .w = O.m_Swizzle[3],
    };
  case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE: {
    return Swizzle{
      .x = (uint8_t)O.m_ComponentName,
      .y = (uint8_t)O.m_ComponentName,
      .z = (uint8_t)O.m_ComponentName,
      .w = (uint8_t)O.m_ComponentName,
    };
  }
  case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE: {
    DXASSERT_DXBC(false && "can not read swizzle from mask");
    // DXASSERT_DXBC(O.m_WriteMask >> 4 == 0b1111);
    // return swizzle_identity;
  }
  }
}

auto readSrcOperandCommon(
  const microsoft::D3D10ShaderBinary::COperandBase &O, OperandDataType read_type
) -> SrcOperandCommon {
  return SrcOperandCommon{
    .swizzle = readSrcOperandSwizzle(O),
    .abs = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_ABS) != 0,
    .neg = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_NEG) != 0,
    .read_type = read_type,
  };
}

SrcOperand readSrcOperand(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase, OperandDataType read_type
) {
  using namespace microsoft;
  switch (O.m_Type) {
  case D3D10_SB_OPERAND_TYPE_IMMEDIATE32: {

    DXASSERT_DXBC(O.m_Modifier == D3D10_SB_OPERAND_MODIFIER_NONE);

    if (O.m_NumComponents == D3D10_SB_OPERAND_4_COMPONENT) {
      return SrcOperandImmediate32{
        ._ = {swizzle_identity, false, false, read_type},
        .uvalue =
          {
            O.m_Value[0],
            O.m_Value[1],
            O.m_Value[2],
            O.m_Value[3],
          },
      };
    } else {
      return SrcOperandImmediate32{
        ._ = {swizzle_identity, false, false, read_type},
        .uvalue =
          {
            O.m_Value[0],
            O.m_Value[0],
            O.m_Value[0],
            O.m_Value[0],
          },
      };
    }
  }

  case D3D10_SB_OPERAND_TYPE_TEMP: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
    unsigned Reg = O.m_Index[0].m_RegIndex;
    return SrcOperandTemp{
      ._ = readSrcOperandCommon(O, read_type),
      .regid = Reg,
      .phase = phase,
    };
  }
  case D3D10_SB_OPERAND_TYPE_INPUT: {
    if (O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D) {
      DXASSERT_DXBC(O.m_IndexType[1] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
      DXASSERT_DXBC(phase == ~0u);
      return SrcOperandInputICP{
        ._ = readSrcOperandCommon(O, read_type),
        .cpid = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
        .regid = O.m_Index[1].m_RegIndex,
      };
    }
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
    if (O.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {

      unsigned Reg = O.m_Index[0].m_RegIndex;
      return SrcOperandInput{
        ._ = readSrcOperandCommon(O, read_type),
        .regid = Reg,
      };
    }
    return SrcOperandIndexableInput{
      ._ = readSrcOperandCommon(O, read_type),
      .regindex = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_CONTROL_POINT: {
    return SrcOperandInputICP{
      ._ = readSrcOperandCommon(O, read_type),
      .cpid = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
      .regid = O.m_Index[1].m_RegIndex,
    };
  }
  case D3D11_SB_OPERAND_TYPE_OUTPUT_CONTROL_POINT: {
    return SrcOperandInputOCP{
      ._ = readSrcOperandCommon(O, read_type),
      .cpid = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
      .regid = O.m_Index[1].m_RegIndex,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_PATCH_CONSTANT: {
    return SrcOperandInputPC{
      ._ = readSrcOperandCommon(O, read_type),
      .regindex = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
    };
  }
  case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D);
    DXASSERT_DXBC(O.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
    unsigned Reg = O.m_Index[0].m_RegIndex;
    return SrcOperandIndexableTemp{
      ._ = readSrcOperandCommon(O, read_type),
      .regfile = Reg,
      .regindex = readOperandIndex(O.m_Index[1], O.m_IndexType[1], phase),
      .phase = phase,
    };
  }

  case D3D10_SB_OPERAND_TYPE_RASTERIZER: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_0D);
    DXASSERT_DXBC(false); // ?
    break;
  }
  case D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER: {
    if (O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D) {
      return SrcOperandConstantBuffer{
        ._ = readSrcOperandCommon(O, read_type),
        .rangeid = O.m_Index[0].m_RegIndex,
        .rangeindex = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
        .regindex = readOperandIndex(O.m_Index[1], O.m_IndexType[1], phase),
      };
    }
    assert(0 && "TODO: SM5.1");
  }
  case D3D10_SB_OPERAND_TYPE_IMMEDIATE_CONSTANT_BUFFER: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
    return SrcOperandImmediateConstantBuffer{
      ._ = readSrcOperandCommon(O, read_type),
      .regindex = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_GROUP_ID: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::ThreadGroupId,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::ThreadId,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::ThreadIdInGroup,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::ThreadIdInGroupFlatten,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_COVERAGE_MASK: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::CoverageMask,
    };
  }
  case D3D11_SB_OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID: {
    return SrcOperandAttribute{
      // providing swizzle here because compiler emits
      // D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE for selection mode
      ._ =
        {
          .swizzle = swizzle_identity,
          .abs = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_ABS) != 0,
          .neg = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_NEG) != 0,
          .read_type = read_type,
        },
      .attribute = shader::common::InputAttribute::OutputControlPointId,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_FORK_INSTANCE_ID: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::ForkInstanceId,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_JOIN_INSTANCE_ID: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::JoinInstanceId,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_DOMAIN_POINT: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::Domain,
    };
  }
  case D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID: {
    return SrcOperandAttribute{
      ._ =
        {
          .swizzle = swizzle_identity,
          .abs = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_ABS) != 0,
          .neg = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_NEG) != 0,
          .read_type = read_type,
        },
      .attribute = shader::common::InputAttribute::PrimitiveId,
    };
  }
  case D3D11_SB_OPERAND_TYPE_INPUT_GS_INSTANCE_ID: {
    return SrcOperandAttribute{
      ._ = readSrcOperandCommon(O, read_type),
      .attribute = shader::common::InputAttribute::GSInstanceId,
    };
  }
  default:
    DXASSERT_DXBC(false && "unhandled src operand");
  }
};

auto readSrcOperandResource(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase
) -> SrcOperandResource {
  using namespace microsoft;
  assert(O.m_Type == D3D10_SB_OPERAND_TYPE_RESOURCE);
  if (O.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {
    if (O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D) {
      return SrcOperandResource{
        .range_id = O.m_Index[0].m_RegIndex,
        .index = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
        .read_swizzle = readSrcOperandSwizzle(O)
      };
    } else {
      return SrcOperandResource{
        .range_id = O.m_Index[0].m_RegIndex,
        .index = readOperandIndex(O.m_Index[1], O.m_IndexType[1], phase),
        .read_swizzle = readSrcOperandSwizzle(O)
      };
    }
  } else {
    assert(0 && "interface resource is not supported yet");
  }
}

auto readSrcOperandSampler(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase
) -> SrcOperandSampler {
  if (O.m_IndexDimension == microsoft::D3D10_SB_OPERAND_INDEX_1D) {
    DXASSERT_DXBC(
      O.m_IndexType[0] == microsoft::D3D10_SB_OPERAND_INDEX_IMMEDIATE32
    );
    return SrcOperandSampler{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
      .gather_channel = (uint8_t)O.m_ComponentName
    };
  } else {
    return SrcOperandSampler{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[1], O.m_IndexType[1], phase),
      .gather_channel = (uint8_t)O.m_ComponentName
    };
  }
}

auto readSrcOperandUAV(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase
) -> SrcOperandUAV {
  if (O.m_IndexDimension == microsoft::D3D10_SB_OPERAND_INDEX_1D) {
    DXASSERT_DXBC(
      O.m_IndexType[0] == microsoft::D3D10_SB_OPERAND_INDEX_IMMEDIATE32
    );
    return SrcOperandUAV{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
      .read_swizzle = readSrcOperandSwizzle(O)
    };
  } else {
    return SrcOperandUAV{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[1], O.m_IndexType[1], phase),
      .read_swizzle = readSrcOperandSwizzle(O)
    };
  }
}

auto readSrcOperandTGSM(const microsoft::D3D10ShaderBinary::COperandBase &O
) -> SrcOperandTGSM {
  assert(
    O.m_Type == microsoft::D3D11_SB_OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY
  );
  return SrcOperandTGSM{
    .id = O.m_Index[0].m_RegIndex, .read_swizzle = readSrcOperandSwizzle(O)
  };
}

auto readDstOperandUAV(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase
) -> AtomicDstOperandUAV {
  if (O.m_IndexDimension == microsoft::D3D10_SB_OPERAND_INDEX_1D) {
    DXASSERT_DXBC(
      O.m_IndexType[0] == microsoft::D3D10_SB_OPERAND_INDEX_IMMEDIATE32
    );
    return AtomicDstOperandUAV{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[0], O.m_IndexType[0], phase),
      .mask = O.m_WriteMask >> 4
    };
  } else {
    return AtomicDstOperandUAV{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[1], O.m_IndexType[1], phase),
      .mask = O.m_WriteMask >> 4
    };
  }
}

std::variant<AtomicDstOperandUAV, AtomicOperandTGSM> readAtomicDst(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase
) {
  if (O.m_Type == microsoft::D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW) {
    return readDstOperandUAV(O, phase);
  } else if (O.m_Type ==
             microsoft::D3D11_SB_OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY) {
    return AtomicOperandTGSM{
      .id = O.m_Index[0].m_RegIndex, .mask = O.m_WriteMask >> 4
    };
  }
  assert(0 && "unexpected atomic operation destination");
}

std::variant<SrcOperandResource, SrcOperandUAV, SrcOperandTGSM> readTypelessSrc(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase
) {
  if (O.m_Type == microsoft::D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW) {
    return readSrcOperandUAV(O, phase);
  } else if (O.m_Type == microsoft::D3D10_SB_OPERAND_TYPE_RESOURCE) {
    return readSrcOperandResource(O, phase);
  } else if (O.m_Type ==
             microsoft::D3D11_SB_OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY) {
    return readSrcOperandTGSM(O);
  }
  assert(0 && "unexpected typeless load/store operation destination");
}

std::variant<SrcOperandResource, SrcOperandUAV> readSrcResourceOrUAV(
  const microsoft::D3D10ShaderBinary::COperandBase &O, uint32_t phase
) {
  if (O.m_Type == microsoft::D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW) {
    return readSrcOperandUAV(O, phase);
  } else if (O.m_Type == microsoft::D3D10_SB_OPERAND_TYPE_RESOURCE) {
    return readSrcOperandResource(O, phase);
  }
  assert(0 && "unexpected resource operation destination");
}

auto
readInstructionCommon(const microsoft::D3D10ShaderBinary::CInstruction &Inst, ShaderInfo &shader_info)
    -> InstructionCommon {
  return InstructionCommon{
      .saturate = Inst.m_bSaturate != 0,
      .precise_mask = shader_info.refactoringAllowed ? (uint8_t)Inst.m_PreciseMask : (uint8_t)0b1111
  };
};

Instruction readInstruction(
  const microsoft::D3D10ShaderBinary::CInstruction &Inst,
  ShaderInfo &shader_info, uint32_t phase
) {
  // HACK: ENB will access temp register out of range
  for (unsigned i = 0; i < Inst.NumOperands(); i++) {
    auto &O = Inst.Operand(i);
    if (O.OperandType() == microsoft::D3D10_SB_OPERAND_TYPE_TEMP) {
      shader_info.tempRegisterCount =
        std::max(shader_info.tempRegisterCount, O.RegIndex(0) + 1);
    }
  }
  using namespace microsoft;
  switch (Inst.m_OpCode) {
  case microsoft::D3D10_SB_OPCODE_MOV: {
    return InstMov{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MOVC: {
    return InstMovConditional{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src_cond = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D11_SB_OPCODE_SWAPC: {
    return InstSwapConditional{
      .dst0 = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .dst1 = readDstOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src_cond = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[4], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_CLAMP_FEEDBACK:
  case microsoft::D3D10_SB_OPCODE_SAMPLE: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_CLAMP_FEEDBACK;
    auto inst = InstSample{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
      .src_resource = readSrcOperandResource(Inst.m_Operands[2 + sparse], phase),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3 + sparse], phase),
      .offsets =
        {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], Inst.m_TexelOffset[2]},
      .min_lod_clamp = (sparse && Inst.m_Operands[4 + sparse].OperandType() !=
                                    microsoft::D3D10_SB_OPERAND_TYPE_NULL)
                         ? readSrcOperand(Inst.m_Operands[4 + sparse], phase, OperandDataType::Float)
                         : std::optional<SrcOperand>(),
      .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                         : std::optional<DstOperand>(),
    };
    shader_info.srvMap[inst.src_resource.range_id].sampled = true;
    return inst;
  };
  case microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_B_CLAMP_FEEDBACK:
  case microsoft::D3D10_SB_OPCODE_SAMPLE_B: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_B_CLAMP_FEEDBACK;
    auto inst = InstSampleBias{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
      .src_resource = readSrcOperandResource(Inst.m_Operands[2 + sparse], phase),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3 + sparse], phase),
      .src_bias = readSrcOperand(Inst.m_Operands[4 + sparse], phase, OperandDataType::Float),
      .offsets =
        {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], Inst.m_TexelOffset[2]},
      .min_lod_clamp = (sparse && Inst.m_Operands[5 + sparse].OperandType() !=
                                    microsoft::D3D10_SB_OPERAND_TYPE_NULL)
                         ? readSrcOperand(Inst.m_Operands[5 + sparse], phase, OperandDataType::Float)
                         : std::optional<SrcOperand>(),
      .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                         : std::optional<DstOperand>(),
    };
    shader_info.srvMap[inst.src_resource.range_id].sampled = true;
    return inst;
  };
  case microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_D_CLAMP_FEEDBACK:
  case microsoft::D3D10_SB_OPCODE_SAMPLE_D: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_D_CLAMP_FEEDBACK;
    auto inst = InstSampleDerivative{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
      .src_resource = readSrcOperandResource(Inst.m_Operands[2 + sparse], phase),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3 + sparse], phase),
      .src_x_derivative = readSrcOperand(Inst.m_Operands[4 + sparse], phase, OperandDataType::Float),
      .src_y_derivative = readSrcOperand(Inst.m_Operands[5 + sparse], phase, OperandDataType::Float),
      .offsets =
        {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], Inst.m_TexelOffset[2]},
      .min_lod_clamp = (sparse && Inst.m_Operands[6 + sparse].OperandType() !=
                                    microsoft::D3D10_SB_OPERAND_TYPE_NULL)
                         ? readSrcOperand(Inst.m_Operands[6 + sparse], phase, OperandDataType::Float)
                         : std::optional<SrcOperand>(),
      .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                         : std::optional<DstOperand>(),
    };
    shader_info.srvMap[inst.src_resource.range_id].sampled = true;
    return inst;
  };
  case microsoft::D3D10_SB_OPCODE_SAMPLE_L:
  case microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_L_FEEDBACK: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_L_FEEDBACK;
    auto inst = InstSampleLOD{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
      .src_resource = readSrcOperandResource(Inst.m_Operands[2 + sparse], phase),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3 + sparse], phase),
      .src_lod = readSrcOperand(Inst.m_Operands[4 + sparse], phase, OperandDataType::Float),
      .offsets =
        {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], Inst.m_TexelOffset[2]},
      .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                         : std::optional<DstOperand>(),
    };
    shader_info.srvMap[inst.src_resource.range_id].sampled = true;
    return inst;
  };
  case microsoft::D3D10_SB_OPCODE_SAMPLE_C_LZ:
  case microsoft::D3D10_SB_OPCODE_SAMPLE_C:
  case microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_C_CLAMP_FEEDBACK:
  case microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_C_LZ_FEEDBACK: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_C_CLAMP_FEEDBACK ||
                  Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_C_LZ_FEEDBACK;
    bool sparse_clamp = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_C_CLAMP_FEEDBACK;
    bool level_zero = Inst.m_OpCode == microsoft::D3D10_SB_OPCODE_SAMPLE_C_LZ ||
                      Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_SAMPLE_C_LZ_FEEDBACK;
    auto inst = InstSampleCompare{
        .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
        .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
        .src_resource = readSrcOperandResource(Inst.m_Operands[2 + sparse], phase),
        .src_sampler = readSrcOperandSampler(Inst.m_Operands[3 + sparse], phase),
        .src_reference = readSrcOperand(Inst.m_Operands[4 + sparse], phase, OperandDataType::Float),
        .offsets = {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], Inst.m_TexelOffset[2]},
        .min_lod_clamp = (sparse_clamp && Inst.m_Operands[5 + sparse].OperandType() != microsoft::D3D10_SB_OPERAND_TYPE_NULL)
                             ? readSrcOperand(Inst.m_Operands[5 + sparse], phase, OperandDataType::Float)
                             : std::optional<SrcOperand>(),
        .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                           : std::optional<DstOperand>(),
        .level_zero = level_zero,
    };
    shader_info.srvMap[inst.src_resource.range_id].compared = true;
    return inst;
  };
  case microsoft::D3D10_1_SB_OPCODE_GATHER4:
  case microsoft::D3DWDDM1_3_SB_OPCODE_GATHER4_FEEDBACK: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_GATHER4_FEEDBACK;
    auto src_resource = readSrcOperandResource(Inst.m_Operands[2 + sparse], phase);
    auto sample_type = shader_info.srvMap[src_resource.range_id].scaler_type;
    auto inst = InstGather{
      .dst = readDstOperand(
        Inst.m_Operands[0], phase,
        sample_type == shader::common::ScalerDataType::Uint
          ? OperandDataType::Integer
        : sample_type == shader::common::ScalerDataType::Int
          ? OperandDataType::Integer
          : OperandDataType::Float
      ),
      .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
      .src_resource = src_resource,
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3 + sparse], phase),
      .offset =
        SrcOperandImmediate32{
          ._ = {swizzle_identity, false, false, OperandDataType::Integer},
          .ivalue = {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], 0, 0},
        },
      .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                         : std::optional<DstOperand>(),
    };
    shader_info.srvMap[src_resource.range_id].sampled = true;
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_GATHER4_C:
  case microsoft::D3DWDDM1_3_SB_OPCODE_GATHER4_C_FEEDBACK: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_GATHER4_C_FEEDBACK;
    auto inst = InstGatherCompare{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
      .src_resource = readSrcOperandResource(Inst.m_Operands[2 + sparse], phase),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3 + sparse], phase),
      .src_reference = readSrcOperand(Inst.m_Operands[4 + sparse], phase, OperandDataType::Float),
      .offset =
        SrcOperandImmediate32{
          ._ = {swizzle_identity, false, false, OperandDataType::Integer},
          .ivalue = {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], 0, 0},
        },
      .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                         : std::optional<DstOperand>(),
    };
    shader_info.srvMap[inst.src_resource.range_id].compared = true;
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_GATHER4_PO:
  case microsoft::D3DWDDM1_3_SB_OPCODE_GATHER4_PO_FEEDBACK: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_GATHER4_PO_FEEDBACK;
    auto src_resource = readSrcOperandResource(Inst.m_Operands[3 + sparse], phase);
    auto sample_type = shader_info.srvMap[src_resource.range_id].scaler_type;
    auto inst = InstGather{
      .dst = readDstOperand(
        Inst.m_Operands[0], phase,
        sample_type == shader::common::ScalerDataType::Uint
          ? OperandDataType::Integer
        : sample_type == shader::common::ScalerDataType::Int
          ? OperandDataType::Integer
          : OperandDataType::Float
      ),
      .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
      .src_resource = src_resource,
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[4 + sparse], phase),
      .offset = readSrcOperand(Inst.m_Operands[2 + sparse], phase, OperandDataType::Integer),
      .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                         : std::optional<DstOperand>(),
    };
    shader_info.srvMap[src_resource.range_id].sampled = true;
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_GATHER4_PO_C:
  case microsoft::D3DWDDM1_3_SB_OPCODE_GATHER4_PO_C_FEEDBACK: {
    bool sparse = Inst.m_OpCode == microsoft::D3DWDDM1_3_SB_OPCODE_GATHER4_PO_C_FEEDBACK;
    auto inst = InstGatherCompare{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src_address = readSrcOperand(Inst.m_Operands[1 + sparse], phase, OperandDataType::Float),
      .src_resource = readSrcOperandResource(Inst.m_Operands[3 + sparse], phase),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[4 + sparse], phase),
      .src_reference = readSrcOperand(Inst.m_Operands[5 + sparse], phase, OperandDataType::Float),
      .offset = readSrcOperand(Inst.m_Operands[2 + sparse], phase, OperandDataType::Integer),
      .feedback = sparse ? readDstOperand(Inst.m_Operands[sparse], phase, OperandDataType::Integer)
                         : std::optional<DstOperand>(),
    };
    shader_info.srvMap[inst.src_resource.range_id].compared = true;
    return inst;
  };
  case microsoft::D3D10_1_SB_OPCODE_SAMPLE_INFO: {
    bool return_uint =
      Inst.m_InstructionReturnType == D3D10_SB_INSTRUCTION_RETURN_UINT;
    if (Inst.m_Operands[1].m_Type ==
        microsoft::D3D10_SB_OPERAND_TYPE_RASTERIZER) {
      return InstSampleInfo{
        .dst = readDstOperand(
          Inst.m_Operands[0], phase,
          return_uint ? OperandDataType::Integer : OperandDataType::Float
        ),
        .src = {},
        .uint_result = return_uint,
        .read_swizzle = readSrcOperandSwizzle(Inst.m_Operands[1]),
      };
    } else {
      auto src = readSrcOperandResource(Inst.m_Operands[1], phase);
      shader_info.srvMap[src.range_id].read = true;
      return InstSampleInfo{
        .dst = readDstOperand(
          Inst.m_Operands[0], phase,
          return_uint ? OperandDataType::Integer : OperandDataType::Float
        ),
        .src = src,
        .uint_result = return_uint,
        .read_swizzle = readSrcOperandSwizzle(Inst.m_Operands[1]),
      };
    };
  };
  case microsoft::D3D10_1_SB_OPCODE_SAMPLE_POS: {
    shader_info.use_samplepos = true;
    if (Inst.m_Operands[1].m_Type ==
        microsoft::D3D10_SB_OPERAND_TYPE_RASTERIZER) {
      return InstSamplePos{
        .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
        .src = {},
        .src_sample_index = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
        .read_swizzle = readSrcOperandSwizzle(Inst.m_Operands[1]),
      };
    } else {
      auto src = readSrcOperandResource(Inst.m_Operands[1], phase);
      shader_info.srvMap[src.range_id].read = true;
      return InstSamplePos{
        .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
        .src = src,
        .src_sample_index = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
        .read_swizzle = readSrcOperandSwizzle(Inst.m_Operands[1]),
      };
    };
  };
  case microsoft::D3D11_SB_OPCODE_BUFINFO: {
    auto inst = InstBufferInfo{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcResourceOrUAV(Inst.m_Operands[1], phase),
    };
    std::visit(
      patterns{
        [&](const SrcOperandResource &res) {
          shader_info.srvMap[res.range_id].read = true;
        },
        [&](const SrcOperandUAV &uav) {
          shader_info.uavMap[uav.range_id].read = true;
        },
        [](auto) {}
      },
      inst.src
    );
    return inst;
  };
  case microsoft::D3D10_SB_OPCODE_RESINFO: {
    InstResourceInfo::M modifier =
      (Inst.m_ResInfoReturnType ==
       microsoft::D3D10_SB_RESINFO_INSTRUCTION_RETURN_UINT)
        ? InstResourceInfo::M::uint
      : (Inst.m_ResInfoReturnType ==
         D3D10_SB_RESINFO_INSTRUCTION_RETURN_RCPFLOAT)
        ? InstResourceInfo::M::rcp
        : InstResourceInfo::M::none;
    auto inst = InstResourceInfo{
      .dst = readDstOperand(
        Inst.m_Operands[0], phase,
        modifier == InstResourceInfo::M::uint ? OperandDataType::Integer
                                              : OperandDataType::Float
      ),
      .src_mip_level = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src_resource = readSrcResourceOrUAV(Inst.m_Operands[2], phase),
      .modifier = modifier
    };
    std::visit(
      patterns{
        [&](const SrcOperandResource &res) {
          shader_info.srvMap[res.range_id].read = true;
        },
        [&](const SrcOperandUAV &uav) {
          shader_info.uavMap[uav.range_id].read = true;
        },
        [](auto) {}
      },
      inst.src_resource
    );
    return inst;
  };
  case microsoft::D3D10_SB_OPCODE_LD: {
    auto src_resource = readSrcOperandResource(Inst.m_Operands[2], phase);
    auto sample_type = shader_info.srvMap[src_resource.range_id].scaler_type;
    auto inst = InstLoad{
      .dst = readDstOperand(
        Inst.m_Operands[0], phase,
        sample_type == shader::common::ScalerDataType::Uint
          ? OperandDataType::Integer
        : sample_type == shader::common::ScalerDataType::Int
          ? OperandDataType::Integer
          : OperandDataType::Float
      ),
      .src_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src_resource = src_resource,
      .src_sample_index = {},
      .offsets =
        {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], Inst.m_TexelOffset[2]},
    };
    shader_info.srvMap[src_resource.range_id].read = true;
    return inst;
  };
  case microsoft::D3D10_SB_OPCODE_LD_MS: {
    auto src_resource = readSrcOperandResource(Inst.m_Operands[2], phase);
    auto sample_type = shader_info.srvMap[src_resource.range_id].scaler_type;
    auto inst = InstLoad{
      .dst = readDstOperand(
        Inst.m_Operands[0], phase,
        sample_type == shader::common::ScalerDataType::Uint
          ? OperandDataType::Integer
        : sample_type == shader::common::ScalerDataType::Int
          ? OperandDataType::Integer
          : OperandDataType::Float
      ),
      .src_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src_resource = src_resource,
      .src_sample_index = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .offsets =
        {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], Inst.m_TexelOffset[2]},
    };
    shader_info.srvMap[src_resource.range_id].read = true;
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_LD_UAV_TYPED: {
    auto src_uav = readSrcOperandUAV(Inst.m_Operands[2], phase);
    auto sample_type = shader_info.uavMap[src_uav.range_id].scaler_type;
    auto inst = InstLoadUAVTyped{
      .dst = readDstOperand(
        Inst.m_Operands[0], phase,
        sample_type == shader::common::ScalerDataType::Uint
          ? OperandDataType::Integer
        : sample_type == shader::common::ScalerDataType::Int
          ? OperandDataType::Integer
          : OperandDataType::Float
      ),
      .src_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src_uav = src_uav,
    };
    shader_info.uavMap[src_uav.range_id].read = true;
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_STORE_UAV_TYPED: {
    auto dst = readDstOperandUAV(Inst.m_Operands[0], phase);
    auto sample_type = shader_info.uavMap[dst.range_id].scaler_type;
    auto inst = InstStoreUAVTyped{
      .dst = dst,
      .src_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(
        Inst.m_Operands[2], phase,
        sample_type == shader::common::ScalerDataType::Uint
          ? OperandDataType::Integer
        : sample_type == shader::common::ScalerDataType::Int
          ? OperandDataType::Integer
          : OperandDataType::Float
      ),
    };
    shader_info.uavMap[dst.range_id].written = true;
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_LD_RAW: {
    auto inst = InstLoadRaw{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src_byte_offset = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readTypelessSrc(Inst.m_Operands[2], phase),
      .opt_flag_offset_is_vec4_aligned = false,
    };
    std::visit(
      patterns{
        [&](const SrcOperandResource &res) {
          shader_info.srvMap[res.range_id].read = true;
        },
        [&](const SrcOperandUAV &uav) {
          shader_info.uavMap[uav.range_id].read = true;
        },
        [](auto) {}
      },
      inst.src
    );
    std::visit(
      patterns{
        [&](const SrcOperandImmediate32 &imm) {
          inst.opt_flag_offset_is_vec4_aligned = (imm.uvalue[0] & 0xF) == 0;
        },
        [](auto) {}
      },
      inst.src_byte_offset
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_STORE_RAW: {
    auto inst = InstStoreRaw{
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_byte_offset = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](const AtomicDstOperandUAV &uav) {
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_LD_STRUCTURED: {
    auto inst = InstLoadStructured{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src_byte_offset = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readTypelessSrc(Inst.m_Operands[3], phase),
      .opt_flag_offset_is_vec4_aligned = false,
    };
    std::visit(
      patterns{
        [&](const SrcOperandResource &res) {
          shader_info.srvMap[res.range_id].read = true;
        },
        [&](const SrcOperandUAV &uav) {
          shader_info.uavMap[uav.range_id].read = true;
        },
        [](auto) {}
      },
      inst.src
    );
    std::visit(
      patterns{
        [&](const SrcOperandImmediate32 &imm) {
          inst.opt_flag_offset_is_vec4_aligned = ((imm.uvalue[0] & 0xF) == 0);
        },
        [](auto) {}
      },
      inst.src_byte_offset
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_STORE_STRUCTURED: {
    auto inst = InstStoreStructured{
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .dst_byte_offset = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](const AtomicDstOperandUAV &uav) {
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D10_SB_OPCODE_DP2: {
    return InstDotProduct{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
      .dimension = 2,
    };
  };
  case microsoft::D3D10_SB_OPCODE_DP3: {
    return InstDotProduct{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
      .dimension = 3,
    };
  };
  case microsoft::D3D10_SB_OPCODE_DP4: {
    return InstDotProduct{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
      .dimension = 4,
    };
  };
  case microsoft::D3D10_SB_OPCODE_RSQ: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::Rsq,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D11_SB_OPCODE_RCP: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::Rcp,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_LOG: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::Log2,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_EXP: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::Exp2,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_SQRT: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::Sqrt,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ROUND_Z: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::RoundZero,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ROUND_PI: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::RoundPositiveInf,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ROUND_NI: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::RoundNegativeInf,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ROUND_NE: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::RoundNearestEven,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_FRC: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatUnaryOp::Fraction,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ADD: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatBinaryOp::Add,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MUL: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatBinaryOp::Mul,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_DIV: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatBinaryOp::Div,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MAX: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatBinaryOp::Max,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MIN: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = FloatBinaryOp::Min,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MAD: {
    return InstFloatMAD{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
      .src2 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IMAD: {
    return InstIntegerMAD{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src2 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .is_signed = true,
    };
  };
  case microsoft::D3D10_SB_OPCODE_UMAD: {
    return InstIntegerMAD{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src2 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .is_signed = false,
    };
  };
  case microsoft::D3D11_1_SB_OPCODE_MSAD: {
    shader_info.use_msad = true;
    return InstMaskedSumOfAbsDiff{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src2 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_SINCOS: {
    return InstSinCos{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst_sin = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .dst_cos = readDstOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_EQ: {
    return InstFloatCompare{
      ._ = readInstructionCommon(Inst, shader_info),
      .cmp = FloatComparison::Equal,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_GE: {
    return InstFloatCompare{
      ._ = readInstructionCommon(Inst, shader_info),
      .cmp = FloatComparison::GreaterEqual,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_LT: {
    return InstFloatCompare{
      ._ = readInstructionCommon(Inst, shader_info),
      .cmp = FloatComparison::LessThan,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_NE: {
    return InstFloatCompare{
      ._ = readInstructionCommon(Inst, shader_info),
      .cmp = FloatComparison::NotEqual,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IEQ: {
    return InstIntegerCompare{
      .cmp = IntegerComparison::Equal,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_INE: {
    return InstIntegerCompare{
      .cmp = IntegerComparison::NotEqual,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IGE: {
    return InstIntegerCompare{
      .cmp = IntegerComparison::SignedGreaterEqual,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_ILT: {
    return InstIntegerCompare{
      .cmp = IntegerComparison::SignedLessThan,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UGE: {
    return InstIntegerCompare{
      .cmp = IntegerComparison::UnsignedGreaterEqual,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_ULT: {
    return InstIntegerCompare{
      .cmp = IntegerComparison::UnsignedLessThan,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_NOT: {
    return InstIntegerUnaryOp{
      .op = IntegerUnaryOp::Not,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_INEG: {
    return InstIntegerUnaryOp{
      .op = IntegerUnaryOp::Neg,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_BFREV: {
    return InstIntegerUnaryOp{
      .op = IntegerUnaryOp::ReverseBits,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_COUNTBITS: {
    return InstIntegerUnaryOp{
      .op = IntegerUnaryOp::CountBits,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_FIRSTBIT_HI: {
    return InstIntegerUnaryOp{
      .op = IntegerUnaryOp::FirstHiBit,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_FIRSTBIT_SHI: {
    return InstIntegerUnaryOp{
      .op = IntegerUnaryOp::FirstHiBitSigned,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_FIRSTBIT_LO: {
    return InstIntegerUnaryOp{
      .op = IntegerUnaryOp::FirstLowBit,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_BFI: {
    return InstBitFiledInsert{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src2 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .src3 = readSrcOperand(Inst.m_Operands[4], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_UBFE: {
    return InstExtractBits{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src2 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .is_signed = false
    };
  };
  case microsoft::D3D11_SB_OPCODE_IBFE: {
    return InstExtractBits{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src2 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .is_signed = true
    };
  };
  case microsoft::D3D10_SB_OPCODE_ISHL: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::IShl,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_ISHR: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::IShr,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_USHR: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::UShr,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_XOR: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::Xor,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_OR: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::Or,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_AND: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::And,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UMIN: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::UMin,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UMAX: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::UMax,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IMIN: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::IMin,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IMAX: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::IMax,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IADD: {
    return InstIntegerBinaryOp{
      .op = IntegerBinaryOp::Add,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IMUL: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = IntegerBinaryOpWithTwoDst::IMul,
      .dst_hi = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .dst_low = readDstOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UMUL: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = IntegerBinaryOpWithTwoDst::UMul,
      .dst_hi = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .dst_low = readDstOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UDIV: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = IntegerBinaryOpWithTwoDst::UDiv,
      .dst_hi = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .dst_low = readDstOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_UADDC: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = IntegerBinaryOpWithTwoDst::UAddCarry,
      .dst_hi = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .dst_low = readDstOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_USUBB: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst, shader_info),
      .op = IntegerBinaryOpWithTwoDst::USubBorrow,
      .dst_hi = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .dst_low = readDstOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D11_SB_OPCODE_F16TOF32: {
    return InstConvert{
      .op = ConversionOp::HalfToFloat,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Half16X16),
    };
  };
  case microsoft::D3D11_SB_OPCODE_F32TOF16: {
    return InstConvert{
      .op = ConversionOp::FloatToHalf,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Half16X16),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_FTOI: {
    return InstConvert{
      .op = ConversionOp::FloatToSigned,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_FTOU: {
    return InstConvert{
      .op = ConversionOp::FloatToUnsigned,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
    };
  };
  case microsoft::D3D10_SB_OPCODE_ITOF: {
    return InstConvert{
      .op = ConversionOp::SignedToFloat,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UTOF: {
    return InstConvert{
      .op = ConversionOp::UnsignedToFloat,
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
    };
  };
  case microsoft::D3D10_SB_OPCODE_DERIV_RTX:
  case microsoft::D3D11_SB_OPCODE_DERIV_RTX_FINE: {
    return InstPartialDerivative{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .ddy = false,
      .coarse = false,
    };
  };
  case microsoft::D3D10_SB_OPCODE_DERIV_RTY:
  case microsoft::D3D11_SB_OPCODE_DERIV_RTY_FINE: {
    return InstPartialDerivative{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .ddy = true,
      .coarse = false,
    };
  };
  case microsoft::D3D11_SB_OPCODE_DERIV_RTX_COARSE: {
    return InstPartialDerivative{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .ddy = false,
      .coarse = true,
    };
  };
  case microsoft::D3D11_SB_OPCODE_DERIV_RTY_COARSE: {
    return InstPartialDerivative{
      ._ = readInstructionCommon(Inst, shader_info),
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .ddy = true,
      .coarse = true,
    };
  };
  case microsoft::D3D10_1_SB_OPCODE_LOD: {
    return InstCalcLOD{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .src_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Float),
      .src_resource = readSrcOperandResource(Inst.m_Operands[2], phase),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3], phase),
    };
  };
  case microsoft::D3D10_SB_OPCODE_NOP: {
    return InstNop{};
  };
  case microsoft::D3D11_SB_OPCODE_SYNC: {
    return InstSync{
      .uav_boundary = Inst.m_SyncFlags.bUnorderedAccessViewMemoryGlobal
                        ? InstSync::UAVBoundary::global
                        : (Inst.m_SyncFlags.bUnorderedAccessViewMemoryGroup
                             ? InstSync::UAVBoundary::group
                             : InstSync::UAVBoundary::none),
      .tgsm_memory_barrier = Inst.m_SyncFlags.bThreadGroupSharedMemory,
      .tgsm_execution_barrier = Inst.m_SyncFlags.bThreadsInGroup
    };
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_AND: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::And,
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .dst_original = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_OR: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::Or,
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .dst_original = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_XOR: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::Xor,
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .dst_original = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_IADD: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::Add,
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .dst_original = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_IMAX: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::IMax,
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .dst_original = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_IMIN: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::IMin,
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .dst_original = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_UMAX: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::UMax,
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .dst_original = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_UMIN: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::UMin,
      .dst = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .dst_original = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_ATOMIC_CMP_STORE: {
    shader_info.use_cmp_exch = true;
    auto inst = InstAtomicImmCmpExchange{
      .dst = DstOperandSideEffect{._ = {.mask = 0b1111, .write_type = OperandDataType::Integer}},
      .dst_resource = readAtomicDst(Inst.m_Operands[0], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[1], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst_resource
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_ALLOC: {
    auto inst = InstAtomicImmIncrement{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .uav = readDstOperandUAV(Inst.m_Operands[1], phase)
    };
    shader_info.uavMap[inst.uav.range_id].with_counter = true;
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_CONSUME: {
    auto inst = InstAtomicImmDecrement{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .uav = readDstOperandUAV(Inst.m_Operands[1], phase)
    };
    shader_info.uavMap[inst.uav.range_id].with_counter = true;
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_EXCH: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::Xchg,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };

  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_CMP_EXCH: {
    shader_info.use_cmp_exch = true;
    auto inst = InstAtomicImmCmpExchange{
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
      .dst_resource = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src0 = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .src1 = readSrcOperand(Inst.m_Operands[4], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst_resource
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_AND: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::And,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_OR: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::Or,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_XOR: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::Xor,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_IADD: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::Add,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_UMAX: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::UMax,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_UMIN: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::UMin,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_IMAX: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::IMax,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_IMM_ATOMIC_IMIN: {
    auto inst = InstAtomicBinOp{
      .op = AtomicBinaryOp::IMin,
      .dst = readAtomicDst(Inst.m_Operands[1], phase),
      .dst_address = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .src = readSrcOperand(Inst.m_Operands[3], phase, OperandDataType::Integer),
      .dst_original = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Integer),
    };
    std::visit(
      patterns{
        [&](AtomicDstOperandUAV uav) {
          shader_info.uavMap[uav.range_id].read = true;
          shader_info.uavMap[uav.range_id].written = true;
        },
        [](auto) {}
      },
      inst.dst
    );
    return inst;
  };
  case microsoft::D3D11_SB_OPCODE_EVAL_CENTROID: {
    assert(Inst.m_Operands[1].m_Type == microsoft::D3D10_SB_OPERAND_TYPE_INPUT);
    assert(Inst.m_Operands[1].m_IndexType[0] == microsoft::D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
    auto regid = Inst.m_Operands[1].m_Index[0].m_RegIndex;
    shader_info.pull_mode_reg_mask |= (1 << regid);
    return InstInterpolateCentroid {
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .regid = regid,
      .read_swizzle = readSrcOperandSwizzle(Inst.m_Operands[1])
    };
  }
  case microsoft::D3D11_SB_OPCODE_EVAL_SAMPLE_INDEX: {
    assert(Inst.m_Operands[1].m_Type == microsoft::D3D10_SB_OPERAND_TYPE_INPUT);
    assert(Inst.m_Operands[1].m_IndexType[0] == microsoft::D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
    auto regid = Inst.m_Operands[1].m_Index[0].m_RegIndex;
    shader_info.pull_mode_reg_mask |= (1 << regid);
    return InstInterpolateSample {
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .sample_index = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .regid = regid,
      .read_swizzle = readSrcOperandSwizzle(Inst.m_Operands[1])
    };
  }
  case microsoft::D3D11_SB_OPCODE_EVAL_SNAPPED: {
    assert(Inst.m_Operands[1].m_Type == microsoft::D3D10_SB_OPERAND_TYPE_INPUT);
    assert(Inst.m_Operands[1].m_IndexType[0] == microsoft::D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
    auto regid = Inst.m_Operands[1].m_Index[0].m_RegIndex;
    shader_info.pull_mode_reg_mask |= (1 << regid);
    return InstInterpolateOffset {
      .dst = readDstOperand(Inst.m_Operands[0], phase, OperandDataType::Float),
      .offset = readSrcOperand(Inst.m_Operands[2], phase, OperandDataType::Integer),
      .regid = regid,
      .read_swizzle = readSrcOperandSwizzle(Inst.m_Operands[1])
    };
  }
  default: {
    llvm::outs() << "unhandled dxbc instruction " << Inst.OpCode() << "\n";
    assert(0 && "unhandled dxbc instruction");
  }
  }
};

BasicBlockCondition readCondition(
  const microsoft::D3D10ShaderBinary::CInstruction &Inst, uint32_t OpIdx,
  uint32_t phase
) {
  using namespace microsoft;
  const microsoft::D3D10ShaderBinary::COperandBase &O = Inst.m_Operands[OpIdx];
  D3D10_SB_INSTRUCTION_TEST_BOOLEAN TestType = Inst.m_Test;

  return BasicBlockCondition{
    .operand = readSrcOperand(O, phase, OperandDataType::Integer),
    .test_nonzero = TestType == D3D10_SB_INSTRUCTION_TEST_NONZERO
  };
}
} // namespace dxmt::dxbc
