#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

#include "../dxbc_instructions.hpp"
#include "adt.hpp"
#include "air_builder.hpp"
#include "tl/generator.hpp"

namespace dxmt::dxbc {

using mask_t = uint32_t;

constexpr mask_t kMaskAll = 0b1111;
constexpr mask_t kMaskComponentX = 0b1;
constexpr mask_t kMaskComponentY = 0b10;
constexpr mask_t kMaskComponentZ = 0b100;
constexpr mask_t kMaskComponentW = 0b1000;
constexpr mask_t kMaskVecXY = 0b11;
constexpr mask_t kMaskVecXYZ = 0b111;

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
  LoadOperandIndex(const uint32_t &SrcOpIndex) {
    return ir.getInt32(SrcOpIndex);
  }
  llvm::Value *LoadOperandIndex(const IndexByIndexableTempComponent &SrcOpIndex);
  llvm::Value *LoadOperandIndex(const IndexByTempComponent &SrcOpIndex);

  llvm::Value *
  LoadOperandIndex(const OperandIndex &SrcOpIndex) {
    return std::visit([this](auto &SrcOpIndex) { return this->LoadOperandIndex(SrcOpIndex); }, SrcOpIndex);
  }
  llvm::Value *LoadOperand(const SrcOperandConstantBuffer &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandImmediateConstantBuffer &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandImmediate32 &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandInput &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandIndexableInput &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandTemp &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandAttribute &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandIndexableTemp &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandInputICP &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandInputPC &SrcOp, mask_t Mask);
  llvm::Value *LoadOperand(const SrcOperandInputOCP &SrcOp, mask_t Mask);

  llvm::Value *
  LoadOperand(const SrcOperand &SrcOp, mask_t Mask) {
    return std::visit([Mask, this](auto &SrcOp) { return this->LoadOperand(SrcOp, Mask); }, SrcOp);
  }

  llvm::Value *ApplySrcModifier(SrcOperandCommon C, llvm::Value *Value, mask_t Mask);

  /* Store Operands */

  void
  StoreOperand(const DstOperandNull &DstOp, llvm::Value *Value) {
    // NOP
  }
  void
  StoreOperand(const DstOperandSideEffect &DstOp, llvm::Value *Value) {
    // NOP
  }
  void StoreOperand(const DstOperandOutput &DstOp, llvm::Value *Value);
  void StoreOperand(const DstOperandOutputCoverageMask &DstOp, llvm::Value *Value);
  void StoreOperand(const DstOperandOutputDepth &DstOp, llvm::Value *Value);
  void StoreOperand(const DstOperandIndexableOutput &DstOp, llvm::Value *Value);
  void StoreOperand(const DstOperandTemp &DstOp, llvm::Value *Value);
  void StoreOperand(const DstOperandIndexableTemp &DstOp, llvm::Value *Value);
  void StoreOperandHull(const DstOperandOutput &DstOp, llvm::Value *Value);
  void StoreOperandHull(const DstOperandIndexableOutput &DstOp, llvm::Value *Value);

  void
  StoreOperand(const DstOperand &DstOp, llvm::Value *Value) {
    std::visit([Value, this](auto &DstOp) { this->StoreOperand(DstOp, Value); }, DstOp);
  }

  void
  StoreOperandVec4(const DstOperand &DstOp, llvm::Value *ValueVec4) {
    std::visit(
        [ValueVec4, this](auto &DstOp) { this->StoreOperand(DstOp, this->MaskSwizzle(ValueVec4, DstOp._.mask)); }, DstOp
    );
  }

  /* Utils */

  bool
  IsNull(const DstOperand &DstOp) {
    return std::visit(patterns{[](DstOperandNull &) { return true; }, [](auto &) { return false; }}, DstOp);
  }

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
  llvm::Value *ZExtAndBitcastToInt32(llvm::Value *Value);
  llvm::Value *ZExtAndBitcastToFloat(llvm::Value *Value);

  llvm::Value *MaskSwizzle(llvm::Value *Value, mask_t Mask, Swizzle Swizzle = swizzle_identity);

  llvm::Value *
  ExtractElement(llvm::Value *MaybeVector, uint64_t Element) {
    if (llvm::isa<llvm::VectorType>(MaybeVector->getType())) {
      return ir.CreateExtractElement(MaybeVector, Element);
    }
    return MaybeVector;
  }

  llvm::Value *
  VectorSplat(uint32_t ElementCount, llvm::Value *MaybeScaler) {
    if (auto VecTy = llvm::dyn_cast<llvm::FixedVectorType>(MaybeScaler->getType())) {
      if (VecTy->getNumElements() == ElementCount) {
        return MaybeScaler;
      }
      assert(0 && "invalid vector type");
    }
    return ir.CreateVectorSplat(ElementCount, MaybeScaler);
  }

  tl::generator<std::pair<uint32_t, uint32_t>>
  EnumerateComponents(mask_t Mask) {
    Mask &= kMaskAll;
    unsigned Index = 0;
    while (Mask) {
      co_yield std::pair{__builtin_ctz(Mask), Index++};
      Mask &= Mask - 1; // clear lowest set bit
    }
  }

private:
  llvm::air::AIRBuilder &air;
  llvm::IRBuilderBase &ir;

  /* Legacy */
  context &ctx;
  io_binding_map &res;
};

} // namespace dxmt::dxbc