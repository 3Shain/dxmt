
#pragma once
#include "air_type.hpp"
#include "llvm/IR/Constant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include <cstdint>

namespace dxmt::air {
// IRBuilder wrapper with helper functions
class AirBuilder {
public:
  AirBuilder(AirType &types, llvm::IRBuilder<> &inner)
      : builder(inner), types(types) {}

  llvm::Value *CreateRegisterLoad(llvm::Value *regs, llvm::Type *regsType,
                                  uint32_t reg);

  llvm::Value *CreateRegisterLoad(llvm::Value *regs, llvm::Type *regsType,
                                  llvm::Value *reg);

  llvm::Value *CreateGenericRegisterLoad(llvm::Value *regs,
                                         llvm::Type *regsType, uint32_t reg,
                                         llvm::Type *desiredType);

  llvm::Value *CreateGenericRegisterLoad(llvm::Value *regs,
                                         llvm::Type *regsType, uint32_t reg,
                                         llvm::Type *desiredType,
                                         const std::array<uint8_t, 4> &swizzle);

  void CreateRegisterStore(llvm::Value *regs, llvm::Type *regsType,
                           uint32_t reg, llvm::Value *value, uint32_t writeMask,
                           const std::array<uint8_t, 4> &swizzle);

  void CreateRegisterStore(llvm::Value *regs, llvm::Type *regsType,
                           llvm::Value *reg, llvm::Value *value,
                           uint32_t writeMask,
                           const std::array<uint8_t, 4> &swizzle);

  // if value is not a vector of 4 element, we add padding to the right
  void CreateGenericRegisterStore(llvm::Value *regs, llvm::Type *regsType,
                                  uint32_t reg, llvm::Value *value,
                                  uint32_t writeMask);

  llvm::Value *CreateShuffleSwizzleMask(uint32_t writeMask,
                                        const std::array<uint8_t, 4> &swizzle);

  llvm::Constant *CreateConstant(uint32_t value);
  llvm::Constant *CreateConstant(float value);

  llvm::Constant *CreateConstantVector(const std::array<float, 4> &float4);
  llvm::Constant *CreateConstantVector(const std::array<uint32_t, 4> &uint4);

  std::array<uint8_t, 4> MaskToSwizzle(uint32_t writeMask) {
    std::array<uint8_t, 4> mask = {0, 1, 2, 3}; // identity selection
    unsigned checked_component = 0;
    if (writeMask & 1) {
      mask[checked_component] = 0;
      checked_component++;
    }
    if (writeMask & 2) {
      mask[checked_component] = 1;
      checked_component++;
    }
    if (writeMask & 4) {
      mask[checked_component] = 2;
      checked_component++;
    }
    if (writeMask & 8) {
      mask[checked_component] = 3;
      checked_component++;
    }
    return mask;
  };

  llvm::IRBuilder<> &builder;

private:
  AirType &types;
};

} // namespace dxmt