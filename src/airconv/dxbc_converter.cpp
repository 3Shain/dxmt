#include "dxbc_converter.hpp"
#include "DXBCParser/DXBCUtils.h"
#include "DXBCParser/ShaderBinary.h"
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "air_signature.hpp"
#include "dxbc_signature.hpp"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

namespace dxmt::dxbc {

#define DXASSERT_DXBC(x) assert(x);

air::Interpolation
convertInterpolation(microsoft::D3D10_SB_INTERPOLATION_MODE mode) {
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

auto readOperandRelativeIndex(
  const microsoft::D3D10ShaderBinary::COperandIndex &OpIndex,
  uint32_t offset = 0
) -> OperandIndex {
  using namespace microsoft;
  switch (OpIndex.m_RelRegType) {
  case D3D10_SB_OPERAND_TYPE_TEMP: {
    return IndexByTempComponent{
      .regid = OpIndex.m_RelIndex,
      .component = (uint8_t)OpIndex.m_ComponentName,
      .offset = offset,
    };
  }

  case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP: {
    return IndexByIndexableTempComponent{
      .regfile = OpIndex.m_RelIndex,
      .regid = OpIndex.m_RelIndex1,
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
  const microsoft::D3D10_SB_OPERAND_INDEX_REPRESENTATION indexType
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
    return readOperandRelativeIndex(OpIndex, 0);
  };

  case D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE: {
    return readOperandRelativeIndex(OpIndex, OpIndex.m_RegIndex);
  }

  case D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE:
    DXASSERT_DXBC(false);
    break;

  default:
    DXASSERT_DXBC(false);
    break;
  }
};

auto readDstOperand(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> DstOperand {
  using namespace microsoft;
  switch (O.m_Type) {
  case D3D10_SB_OPERAND_TYPE_TEMP: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
    unsigned Reg = O.m_Index[0].m_RegIndex;
    return DstOperandTemp{
      ._ = {.mask = O.m_WriteMask >> 4},
      .regid = Reg,
    };
    // CompType DxbcValueType =
    //     DXBC::GetCompTypeFromMinPrec(O.m_MinPrecision, ValueType);
    // if (DxbcValueType.IsBoolTy()) {
    //   DxbcValueType = CompType::getI32();
    // }
    // Type *pDxbcValueType = DxbcValueType.GetLLVMType(m_Ctx);
    // if (DxbcValueType.GetKind() != CompType::Kind::F64) {
    //   for (BYTE c = 0; c < DXBC::kWidth; c++) {
    //     if (!Mask.IsSet(c))
    //       continue;
    //     Value *Args[3];
    //     Args[0] =
    //         m_pOP->GetU32Const((unsigned)OP::OpCode::TempRegStore); // OpCode
    //     Args[1] = m_pOP->GetU32Const(
    //         DXBC::GetRegIndex(Reg, c)); // Linearized register index
    //     Args[2] = MarkPrecise(
    //         CastDxbcValue(DstVal[c], ValueType, DxbcValueType), c); // Value
    //     Function *F =
    //         m_pOP->GetOpFunc(OP::OpCode::TempRegStore, pDxbcValueType);
    //     MarkPrecise(m_pBuilder->CreateCall(F, Args));
    //   }
    // } else {
    //   for (BYTE c = 0; c < DXBC::kWidth; c += 2) {
    //     if (!Mask.IsSet(c))
    //       continue;
    //     Value *pSDT; // Split double type.
    //     {
    //       Value *Args[2];
    //       Args[0] =
    //           m_pOP->GetU32Const((unsigned)OP::OpCode::SplitDouble); //
    //           OpCode
    //       Args[1] = DstVal[c]; // Double value
    //       Function *F =
    //           m_pOP->GetOpFunc(OP::OpCode::SplitDouble, pDxbcValueType);
    //       pSDT = MarkPrecise(m_pBuilder->CreateCall(F, Args), c);
    //     }
    //     Value *Args[3];
    //     Args[0] =
    //         m_pOP->GetU32Const((unsigned)OP::OpCode::TempRegStore); // OpCode
    //     Args[1] = m_pOP->GetU32Const(
    //         DXBC::GetRegIndex(Reg, c)); // Linearized register index 1
    //     Args[2] = MarkPrecise(m_pBuilder->CreateExtractValue(pSDT, 0),
    //                           c); // Value to store
    //     Function *F =
    //         m_pOP->GetOpFunc(OP::OpCode::TempRegStore,
    //         Type::getInt32Ty(m_Ctx));
    //     Value *pVal = m_pBuilder->CreateCall(F, Args);
    //     MarkPrecise(pVal, c);
    //     Args[1] = m_pOP->GetU32Const(
    //         DXBC::GetRegIndex(Reg, c + 1)); // Linearized register index 2
    //     Args[2] = MarkPrecise(m_pBuilder->CreateExtractValue(pSDT, 1),
    //                           c + 1); // Value to store
    //     MarkPrecise(m_pBuilder->CreateCall(F, Args));
    //   }
    // }

    break;
  }

  case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D);
    DXASSERT_DXBC(O.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
    unsigned Reg = O.m_Index[0].m_RegIndex;

    return DstOperandIndexableTemp{
      ._ = {.mask = O.m_WriteMask >> 4},
      .regfile = Reg,
      .regindex = readOperandIndex(O.m_Index[1], O.m_IndexType[1])
    };
    // IndexableReg &IRRec = m_IndexableRegs[Reg];
    // Value *pXRegIndex = LoadOperandIndex(O.m_Index[1], O.m_IndexType[1]);
    // Value *pRegIndex =
    //     m_pBuilder->CreateMul(pXRegIndex,
    //     m_pOP->GetI32Const(IRRec.NumComps));
    // CompType DxbcValueType =
    //     DXBC::GetCompTypeFromMinPrec(O.m_MinPrecision, ValueType);
    // if (DxbcValueType.IsBoolTy()) {
    //   DxbcValueType = CompType::getI32();
    // }

    // if (DxbcValueType.GetKind() != CompType::Kind::F64) {
    //   for (BYTE c = 0; c < DXBC::kWidth; c++) {
    //     if (!Mask.IsSet(c))
    //       continue;

    //     // Create GEP.
    //     Value *pIndex = m_pBuilder->CreateAdd(pRegIndex,
    //     m_pOP->GetU32Const(c)); Value *pGEPIndices[2] =
    //     {m_pOP->GetU32Const(0), pIndex}; if (!DxbcValueType.HasMinPrec()) {
    //       Value *pBasePtr = m_IndexableRegs[Reg].pValue32;
    //       Value *pPtr = m_pBuilder->CreateGEP(pBasePtr, pGEPIndices);
    //       Value *pValue = MarkPrecise(
    //           CastDxbcValue(DstVal[c], ValueType, CompType::getF32()), c);
    //       MarkPrecise(
    //           m_pBuilder->CreateAlignedStore(pValue, pPtr,
    //           kRegCompAlignment), c);
    //     } else {
    //       Value *pBasePtr = m_IndexableRegs[Reg].pValue16;
    //       Value *pPtr = m_pBuilder->CreateGEP(pBasePtr, pGEPIndices);
    //       Value *pValue = MarkPrecise(
    //           CastDxbcValue(DstVal[c], ValueType, CompType::getF16()), c);
    //       MarkPrecise(m_pBuilder->CreateAlignedStore(pValue, pPtr,
    //                                                  kRegCompAlignment / 2),
    //                   c);
    //     }
    //   }
    // } else {
    //   // Double precision.
    //   for (BYTE c = 0; c < DXBC::kWidth; c += 2) {
    //     if (!Mask.IsSet(c))
    //       continue;

    //     // Create GEP.
    //     Value *pIndex = m_pBuilder->CreateAdd(pRegIndex,
    //     m_pOP->GetU32Const(c)); Value *pGEPIndices[] = {pIndex}; Value
    //     *pBasePtr = m_pBuilder->CreateBitCast(
    //         m_IndexableRegs[Reg].pValue32, Type::getDoublePtrTy(m_Ctx));
    //     Value *pPtr = m_pBuilder->CreateGEP(pBasePtr, pGEPIndices);
    //     MarkPrecise(m_pBuilder->CreateAlignedStore(DstVal[c], pPtr,
    //                                                kRegCompAlignment * 2));
    //   }
    // }
    // break;
  }

  case D3D10_SB_OPERAND_TYPE_OUTPUT: {
    unsigned Reg = O.m_Index[0].m_RegIndex;
    return DstOperandOutput{
      ._ = {.mask = O.m_WriteMask >> 4},
      .regid = Reg,
    };
    // Row index expression.
    // Value *pRowIndexValue = LoadOperandIndex(O.m_Index[0], O.m_IndexType[0]);

    // bool bStoreOutputReg =
    //     !(m_pSM->IsGS() && m_pPR->HasMultipleOutputStreams());

    // if (bStoreOutputReg) {
    //   for (unsigned c = 0; c < DXBC::kWidth; c++) {
    //     if (!Mask.IsSet(c))
    //       continue;

    //     // Retrieve signature element.
    //     OP::OpCode OpCode;
    //     const DxilSignatureElement *E;
    //     if (!m_bPatchConstantPhase) {
    //       E = m_pOutputSignature->GetElementWithStream(
    //           Reg, c, m_pPR->GetOutputStream());
    //       OpCode = OP::OpCode::StoreOutput;
    //     } else {
    //       E = m_pPatchConstantSignature->GetElementWithStream(
    //           Reg, c, m_pPR->GetOutputStream());
    //       OpCode = OP::OpCode::StorePatchConstant;
    //     }
    //     CompType DxbcValueType = E->GetCompType();
    //     if (DxbcValueType.IsBoolTy()) {
    //       DxbcValueType = CompType::getI32();
    //     }
    //     Type *pLlvmDxbcValueType = DxbcValueType.GetLLVMType(m_Ctx);

    //     // Make row index relative within element.
    //     Value *pRowIndexValueRel = m_pBuilder->CreateSub(
    //         pRowIndexValue, m_pOP->GetU32Const(E->GetStartRow()));

    //     Value *Args[5];
    //     Args[0] = m_pOP->GetU32Const((unsigned)OpCode); // OpCode
    //     Args[1] = m_pOP->GetU32Const(E->GetID()); // Output signature element
    //     ID Args[2] = pRowIndexValueRel; // Row, relative to the element
    //     Args[3] = m_pOP->GetU8Const(
    //         c - E->GetStartCol()); // Col, relative to the element
    //     Args[4] = MarkPrecise(
    //         CastDxbcValue(DstVal[c], ValueType, DxbcValueType), c); // Value
    //     Function *F = m_pOP->GetOpFunc(OpCode, pLlvmDxbcValueType);
    //     MarkPrecise(m_pBuilder->CreateCall(F, Args));
    //   }
    // } else {
    //   // In GS with multiple streams, output register file is shared among
    //   the
    //   // streams. Store the values into additional temp registers, and later,
    //   // store these at the emit points.
    //   CompType DxbcValueType =
    //       DXBC::GetCompTypeFromMinPrec(O.m_MinPrecision, ValueType);
    //   if (DxbcValueType.IsBoolTy()) {
    //     DxbcValueType = CompType::getI32();
    //   }
    //   Type *pDxbcValueType = DxbcValueType.GetLLVMType(m_Ctx);

    //   for (BYTE c = 0; c < DXBC::kWidth; c++) {
    //     if (!Mask.IsSet(c))
    //       continue;

    //     Value *Args[3];
    //     Args[0] =
    //         m_pOP->GetU32Const((unsigned)OP::OpCode::TempRegStore); // OpCode
    //     unsigned TempReg = GetGSTempRegForOutputReg(Reg);
    //     Args[1] = m_pOP->GetU32Const(
    //         DXBC::GetRegIndex(TempReg, c)); // Linearized register index
    //     Args[2] =
    //         MarkPrecise(CastDxbcValue(DstVal[c], ValueType, DxbcValueType),
    //                     c); // Value to store
    //     Function *F =
    //         m_pOP->GetOpFunc(OP::OpCode::TempRegStore, pDxbcValueType);
    //     MarkPrecise(m_pBuilder->CreateCall(F, Args));
    //   }
    // }

    break;
  }

  case D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH:
  case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
  case D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL:
  case D3D11_SB_OPERAND_TYPE_OUTPUT_STENCIL_REF:
  case D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK: {
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
    return DstOperandNull{};
  }

  default:
    assert(0 && "Unhandled operand type");
  }
}

auto readSrcOperandSwizzle(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> swizzle {
  using namespace microsoft;
  switch (O.m_ComponentSelection) {
  case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE:
    return swizzle{
      .x = O.m_Swizzle[0],
      .y = O.m_Swizzle[1],
      .z = O.m_Swizzle[2],
      .w = O.m_Swizzle[3],
    };
  case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE: {
    return swizzle{
      .x = (uint8_t)O.m_ComponentName,
      .y = (uint8_t)O.m_ComponentName,
      .z = (uint8_t)O.m_ComponentName,
      .w = (uint8_t)O.m_ComponentName,
    };
  }
  case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE: {
    DXASSERT_DXBC(false && "can not read swizzle from mask");
  }
  }
}

auto readSrcOperandCommon(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperandCommon {
  return SrcOperandCommon{
    .swizzle = readSrcOperandSwizzle(O),
    .abs = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_ABS) != 0,
    .neg = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_NEG) != 0,
  };
}

auto readSrcOperand(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperand {
  using namespace microsoft;
  switch (O.m_Type) {
  case D3D10_SB_OPERAND_TYPE_IMMEDIATE32: {

    DXASSERT_DXBC(O.m_Modifier == D3D10_SB_OPERAND_MODIFIER_NONE);

    if (O.m_NumComponents == D3D10_SB_OPERAND_4_COMPONENT) {
      return SrcOperandImmediate32{
        ._ = readSrcOperandCommon(O),
        .uvalue =
          {
            O.m_Value[0],
            O.m_Value[1],
            O.m_Value[2],
            O.m_Value[3],
          }
      };
    } else {
      return SrcOperandImmediate32{
        ._ = readSrcOperandCommon(O),
        .uvalue =
          {
            O.m_Value[0],
            O.m_Value[0],
            O.m_Value[0],
            O.m_Value[0],
          }
      };
    }
  }

  case D3D10_SB_OPERAND_TYPE_TEMP: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
    unsigned Reg = O.m_Index[0].m_RegIndex;
    return SrcOperandTemp{
      ._ = readSrcOperandCommon(O),
      .regid = Reg,
    };
    // CompType DxbcValueType =
    //     DXBC::GetCompTypeFromMinPrec(O.m_MinPrecision, ValueType);
    // if (DxbcValueType.IsBoolTy()) {
    //   DxbcValueType = CompType::getI32();
    // }
    // Type *pDxbcValueType = DxbcValueType.GetLLVMType(m_Ctx);
    // if (DxbcValueType.GetKind() != CompType::Kind::F64) {
    //   for (OperandValueHelper OVH(SrcVal, Mask, O); !OVH.IsDone();
    //        OVH.Advance()) {
    //     BYTE Comp = OVH.GetComp();
    //     Value *Args[2];
    //     Args[0] =
    //         m_pOP->GetU32Const((unsigned)OP::OpCode::TempRegLoad); // OpCode
    //     Args[1] = m_pOP->GetU32Const(
    //         DXBC::GetRegIndex(Reg, Comp)); // Linearized register index
    //     Function *F = m_pOP->GetOpFunc(OP::OpCode::TempRegLoad,
    //     pDxbcValueType); Value *pValue = m_pBuilder->CreateCall(F, Args);
    //     pValue = CastDxbcValue(pValue, DxbcValueType, ValueType);
    //     pValue = ApplyOperandModifiers(pValue, O);
    //     OVH.SetValue(pValue);
    //   }
    // } else {
    //   DXASSERT_DXBC(CMask::IsValidDoubleMask(Mask));
    //   for (OperandValueHelper OVH(SrcVal, Mask, O); !OVH.IsDone();
    //        OVH.Advance()) {
    //     BYTE Comp = OVH.GetComp();
    //     Value *pValue1, *pValue2;
    //     {
    //       Value *Args[2];
    //       Args[0] =
    //           m_pOP->GetU32Const((unsigned)OP::OpCode::TempRegLoad); //
    //           OpCode
    //       Args[1] = m_pOP->GetU32Const(
    //           DXBC::GetRegIndex(Reg, Comp)); // Linearized register index1
    //       Function *F = m_pOP->GetOpFunc(OP::OpCode::TempRegLoad,
    //                                      CompType::getU32().GetLLVMType(m_Ctx));
    //       pValue1 = m_pBuilder->CreateCall(F, Args);
    //       Args[1] = m_pOP->GetU32Const(
    //           DXBC::GetRegIndex(Reg, Comp + 1)); // Linearized register
    //           index2
    //       pValue2 = m_pBuilder->CreateCall(F, Args);
    //     }
    //     Value *pValue;
    //     {
    //       Value *Args[3];
    //       Function *F =
    //           m_pOP->GetOpFunc(OP::OpCode::MakeDouble, pDxbcValueType);
    //       Args[0] =
    //           m_pOP->GetU32Const((unsigned)OP::OpCode::MakeDouble); // OpCode
    //       Args[1] = pValue1;                                        // Lo
    //       part Args[2] = pValue2;                                        //
    //       Hi part pValue = m_pBuilder->CreateCall(F, Args); pValue =
    //       ApplyOperandModifiers(pValue, O);
    //     }
    //     OVH.SetValue(pValue);
    //     OVH.Advance();
    //   }
    // }

    // break;
  }

  case D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D);
    DXASSERT_DXBC(O.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
    unsigned Reg = O.m_Index[0].m_RegIndex;
    return SrcOperandIndexableTemp{
      ._ = readSrcOperandCommon(O),
      .regfile = Reg,
      .regindex = readOperandIndex(O.m_Index[1], O.m_IndexType[1])
    };

    // IndexableReg &IRRec = m_IndexableRegs[Reg];
    // Value *pXRegIndex = LoadOperandIndex(O.m_Index[1], O.m_IndexType[1]);
    // Value *pRegIndex =
    //     m_pBuilder->CreateMul(pXRegIndex,
    //     m_pOP->GetI32Const(IRRec.NumComps));
    // CompType DxbcValueType =
    //     DXBC::GetCompTypeFromMinPrec(O.m_MinPrecision, ValueType);
    // if (DxbcValueType.IsBoolTy()) {
    //   DxbcValueType = CompType::getI32();
    // }
    // if (DxbcValueType.GetKind() != CompType::Kind::F64) {
    //   for (OperandValueHelper OVH(SrcVal, Mask, O); !OVH.IsDone();
    //        OVH.Advance()) {
    //     BYTE Comp = OVH.GetComp();
    //     Value *pValue = nullptr;
    //     // Create GEP.
    //     Value *pIndex =
    //         m_pBuilder->CreateAdd(pRegIndex, m_pOP->GetU32Const(Comp));
    //     Value *pGEPIndices[2] = {m_pOP->GetU32Const(0), pIndex};
    //     if (!DxbcValueType.HasMinPrec()) {
    //       Value *pBasePtr = m_IndexableRegs[Reg].pValue32;
    //       Value *pPtr = m_pBuilder->CreateGEP(pBasePtr, pGEPIndices);
    //       pValue = m_pBuilder->CreateAlignedLoad(pPtr, kRegCompAlignment);
    //       pValue = CastDxbcValue(pValue, CompType::getF32(), ValueType);
    //     } else {
    //       // Create GEP.
    //       Value *pBasePtr = m_IndexableRegs[Reg].pValue16;
    //       Value *pPtr = m_pBuilder->CreateGEP(pBasePtr, pGEPIndices);
    //       pValue = m_pBuilder->CreateAlignedLoad(pPtr, kRegCompAlignment /
    //       2); pValue = CastDxbcValue(pValue, CompType::getF16(), ValueType);
    //     }
    //     pValue = ApplyOperandModifiers(pValue, O);
    //     OVH.SetValue(pValue);
    //   }
    // } else {
    //   // Double precision.
    //   for (OperandValueHelper OVH(SrcVal, Mask, O); !OVH.IsDone();
    //        OVH.Advance()) {
    //     BYTE Comp = OVH.GetComp();
    //     Value *pValue = nullptr;
    //     // Create GEP.
    //     Value *pIndex =
    //         m_pBuilder->CreateAdd(pRegIndex, m_pOP->GetU32Const(Comp));
    //     Value *pGEPIndices[1] = {pIndex};
    //     Value *pBasePtr = m_pBuilder->CreateBitCast(
    //         m_IndexableRegs[Reg].pValue32, Type::getDoublePtrTy(m_Ctx));
    //     Value *pPtr = m_pBuilder->CreateGEP(pBasePtr, pGEPIndices);
    //     pValue = m_pBuilder->CreateAlignedLoad(pPtr, kRegCompAlignment * 2);
    //     pValue = ApplyOperandModifiers(pValue, O);
    //     OVH.SetValue(pValue);
    //     OVH.Advance();
    //     OVH.SetValue(pValue);
    //   }
    // }

    // break;
  }

  case D3D10_SB_OPERAND_TYPE_RASTERIZER: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_0D);
    DXASSERT_DXBC(false); // ?
    break;
  }
  default:
    DXASSERT_DXBC(false);
  }
};

auto readSrcOperandResource(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperandResource {}

auto readSrcOperandSampler(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperandSampler {}

auto readSrcOperandUAV(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperandUAV {}

auto readSrcOperandLoadable(const microsoft::D3D10ShaderBinary::COperandBase &O
) {}

auto readInstructionCommon(
  const microsoft::D3D10ShaderBinary::CInstruction &Inst
) -> InstructionCommon {
  return InstructionCommon{.saturate = Inst.m_bSaturate != 0};
};

auto readInstruction(const microsoft::D3D10ShaderBinary::CInstruction &Inst)
  -> Instruction {
  using namespace microsoft;
  switch (Inst.m_OpCode) {
  case microsoft::D3D10_SB_OPCODE_DERIV_RTX:
  case microsoft::D3D11_SB_OPCODE_DERIV_RTX_FINE: {
    return InstPartialDerivative{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
      .ddy = false,
      .coarse = false,
    };
  };
  case microsoft::D3D10_SB_OPCODE_DERIV_RTY:
  case microsoft::D3D11_SB_OPCODE_DERIV_RTY_FINE: {
    return InstPartialDerivative{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
      .ddy = true,
      .coarse = false,
    };
  };
  case microsoft::D3D11_SB_OPCODE_DERIV_RTX_COARSE: {
    return InstPartialDerivative{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
      .ddy = false,
      .coarse = true,
    };
  };
  case microsoft::D3D11_SB_OPCODE_DERIV_RTY_COARSE: {
    return InstPartialDerivative{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
      .ddy = true,
      .coarse = true,
    };
  };
  case microsoft::D3D10_1_SB_OPCODE_LOD: {
    return InstCalcLOD{
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src_address = readSrcOperand(Inst.m_Operands[1]),
      .src_resource = readSrcOperandResource(Inst.m_Operands[2]),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3]),
    };
  };
  default:
    DXASSERT_DXBC(false);
  }
};

auto readCondition(
  const microsoft::D3D10ShaderBinary::CInstruction &Inst, uint32_t OpIdx
) {
  using namespace microsoft;
  const microsoft::D3D10ShaderBinary::COperandBase &O = Inst.m_Operands[OpIdx];
  D3D10_SB_INSTRUCTION_TEST_BOOLEAN TestType = Inst.m_Test;

  return BasicBlockCondition{
    .operand = readSrcOperand(O),
    .test_nonzero = TestType == D3D10_SB_INSTRUCTION_TEST_NONZERO
  };
}

void convertDXBC(
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
  };

  bool sm_ver_5_1_ = CodeParser.ShaderMajorVersion() == 5 &&
                     CodeParser.ShaderMinorVersion() >= 1;
  auto shader_type = CodeParser.ShaderType();

  air::ArgumentBufferBuilder binding_table;
  air::FunctionSignatureBuilder<> func_signature;
  uint32_t binding_table_index = 0;

  std::function<std::shared_ptr<BasicBlock>(
    const std::shared_ptr<BasicBlock> &ctx,
    const std::shared_ptr<BasicBlock> &block_after_endif,
    const std::shared_ptr<BasicBlock> &continue_point,
    const std::shared_ptr<BasicBlock> &break_point,
    const std::shared_ptr<BasicBlock> &return_point,
    std::shared_ptr<BasicBlockSwitch> &switch_context
  )>
    readControlFlow;

  auto null_bb = std::make_shared<BasicBlock>(nullptr);
  auto null_switch_context = std::make_shared<BasicBlockSwitch>(nullptr);

  auto shader_info = std::make_shared<ShaderInfo>();

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
          // R.SetKind(DXBC::GetResourceKind(Inst.m_ResourceDecl.Dimension));
          // const unsigned kTypedBufferElementSizeInBytes = 4;
          // R.SetElementStride(kTypedBufferElementSizeInBytes);
          // R.SetSampleCount(Inst.m_ResourceDecl.SampleCount);
          // CompType DeclCT =
          //     DXBC::GetDeclResCompType(Inst.m_ResourceDecl.ReturnType[0]);
          // if (DeclCT.IsInvalid())
          //   DeclCT = CompType::getU32();
          // R.SetCompType(DeclCT);
          // pResType = GetTypedResElemType(DeclCT);
          break;
        }
        case D3D11_SB_OPCODE_DCL_RESOURCE_RAW: {
          srv.range.space = (Inst.m_RawSRVDecl.Space);
          // R.SetKind(DxilResource::Kind::RawBuffer);
          // const unsigned kRawBufferElementSizeInBytes = 1;
          // R.SetElementStride(kRawBufferElementSizeInBytes);
          // pResType = GetTypedResElemType(CompType::getU32());
          break;
        }
        case D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED: {
          srv.range.space = (Inst.m_StructuredSRVDecl.Space);
          // R.SetKind(DxilResource::Kind::StructuredBuffer);
          // unsigned Stride = Inst.m_StructuredSRVDecl.ByteStride;
          // R.SetElementStride(Stride);
          // pResType = GetStructResElemType(Stride);
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
          // R.SetKind(DXBC::GetResourceKind(Inst.m_TypedUAVDecl.Dimension));
          // const unsigned kTypedBufferElementSizeInBytes = 4;
          // R.SetElementStride(kTypedBufferElementSizeInBytes);
          // CompType DeclCT =
          //     DXBC::GetDeclResCompType(Inst.m_TypedUAVDecl.ReturnType[0]);
          // if (DeclCT.IsInvalid())
          //   DeclCT = CompType::getU32();
          // R.SetCompType(DeclCT);
          // pResType = GetTypedResElemType(DeclCT);
          break;
        }
        case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW: {
          uav.range.space = (Inst.m_RawUAVDecl.Space);
          // R.SetKind(DxilResource::Kind::RawBuffer);
          Flags = Inst.m_RawUAVDecl.Flags;
          // const unsigned kRawBufferElementSizeInBytes = 1;
          // R.SetElementStride(kRawBufferElementSizeInBytes);
          // pResType = GetTypedResElemType(CompType::getU32());
          break;
        }
        case D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED: {
          uav.range.space = (Inst.m_StructuredUAVDecl.Space);
          // R.SetKind(DxilResource::Kind::StructuredBuffer);
          Flags = Inst.m_StructuredUAVDecl.Flags;
          // unsigned Stride = Inst.m_StructuredUAVDecl.ByteStride;
          // R.SetElementStride(Stride);
          // pResType = GetStructResElemType(Stride);
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
      case D3D10_SB_OPCODE_DCL_INPUT_SIV:
      case D3D10_SB_OPCODE_DCL_INPUT_SGV:
      case D3D10_SB_OPCODE_DCL_INPUT: {
        break;
      }
      case D3D10_SB_OPCODE_DCL_INPUT_PS_SIV:
      case D3D10_SB_OPCODE_DCL_INPUT_PS_SGV:
      case D3D10_SB_OPCODE_DCL_INPUT_PS: {
        break;
      }
      case D3D10_SB_OPCODE_DCL_OUTPUT_SIV:
      case D3D10_SB_OPCODE_DCL_OUTPUT_SGV:
      case D3D10_SB_OPCODE_DCL_OUTPUT: {
        break;
      }
      case D3D10_SB_OPCODE_CUSTOMDATA: {
        if (Inst.m_CustomData.Type == D3D10_SB_CUSTOMDATA_DCL_IMMEDIATE_CONSTANT_BUFFER) {
          unsigned Size = Inst.m_CustomData.DataSizeInBytes >> 2;
          DXASSERT_DXBC(Inst.m_CustomData.DataSizeInBytes == Size * 4);
          shader_info->immConstantBufferData.assign(
            (uint32_t *)Inst.m_CustomData.pData,
            ((uint32_t *)Inst.m_CustomData.pData) + Size
          );
        }
        break;
      }
      case D3D10_SB_OPCODE_DCL_INDEX_RANGE:
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
        ctx->instructions.push_back(readInstruction(Inst));
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

  // then we can start build ... real IR code (visit all basicblocks)

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
};
} // namespace dxmt::dxbc