#include "dxbc_converter_base.hpp"
#include "../dxbc_converter.hpp"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"

namespace dxmt::dxbc {

llvm::ArrayType *
GetArrayType(llvm::Value *Array) {
  return llvm::cast<llvm::ArrayType>(llvm::cast<llvm::PointerType>(Array->getType())->getNonOpaquePointerElementType());
}

llvm::Value *
Converter::LoadOperandIndex(IndexByTempComponent &SrcOpIndex) {
  auto Comp = SrcOpIndex.component;
  SrcOperandTemp RelReg{
      ._ = {{Comp, Comp, Comp, Comp}, false, false, OperandDataType::Integer},
      .phase = SrcOpIndex.phase,
      .regid = SrcOpIndex.regid,
  };
  auto Rel = LoadOperand(RelReg, 0b1);
  return ir.CreateAdd(Rel, ir.getInt32(SrcOpIndex.offset));
}

llvm::Value *
Converter::LoadOperandIndex(IndexByIndexableTempComponent &SrcOpIndex) {
  auto Comp = SrcOpIndex.component;
  SrcOperandIndexableTemp RelReg{
      ._ = {{Comp, Comp, Comp, Comp}, false, false, OperandDataType::Integer},
      .phase = SrcOpIndex.phase,
      .regindex = SrcOpIndex.regid,
      .regfile = SrcOpIndex.regfile,
  };
  auto Rel = LoadOperand(RelReg, 0b1);
  return ir.CreateAdd(Rel, ir.getInt32(SrcOpIndex.offset));
}

llvm::Value *
Converter::LoadOperand(SrcOperandConstantBuffer &SrcOp, mask_t Mask) {

  auto RangeId = SrcOp.rangeid;

  auto V = res.cb_range_map[RangeId](nullptr).build(ctx);
  if (auto err = V.takeError()) {
    return ApplySrcModifier(SrcOp._, llvm::ConstantAggregateZero::get(air.getIntTy(4)), Mask);
  }
  auto Handle = V.get();

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(air.getIntTy(4), Handle, {LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyIntVec4, Handle, {LoadOperandIndex(SrcOp.regindex)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandImmediateConstantBuffer &SrcOp, mask_t Mask) {
  auto Handle = res.icb;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandImmediate32 &SrcOp, mask_t Mask) {
  llvm::SmallVector<llvm::Constant *> Constants;
  switch (SrcOp._.read_type) {
  case OperandDataType::Float:
    switch (Mask) {
    case 0b1:
      return air.getFloat(SrcOp.fvalue[0]);
    case 0b10:
      return air.getFloat(SrcOp.fvalue[1]);
    case 0b100:
      return air.getFloat(SrcOp.fvalue[2]);
    case 0b1000:
      return air.getFloat(SrcOp.fvalue[3]);
    default:
      for (int i = 0; i < 4; i++) {
        if (Mask & (1 << i)) {
          Constants.push_back(air.getFloat(SrcOp.fvalue[i]));
        }
      }
      break;
    }
    break;
  case OperandDataType::Integer:
    switch (Mask) {
    case 0b1:
      return air.getInt(SrcOp.uvalue[0]);
    case 0b10:
      return air.getInt(SrcOp.uvalue[1]);
    case 0b100:
      return air.getInt(SrcOp.uvalue[2]);
    case 0b1000:
      return air.getInt(SrcOp.uvalue[3]);
    default:
      for (int i = 0; i < 4; i++) {
        if (Mask & (1 << i)) {
          Constants.push_back(air.getInt(SrcOp.uvalue[i]));
        }
      }
      break;
    }
    break;
  case OperandDataType::Half16X16:
    // in case f16to32 reads a literal (doesn't make much sense)
    switch (Mask) {
    case 0b1:
      return air.getHalf(SrcOp.uvalue[0]);
    case 0b10:
      return air.getHalf(SrcOp.uvalue[1]);
    case 0b100:
      return air.getHalf(SrcOp.uvalue[2]);
    case 0b1000:
      return air.getHalf(SrcOp.uvalue[3]);
    default:
      for (int i = 0; i < 4; i++) {
        if (Mask & (1 << i)) {
          Constants.push_back(air.getHalf(SrcOp.uvalue[i]));
        }
      }
      break;
    }
    break;
  default:
    llvm_unreachable("unhandled operand data type");
  }
  return llvm::ConstantVector::get(Constants);
}

llvm::Value *
Converter::LoadOperand(SrcOperandInput &SrcOp, mask_t Mask) {

  auto Handle = res.input.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), ir.getInt32(SrcOp.regid), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), ir.getInt32(SrcOp.regid)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandTemp &SrcOp, mask_t Mask) {

  auto Handle = res.temp.ptr_int4;

  if (SrcOp.phase != ~0u) {
    Handle = res.phases[SrcOp.phase].temp.ptr_int4;
  }
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), ir.getInt32(SrcOp.regid), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), ir.getInt32(SrcOp.regid)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandAttribute &SrcOp, mask_t Mask) {
  using shader::common::InputAttribute;
  llvm::Value *Ret = llvm::PoisonValue::get(air.getIntTy(4));
  switch (SrcOp.attribute) {
  case InputAttribute::VertexId:
  case InputAttribute::InstanceId:
    assert(0 && "never reached: should be handled separately");
    break;
  case InputAttribute::ThreadId: {
    Ret = res.thread_id_arg;
    break;
  }
  case InputAttribute::ThreadIdInGroup: {
    Ret = res.thread_id_in_group_arg;
    break;
  }
  case InputAttribute::ThreadGroupId: {
    Ret = res.thread_group_id_arg;
    break;
  }
  case InputAttribute::ThreadIdInGroupFlatten: {
    Ret = res.thread_id_in_group_flat_arg;
    break;
  }

  case InputAttribute::OutputControlPointId:
  case InputAttribute::ForkInstanceId:
  case InputAttribute::JoinInstanceId: {
    Ret = res.thread_id_in_patch;
    break;
  }
  case InputAttribute::CoverageMask: {
    Ret = ctx.pso_sample_mask != 0xffffffff ? ir.CreateAnd(res.coverage_mask_arg, ctx.pso_sample_mask)
                                            : res.coverage_mask_arg;
    break;
  }
  case InputAttribute::Domain: {
    Ret = res.domain;
    break;
  }
  case InputAttribute::PrimitiveId: {
    Ret = res.patch_id;
    break;
  }
  case InputAttribute::GSInstanceId: {
    Ret = res.gs_instance_id;
    break;
  }
  }

  return ApplySrcModifier(SrcOp._, Ret, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandIndexableTemp &SrcOp, mask_t Mask) {

  indexable_register_file regfile;

  if (SrcOp.phase != ~0u) {
    regfile = res.phases[SrcOp.phase].indexable_temp_map[SrcOp.regfile];
  } else {
    regfile = res.indexable_temp_map[SrcOp.regfile];
  }

  auto Handle = regfile.ptr_int_vec;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec = air.getIntTy(regfile.vec_size);
  auto Ptr = ir.CreateGEP(
      TyHandle, Handle,
      {
          ir.getInt32(0),
          LoadOperandIndex(SrcOp.regindex),
      }
  );
  auto ValueIntVec = ir.CreateLoad(TyIntVec, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandIndexableInput &SrcOp, mask_t Mask) {

  auto Handle = res.input.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandInputICP &SrcOp, mask_t Mask) {
  /* applies to both hull and domain shader, in this case input is a "2d" array */
  auto Handle = res.input.ptr_int4;
  auto TyHandle = GetArrayType(Handle);
  auto InputElementCount = res.input_element_count;

  auto Offset = ir.CreateAdd(
      ir.CreateMul(LoadOperandIndex(SrcOp.cpid), ir.getInt32(InputElementCount)), ir.getInt32(SrcOp.regid)
  );

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), Offset, ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), Offset});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandInputPC &SrcOp, mask_t Mask) {

  auto Handle = res.patch_constant_output.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(SrcOperandInputOCP &SrcOp, mask_t Mask) {
  /* applies to both hull and domain shader, in this case input is a "2d" array */
  auto Handle = res.output.ptr_int4;
  auto TyHandle = GetArrayType(Handle);
  auto OutputElementCount = res.output_element_count;

  auto Offset = ir.CreateAdd(
      ir.CreateMul(LoadOperandIndex(SrcOp.cpid), ir.getInt32(OutputElementCount)), ir.getInt32(SrcOp.regid)
  );

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), Offset, ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), Offset});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::ApplySrcModifier(SrcOperandCommon C, llvm::Value *Value, mask_t Mask) {
  Value = MaskSwizzle(Value, Mask, C.swizzle);
  switch (C.read_type) {
  case OperandDataType::Float:
    Value = BitcastToFloat(Value);
    if (C.abs)
      Value = ir.CreateUnaryIntrinsic(llvm::Intrinsic::fabs, Value);
    if (C.neg)
      Value = ir.CreateFNeg(Value);
    break;
  case OperandDataType::Integer:
    Value = BitcastToInt32(Value);
    if (C.neg)
      Value = ir.CreateNeg(Value);
    break;
  case OperandDataType::Half16X16:
    Value = TruncAndBitcastToHalf(Value);
    if (C.neg)
      Value = ir.CreateFNeg(Value);
    break;
  }
  return Value;
}

llvm::Value *
Converter::BitcastToFloat(llvm::Value *Value) {
  auto Ty = Value->getType();
  if (Ty->getScalarType()->isFloatTy()) {
    return Value;
  }
  if (auto TyVec = llvm::dyn_cast<llvm::FixedVectorType>(Ty)) {
    return ir.CreateBitCast(Value, air.getFloatTy(TyVec->getNumElements()));
  }
  return ir.CreateBitCast(Value, ir.getFloatTy());
}

llvm::Value *
Converter::BitcastToInt32(llvm::Value *Value) {
  auto Ty = Value->getType();
  if (Ty->getScalarType()->isIntegerTy(32)) {
    return Value;
  }
  if (auto TyVec = llvm::dyn_cast<llvm::FixedVectorType>(Ty)) {
    return ir.CreateBitCast(Value, air.getIntTy(TyVec->getNumElements()));
  }
  return ir.CreateBitCast(Value, ir.getInt32Ty());
}

llvm::Value *
Converter::TruncAndBitcastToHalf(llvm::Value *Value) {
  auto Ty = Value->getType();
  if (Ty->getScalarType()->isHalfTy()) {
    return Value;
  }
  llvm_unreachable("untested path and likely never used, remove this if it has been ever reached");
  if (auto TyVec = llvm::dyn_cast<llvm::FixedVectorType>(Ty)) {
    auto ElementCount = TyVec->getNumElements();
    auto TyShortVec = llvm::FixedVectorType::get(ir.getInt16Ty(), ElementCount);
    if (!Ty->getScalarType()->isIntegerTy(32)) {
      Value = ir.CreateBitCast(Value, air.getIntTy(ElementCount));
    }
    return ir.CreateBitCast(ir.CreateTrunc(Value, TyShortVec), air.getHalfTy(ElementCount));
  }
  if (!Ty->getScalarType()->isIntegerTy(32)) {
    Value = ir.CreateBitCast(Value, ir.getInt32Ty());
  }
  return ir.CreateBitCast(ir.CreateTrunc(Value, ir.getInt16Ty()), air.getHalfTy());
}

llvm::Value *
Converter::MaskSwizzle(llvm::Value *Value, mask_t Mask, Swizzle Swizzle) {
  if (!isa<llvm::FixedVectorType>(Value->getType())) {
    switch (Mask) {
    case 0b1:
    case 0b10:
    case 0b100:
    case 0b1000:
      return Value;
    case 0b1100:
    case 0b0110:
    case 0b0011:
    case 0b1010:
    case 0b0101:
    case 0b1001:
      return ir.CreateVectorSplat(2, Value);
    case 0b1101:
    case 0b1011:
    case 0b1110:
    case 0b0111:
      return ir.CreateVectorSplat(3, Value);
    case 0b1111:
      return ir.CreateVectorSplat(4, Value);
    default:
      break;
    }
    return Value;
  }
  switch (Mask) {
  case 0b1:
    return ir.CreateExtractElement(Value, Swizzle[0]);
  case 0b10:
    return ir.CreateExtractElement(Value, Swizzle[1]);
  case 0b100:
    return ir.CreateExtractElement(Value, Swizzle[2]);
  case 0b1000:
    return ir.CreateExtractElement(Value, Swizzle[3]);
  case 0b1100:
    return ir.CreateShuffleVector(Value, {Swizzle[2], Swizzle[3]});
  case 0b0110:
    return ir.CreateShuffleVector(Value, {Swizzle[1], Swizzle[2]});
  case 0b0011:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[1]});
  case 0b1010:
    return ir.CreateShuffleVector(Value, {Swizzle[1], Swizzle[3]});
  case 0b0101:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[2]});
  case 0b1001:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[3]});
  case 0b1101:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[2], Swizzle[3]});
  case 0b1011:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[1], Swizzle[3]});
  case 0b1110:
    return ir.CreateShuffleVector(Value, {Swizzle[1], Swizzle[2], Swizzle[3]});
  case 0b0111:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[1], Swizzle[2]});
  case 0b1111:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[1], Swizzle[2], Swizzle[3]});
  default:
    break;
  }
  return Value;
}
} // namespace dxmt::dxbc