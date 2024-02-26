#include "./air_builder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Value.h"
#include <array>
#include <cstdint>

using namespace llvm;

namespace dxmt {

constexpr std::array<uint8_t, 4> swizzleXyzw = {0, 1, 2, 3};

Value *AirBuilder::CreateRegisterLoad(llvm::Value *regs, llvm::Type *regsType,
                                      uint32_t reg) {
  assert(regs);
  assert(regsType);
  auto regPtr = builder.CreateConstInBoundsGEP2_32(regsType, regs, 0, reg);
  auto regValue = builder.CreateLoad(types.tyInt32V4, regPtr);

  return regValue;
}

Value *AirBuilder::CreateRegisterLoad(llvm::Value *regs, llvm::Type *regsType,
                                      llvm::Value *reg) {
  assert(regs);
  assert(regsType);
  auto regPtr = builder.CreateInBoundsGEP(regsType, regs,
                                          {CreateConstant((uint32_t)0), reg});
  auto regValue = builder.CreateLoad(types.tyInt32V4, regPtr);

  return regValue;
}

Value *AirBuilder::CreateGenericRegisterLoad(llvm::Value *regs,
                                             llvm::Type *regsType, uint32_t reg,
                                             llvm::Type *desiredType) {
  return CreateGenericRegisterLoad(regs, regsType, reg, desiredType,
                                   swizzleXyzw);
}

Value *
AirBuilder::CreateGenericRegisterLoad(llvm::Value *regs, llvm::Type *regsType,
                                      uint32_t reg, llvm::Type *desiredType,
                                      const std::array<uint8_t, 4> &swizzle) {
  auto valueType = desiredType;
  if (valueType == types.tyInt32V4) {
    return CreateRegisterLoad(regs, regsType, reg);
  }
  auto value = CreateRegisterLoad(regs, regsType, reg);
  if (valueType == types.tyInt32) {
    return builder.CreateExtractElement(value, swizzle[0]);
  }
  if (valueType == types.tyFloat) {
    return builder.CreateBitCast(
        builder.CreateExtractElement(value, swizzle[0]), types.tyFloat);
  }
  if (valueType == types.tyFloatV4) {
    return builder.CreateBitCast(value, types.tyFloatV4);
  }
  if (valueType == types.typeContext["float3"]) {
    return builder.CreateShuffleVector(
        builder.CreateBitCast(value, types.tyFloatV4),
        PoisonValue::get(types.tyFloatV4),
        {swizzle[0], swizzle[1], swizzle[2]});
  }
  if (valueType == types.typeContext["float2"]) {
    return builder.CreateShuffleVector(
        builder.CreateBitCast(value, types.tyFloatV4),
        PoisonValue::get(types.tyFloatV4), {swizzle[0], swizzle[1]});
  }
  if (valueType == types.typeContext["uint3"]) {
    return builder.CreateShuffleVector(value, PoisonValue::get(types.tyInt32V4),
                                       {swizzle[0], swizzle[1], swizzle[2]});
  }
  if (valueType == types.typeContext["uint2"]) {
    return builder.CreateShuffleVector(value, PoisonValue::get(types.tyInt32V4),
                                       {swizzle[0], swizzle[1]});
  }
  assert(0 && "Overload not implemented yet.");
}

void AirBuilder::CreateRegisterStore(llvm::Value *regs, llvm::Type *regsType,
                                     uint32_t reg, llvm::Value *value,
                                     uint32_t writeMask,
                                     const std::array<uint8_t, 4> &swizzle) {
  assert(value->getType() == types.tyInt32V4);
  llvm::Value *newValue = value;
  auto regPtr = builder.CreateConstInBoundsGEP2_32(regsType, regs, 0, reg);
  if (((writeMask & 0xf) != 0xf /* xyzw */) ||
      *((uint32_t *)swizzle.begin()) != 0xe40 /* xyzw */) {
    auto currentValue = builder.CreateLoad(types.tyInt32V4, regPtr);
    newValue = builder.CreateShuffleVector(
        currentValue, value, CreateShuffleSwizzleMask(writeMask, swizzle));
  }
  builder.CreateStore(newValue, regPtr);
}

void AirBuilder::CreateRegisterStore(llvm::Value *regs, llvm::Type *regsType,
                                     llvm::Value *reg, llvm::Value *value,
                                     uint32_t writeMask,
                                     const std::array<uint8_t, 4> &swizzle) {
  assert(value->getType() == types.tyInt32V4);
  llvm::Value *newValue = value;
  auto regPtr = builder.CreateInBoundsGEP(regsType, regs,
                                          {CreateConstant((uint32_t)0), reg});
  if (((writeMask & 0xf) != 0xf /* xyzw */) ||
      *((uint32_t *)swizzle.begin()) != 0xe40 /* xyzw */) {
    auto currentValue = builder.CreateLoad(types.tyInt32V4, regPtr);
    newValue = builder.CreateShuffleVector(
        currentValue, value, CreateShuffleSwizzleMask(writeMask, swizzle));
  }
  builder.CreateStore(newValue, regPtr);
}

void AirBuilder::CreateGenericRegisterStore(llvm::Value *regs,
                                            llvm::Type *regsType, uint32_t reg,
                                            llvm::Value *value,
                                            uint32_t writeMask) {
  auto valueType = value->getType();
  if (valueType == types.tyInt32V4) {
    return CreateRegisterStore(regs, regsType, reg, value, writeMask,
                               swizzleXyzw);
  }
  if (valueType == types.tyInt32) {
    return CreateGenericRegisterStore(
        regs, regsType, reg, builder.CreateVectorSplat(4, value), writeMask);
  }
  if (valueType == types.tyFloat) {
    return CreateGenericRegisterStore(
        regs, regsType, reg,
        builder.CreateVectorSplat(4,
                                  builder.CreateBitCast(value, types.tyInt32)),
        writeMask);
  }
  if (valueType == types.tyFloatV4) {
    return CreateGenericRegisterStore(
        regs, regsType, reg,

        builder.CreateBitCast(value, types.tyInt32V4), writeMask);
  }
  if (valueType == types.typeContext["float3"]) {
    return CreateGenericRegisterStore(
        regs, regsType, reg,
        builder.CreateBitCast(
            builder.CreateShuffleVector(
                value, PoisonValue::get(value->getType()), {0, 1, 2, 0}),
            types.tyInt32V4),
        writeMask);
  }
  if (valueType == types.typeContext["float2"]) {
    return CreateGenericRegisterStore(
        regs, regsType, reg,
        builder.CreateBitCast(
            builder.CreateShuffleVector(
                value, PoisonValue::get(value->getType()), {0, 1, 0, 0}),
            types.tyInt32V4),
        writeMask);
  }
  if (valueType == types.typeContext["uint3"]) {
    return CreateGenericRegisterStore(
        regs, regsType, reg,
        builder.CreateShuffleVector(value, PoisonValue::get(value->getType()),
                                    {0, 1, 2, 0}),
        writeMask);
  }
  if (valueType == types.typeContext["uint2"]) {
    return CreateGenericRegisterStore(
        regs, regsType, reg,
        builder.CreateShuffleVector(value, PoisonValue::get(value->getType()),
                                    {0, 1, 0, 0}),
        writeMask);
  }
  assert(0 && "Overload not implemented yet.");
}

Value *AirBuilder::CreateShuffleSwizzleMask(
    uint32_t writeMask,
    const std::array<uint8_t, 4>
        &swizzle /* not all components are needed: it depends on mask*/) {
  // original value: 0 1 2 3
  // new value: 4 5 6 7
  // if writeMask at specific component is 0, use original value
  std::array<uint32_t, 4> mask = {0, 1, 2, 3}; // identity selection
  unsigned checked_component = 0;
  if (writeMask & 1) {
    mask[0] = 4 + swizzle[checked_component];
    checked_component++;
  }
  if (writeMask & 2) {
    mask[1] = 4 + swizzle[checked_component];
    checked_component++;
  }
  if (writeMask & 4) {
    mask[2] = 4 + swizzle[checked_component];
    checked_component++;
  }
  if (writeMask & 8) {
    mask[3] = 4 + swizzle[checked_component];
    checked_component++;
  }
  return CreateConstantVector(mask);
}

Constant *AirBuilder::CreateConstant(uint32_t value) {
  return ConstantInt::get(types.tyInt32, value, false);
}

Constant *AirBuilder::CreateConstant(float value) {
  return ConstantFP::get(types.tyFloat, APFloat(value));
}

Constant *AirBuilder::CreateConstantVector(const std::array<float, 4> &float4) {
  return ConstantVector::get(
      {CreateConstant(float4[0]), CreateConstant(float4[1]),
       CreateConstant(float4[2]), CreateConstant(float4[3])});
}

Constant *
AirBuilder::CreateConstantVector(const std::array<uint32_t, 4> &uint4) {
  return ConstantVector::get(
      {CreateConstant(uint4[0]), CreateConstant(uint4[1]),
       CreateConstant(uint4[2]), CreateConstant(uint4[3])});
}

} // namespace dxmt