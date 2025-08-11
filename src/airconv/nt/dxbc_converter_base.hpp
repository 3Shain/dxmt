#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

#include "../dxbc_instructions.hpp"
#include "adt.hpp"
#include "air_builder.hpp"

namespace dxmt::dxbc {

using mask_t = uint32_t;

struct io_binding_map;
struct context;

class Converter {
public:
  Converter(llvm::air::AIRBuilder &air, context &ctx_legacy, io_binding_map &res_legacy) :
      air(air),
      ir(air.builder),
      ctx(ctx_legacy),
      res(res_legacy) {}

  /* Load Operands */

  llvm::Value *
  LoadOperandIndex(uint32_t &SrcOpIndex) {
    return ir.getInt32(SrcOpIndex);
  }
  llvm::Value *LoadOperandIndex(IndexByIndexableTempComponent &SrcOpIndex);
  llvm::Value *LoadOperandIndex(IndexByTempComponent &SrcOpIndex);

  llvm::Value *
  LoadOperandIndex(OperandIndex &SrcOpIndex) {
    return std::visit([this](auto &SrcOpIndex) { return this->LoadOperandIndex(SrcOpIndex); }, SrcOpIndex);
  }
  llvm::Value *LoadOperand(SrcOperandConstantBuffer &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandImmediateConstantBuffer &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandImmediate32 &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandInput &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandIndexableInput &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandTemp &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandAttribute &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandIndexableTemp &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandInputICP &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandInputPC &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(SrcOperandInputOCP &SrcOp, mask_t Mask);

  llvm::Value *
  LoadOperand(SrcOperand &SrcOp, mask_t Mask) {
    return std::visit([Mask, this](auto &SrcOp) { return this->LoadOperand(SrcOp, Mask); }, SrcOp);
  }

  llvm::Value *ApplySrcModifier(SrcOperandCommon C, llvm::Value *Value, mask_t Mask);

  /* Utils */

  // bool
  // IsNull(DstOperand &DstOp) {
  //   return std::visit(patterns{[](DstOperandNull &) { return true; }, [](auto &) { return false; }}, DstOp);
  // }

  int32_t
  ComponentFromScalarMask(mask_t Mask, Swizzle Swizzle) {
    switch (Mask) {
    case 0b1:
      return Swizzle[0];
    case 0b10:
      return Swizzle[1];
    case 0b100:
      return Swizzle[2];
    case 0b1000:
      return Swizzle[3];
    default:
      break;
    }
    return -1;
  }

  llvm::Value *BitcastToFloat(llvm::Value *Value);
  llvm::Value *BitcastToInt32(llvm::Value *Value);
  llvm::Value *TruncAndBitcastToHalf(llvm::Value *Value);

  llvm::Value *MaskSwizzle(llvm::Value *Value, mask_t Mask, Swizzle Swizzle = swizzle_identity);

private:
  llvm::air::AIRBuilder &air;
  llvm::IRBuilderBase &ir;

  /* Legacy */
  context &ctx;
  io_binding_map &res;
};

} // namespace dxmt::dxbc