#pragma once

#include "dxbc_converter.hpp"

#include "DXBCParser/ShaderBinary.h"
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "air_signature.hpp"
#include "shader_common.hpp"
#include "llvm/Support/raw_ostream.h"

// it's suposed to be include by specific file
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
// NOLINTBEGIN(misc-definitions-in-headers)

namespace dxmt::dxbc {

#define DXASSERT_DXBC(x) assert(x);

air::Interpolation
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

dxmt::shader::common::ResourceType
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
    break;
  }
  assert(0 && "invalid D3D10_SB_RESOURCE_DIMENSION");
};

dxmt::shader::common::ScalerDataType
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
  case microsoft::D3D11_SB_RETURN_TYPE_DOUBLE:
  case microsoft::D3D11_SB_RETURN_TYPE_CONTINUED:
  case microsoft::D3D11_SB_RETURN_TYPE_UNUSED:
    break;
  }
  assert(0 && "invalid D3D10_SB_RESOURCE_RETURN_TYPE");
}

static auto readOperandRelativeIndex(
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

static auto readOperandIndex(
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

static auto readDstOperand(const microsoft::D3D10ShaderBinary::COperandBase &O)
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

static auto
readSrcOperandSwizzle(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> Swizzle {
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

static auto
readSrcOperandCommon(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperandCommon {
  return SrcOperandCommon{
    .swizzle = readSrcOperandSwizzle(O),
    .abs = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_ABS) != 0,
    .neg = (O.m_Modifier & microsoft::D3D10_SB_OPERAND_MODIFIER_NEG) != 0,
  };
}

static auto readSrcOperand(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperand {
  using namespace microsoft;
  switch (O.m_Type) {
  case D3D10_SB_OPERAND_TYPE_IMMEDIATE32: {

    DXASSERT_DXBC(O.m_Modifier == D3D10_SB_OPERAND_MODIFIER_NONE);

    if (O.m_NumComponents == D3D10_SB_OPERAND_4_COMPONENT) {
      return SrcOperandImmediate32{
        ._ =
          {
            .swizzle = swizzle_identity,
            .abs = false,
            .neg = false,
          },
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
        ._ =
          {
            .swizzle = swizzle_identity,
            .abs = false,
            .neg = false,
          },
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
  case D3D10_SB_OPERAND_TYPE_INPUT: {
    DXASSERT_DXBC(O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D);
    unsigned Reg = O.m_Index[0].m_RegIndex;
    return SrcOperandInput{
      ._ = readSrcOperandCommon(O),
      .regid = Reg,
    };
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
  case D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER: {
    if (O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_2D) {
      return SrcOperandConstantBuffer{
        ._ = readSrcOperandCommon(O),
        .rangeid = O.m_Index[0].m_RegIndex,
        .rangeindex = readOperandIndex(O.m_Index[0], O.m_IndexType[0]),
        .regindex = readOperandIndex(O.m_Index[1], O.m_IndexType[1])
      };
    }
    assert(0 && "TODO: SM5.1");
  }
  default:
    DXASSERT_DXBC(false && "unhandled src operand");
  }
};

static auto
readSrcOperandResource(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperandResource {
  using namespace microsoft;
  assert(O.m_Type == D3D10_SB_OPERAND_TYPE_RESOURCE);
  if (O.m_IndexType[0] == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {
    if (O.m_IndexDimension == D3D10_SB_OPERAND_INDEX_1D) {
      return SrcOperandResource{
        .range_id = O.m_Index[0].m_RegIndex,
        .index = readOperandIndex(O.m_Index[0], O.m_IndexType[0]),
        .read_swizzle = readSrcOperandSwizzle(O)
      };
    } else {
      return SrcOperandResource{
        .range_id = O.m_Index[0].m_RegIndex,
        .index = readOperandIndex(O.m_Index[1], O.m_IndexType[1]),
        .read_swizzle = readSrcOperandSwizzle(O)
      };
    }
  } else {
    assert(0 && "interface resource is not supported yet");
  }
}

static auto
readSrcOperandSampler(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperandSampler {
  if (O.m_IndexDimension == microsoft::D3D10_SB_OPERAND_INDEX_1D) {
    DXASSERT_DXBC(
      O.m_IndexType[0] == microsoft::D3D10_SB_OPERAND_INDEX_IMMEDIATE32
    );
    return SrcOperandSampler{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[0], O.m_IndexType[0])
    };
  } else {
    return SrcOperandSampler{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[1], O.m_IndexType[1])
    };
  }
}

static auto
readSrcOperandUAV(const microsoft::D3D10ShaderBinary::COperandBase &O)
  -> SrcOperandUAV {
  if (O.m_IndexDimension == microsoft::D3D10_SB_OPERAND_INDEX_1D) {
    DXASSERT_DXBC(
      O.m_IndexType[0] == microsoft::D3D10_SB_OPERAND_INDEX_IMMEDIATE32
    );
    return SrcOperandUAV{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[0], O.m_IndexType[0])
    };
  } else {
    return SrcOperandUAV{
      .range_id = O.m_Index[0].m_RegIndex,
      .index = readOperandIndex(O.m_Index[1], O.m_IndexType[1])
    };
  }
}

static auto
readSrcOperandLoadable(const microsoft::D3D10ShaderBinary::COperandBase &O) {}

static auto
readInstructionCommon(const microsoft::D3D10ShaderBinary::CInstruction &Inst)
  -> InstructionCommon {
  assert(Inst.m_bSaturate == 0);
  return InstructionCommon{.saturate = Inst.m_bSaturate != 0};
};

static auto readInstruction(
  const microsoft::D3D10ShaderBinary::CInstruction &Inst,
  ShaderInfo &shader_info
) -> Instruction {
  using namespace microsoft;
  switch (Inst.m_OpCode) {
  case microsoft::D3D10_SB_OPCODE_MOV: {
    return InstMov{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_SAMPLE: {
    auto inst = InstSample{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src_address = readSrcOperand(Inst.m_Operands[1]),
      .src_resource = readSrcOperandResource(Inst.m_Operands[2]),
      .src_sampler = readSrcOperandSampler(Inst.m_Operands[3]),
      .offsets =
        {Inst.m_TexelOffset[0], Inst.m_TexelOffset[1], Inst.m_TexelOffset[2]},
    };
    shader_info.srvMap[inst.src_resource.range_id].sampled = true;
    return inst;
  };

  case microsoft::D3D10_SB_OPCODE_DP2: {
    return InstDotProduct{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
      .dimension = 2,
    };
  };
  case microsoft::D3D10_SB_OPCODE_DP3: {
    return InstDotProduct{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
      .dimension = 3,
    };
  };
  case microsoft::D3D10_SB_OPCODE_DP4: {
    return InstDotProduct{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
      .dimension = 4,
    };
  };
  case microsoft::D3D10_SB_OPCODE_RSQ: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::Rsq,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D11_SB_OPCODE_RCP: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::Rcp,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_LOG: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::Log2,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_EXP: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::Exp2,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_SQRT: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::Sqrt,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ROUND_Z: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::RoundZero,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ROUND_PI: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::RoundPositiveInf,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ROUND_NI: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::RoundNegativeInf,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ROUND_NE: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::RoundNearestEven,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_FRC: {
    return InstFloatUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatUnaryOp::Fraction,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  }
  case microsoft::D3D10_SB_OPCODE_ADD: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatBinaryOp::Add,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MUL: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatBinaryOp::Mul,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_DIV: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatBinaryOp::Div,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MAX: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatBinaryOp::Max,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MIN: {
    return InstFloatBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = FloatBinaryOp::Min,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_MAD: {
    return InstFloatMAD{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
      .src2 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IMAD: {
    return InstSignedMAD{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
      .src2 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UMAD: {
    return InstUnsignedMAD{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
      .src2 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
  case microsoft::D3D11_1_SB_OPCODE_MSAD: {
    return InstMaskedSumOfAbsDiff{
      ._ = readInstructionCommon(Inst),
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
      .src2 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_SINCOS: {
    return InstSinCos{
      ._ = readInstructionCommon(Inst),
      .dst_sin = readDstOperand(Inst.m_Operands[0]),
      .dst_cos = readDstOperand(Inst.m_Operands[1]),
      .src = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_EQ: {
    return InstFloatCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = FloatComparison::Equal,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_GE: {
    return InstFloatCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = FloatComparison::GreaterEqual,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_LT: {
    return InstFloatCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = FloatComparison::LessThan,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_NE: {
    return InstFloatCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = FloatComparison::NotEqual,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IEQ: {
    return InstIntegerCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = IntegerComparison::Equal,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_INE: {
    return InstIntegerCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = IntegerComparison::NotEqual,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IGE: {
    return InstIntegerCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = IntegerComparison::SignedGreaterEqual,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_ILT: {
    return InstIntegerCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = IntegerComparison::SignedLessThan,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UGE: {
    return InstIntegerCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = IntegerComparison::UnsignedGreaterEqual,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_ULT: {
    return InstIntegerCompare{
      ._ = readInstructionCommon(Inst),
      .cmp = IntegerComparison::UnsignedLessThan,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_NOT: {
    return InstIntegerUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerUnaryOp::Not,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_INEG: {
    return InstIntegerUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerUnaryOp::Neg,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  };
  case microsoft::D3D11_SB_OPCODE_BFREV: {
    return InstIntegerUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerUnaryOp::ReverseBits,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  };
  case microsoft::D3D11_SB_OPCODE_COUNTBITS: {
    return InstIntegerUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerUnaryOp::CountBits,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  };
  case microsoft::D3D11_SB_OPCODE_FIRSTBIT_HI: {
    return InstIntegerUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerUnaryOp::FirstHiBit,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  };
  case microsoft::D3D11_SB_OPCODE_FIRSTBIT_SHI: {
    return InstIntegerUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerUnaryOp::FirstHiBitSigned,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  };
  case microsoft::D3D11_SB_OPCODE_FIRSTBIT_LO: {
    return InstIntegerUnaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerUnaryOp::FirstLowBit,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src = readSrcOperand(Inst.m_Operands[1]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_ISHL: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::IShl,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_ISHR: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::IShr,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_USHR: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::UShr,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_XOR: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::Xor,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_OR: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::Or,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_AND: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::And,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UMIN: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::UMin,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UMAX: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::UMax,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IMIN: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::IMin,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IMAX: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::IMax,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IADD: {
    return InstIntegerBinaryOp{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOp::Add,
      .dst = readDstOperand(Inst.m_Operands[0]),
      .src0 = readSrcOperand(Inst.m_Operands[1]),
      .src1 = readSrcOperand(Inst.m_Operands[2]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_IMUL: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOpWithTwoDst::IMul,
      .dst_hi = readDstOperand(Inst.m_Operands[0]),
      .dst_low = readDstOperand(Inst.m_Operands[1]),
      .src0 = readSrcOperand(Inst.m_Operands[2]),
      .src1 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UMUL: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOpWithTwoDst::UMul,
      .dst_hi = readDstOperand(Inst.m_Operands[0]),
      .dst_low = readDstOperand(Inst.m_Operands[1]),
      .src0 = readSrcOperand(Inst.m_Operands[2]),
      .src1 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
  case microsoft::D3D10_SB_OPCODE_UDIV: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOpWithTwoDst::UDiv,
      .dst_hi = readDstOperand(Inst.m_Operands[0]),
      .dst_low = readDstOperand(Inst.m_Operands[1]),
      .src0 = readSrcOperand(Inst.m_Operands[2]),
      .src1 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
  case microsoft::D3D11_SB_OPCODE_UADDC: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOpWithTwoDst::UAddCarry,
      .dst_hi = readDstOperand(Inst.m_Operands[0]),
      .dst_low = readDstOperand(Inst.m_Operands[1]),
      .src0 = readSrcOperand(Inst.m_Operands[2]),
      .src1 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
  case microsoft::D3D11_SB_OPCODE_USUBB: {
    return InstIntegerBinaryOpWithTwoDst{
      ._ = readInstructionCommon(Inst),
      .op = IntegerBinaryOpWithTwoDst::USubBorrow,
      .dst_hi = readDstOperand(Inst.m_Operands[0]),
      .dst_low = readDstOperand(Inst.m_Operands[1]),
      .src0 = readSrcOperand(Inst.m_Operands[2]),
      .src1 = readSrcOperand(Inst.m_Operands[3]),
    };
  };
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
  case microsoft::D3D10_SB_OPCODE_NOP: {
    return InstNop{};
  };
  default: {
    llvm::outs() << "unhandled dxbc instruction " << Inst.OpCode() << "\n";
    assert(0 && "unhandled dxbc instruction");
    // return InstNop{};
  }
  }
};

static auto readCondition(
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
} // namespace dxmt::dxbc

// NOLINTEND(misc-definitions-in-headers)
#pragma clang diagnostic pop