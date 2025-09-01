#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

#include "../dxbc_instructions.hpp"
#include "adt.hpp"
#include "air_builder.hpp"
#include "tl/generator.hpp"
#include "ftl.hpp"

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

struct TextureResourceHandle {
  llvm::air::Texture Texture;
  llvm::air::Texture::ResourceKind Logical;
  llvm::Value *Handle;
  llvm::Value *Metadata;
  Swizzle Swizzle;
};

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

  llvm::Optional<TextureResourceHandle> LoadTexture(const SrcOperandResource &SrcOp);
  llvm::Optional<TextureResourceHandle> LoadTexture(const SrcOperandUAV &SrcOp);
  llvm::Optional<TextureResourceHandle> LoadTexture(const AtomicDstOperandUAV &DstOp);

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
  StoreOperand(const DstOperand &DstOp, llvm::Value *Value, bool Saturate = false) {
    std::visit(
        [=, this](auto &DstOp) {
          auto Value_ = Value;
          if (DstOp._.write_type == OperandDataType::Float && Saturate)
            Value_ = air.CreateFPUnOp(llvm::air::AIRBuilder::saturate, Value);
          this->StoreOperand(DstOp, Value_);
        },
        DstOp
    );
  }

  void
  StoreOperandVec4(const DstOperand &DstOp, llvm::Value *ValueVec4, bool Saturate = false) {
    std::visit(
        [=, this](auto &DstOp) {
          auto Value_ = ValueVec4;
          if (DstOp._.write_type == OperandDataType::Float && Saturate)
            Value_ = air.CreateFPUnOp(llvm::air::AIRBuilder::saturate, ValueVec4);
          this->StoreOperand(DstOp, this->MaskSwizzle(Value_, DstOp._.mask));
        },
        DstOp
    );
  }

  /* Instructions */

  void
  operator()(const InstNop &) {
    // just do nothing
  }

  void
  operator()(const InstPixelDiscard &) {
    air.CreateDiscard();
  }

  void
  operator()(const InstSync &sync) {
    using namespace llvm::air;
    MemFlags mem_flag = sync.tgsm_memory_barrier ? MemFlags::Threadgroup : MemFlags::None;
    if (sync.uav_boundary != InstSync::UAVBoundary::none) {
      mem_flag |= MemFlags::Device | MemFlags::Texture;
    }
    air.CreateBarrier(mem_flag);
  }

  void operator()(const InstMov &);
  void operator()(const InstMovConditional &);
  void operator()(const InstSwapConditional &);
  void operator()(const InstDotProduct &);
  void operator()(const InstFloatUnaryOp &);
  void operator()(const InstFloatBinaryOp &);
  void operator()(const InstIntegerUnaryOp &);
  void operator()(const InstIntegerBinaryOp &);
  void operator()(const InstPartialDerivative &);

  void operator()(const InstConvert &);

  void operator()(const InstFloatCompare &);
  void operator()(const InstFloatMAD &);
  void operator()(const InstSinCos &);
  void operator()(const InstIntegerCompare &);
  void operator()(const InstIntegerMAD &);
  void operator()(const InstIntegerBinaryOpWithTwoDst &);

  void operator()(const InstExtractBits &);
  void operator()(const InstBitFiledInsert &);

  void operator()(const InstLoad &);
  void operator()(const InstLoadUAVTyped &);
  void operator()(const InstStoreUAVTyped &);

  /* Utils */

  bool
  IsNull(const DstOperand &DstOp) {
    return std::visit(patterns{[](DstOperandNull &) { return true; }, [](auto &) { return false; }}, DstOp);
  }

  mask_t
  GetMask(const DstOperand &DstOp) {
    return std::visit(
        patterns{
            [](DstOperandNull) { return (uint32_t)0b1111; },
            [](DstOperandSideEffect) { return (uint32_t)0b1111; },
            [](auto dst) { return dst._.mask; },
        },
        DstOp
    );
  }

  llvm::Value *
  MaxIfInMask(uint64_t Mask, llvm::Value *Value) {
    llvm::Value *Max = llvm::ConstantInt::getAllOnesValue(Value->getType());
    return ir.CreateSelect(ir.CreateIsNull(ir.CreateAnd(Value, Mask)), Value, Max);
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

  llvm::Value *
  ExtractFromCombinedMask(llvm::Value *Value, mask_t CombinedMask, mask_t Mask) {
    if (Mask == CombinedMask)
      return Value;

    llvm::SmallVector<int, 4> Idx;
    unsigned SrcIdx = 0;
    for (unsigned Bit = 0; Bit < 4; ++Bit) {
      if (CombinedMask & (1 << Bit)) {
        if (Mask & (1 << Bit))
          Idx.push_back(SrcIdx);
        SrcIdx++;
      }
    }

    return ir.CreateShuffleVector(Value, Idx);
  }

  llvm::Value *
  DecodeTextureBufferOffset(llvm::Value *Metadata) {
    return ir.CreateTrunc(Metadata, air.getIntTy());
  }

private:
  llvm::air::AIRBuilder &air;
  llvm::IRBuilderBase &ir;

  /* Legacy */
  context &ctx;
  io_binding_map &res;
};

} // namespace dxmt::dxbc