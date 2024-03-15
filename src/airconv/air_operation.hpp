#pragma once

#include "llvm/ADT/Twine.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "air_builder.hpp"
#include "air_constants.hpp"
#include "air_type.hpp"

namespace dxmt::air {

enum class EBinaryOp {
  FAdd,

  FMax,
  FMin,
  FMul,

  FDp2,
  FDp3,
  FDp4,

  BAnd,
  BOr,

  BIShl,
  BIShr,
  BUShl,
  BUShr,
  BXor,

  IAdd,
  IMax,
  IMin,
  IMul,
  
  UMax,
  UMin,
};

enum class EAtomicOp {
  And,
  Xor,
  Add,
  IMax,
  IMin,
  UMax,
  UMin,
  // TO BE DONE..
};

class AirOp {

public:
  AirOp(AirType &types, AirBuilder& builder, llvm::LLVMContext &context, llvm::Module &module);


  llvm::Twine GetOverloadSymbolSegment(air::EType overload);

  llvm::FunctionCallee GetDotProduct();

  llvm::Value* CreateFloatUnaryOp(EFloatUnaryOp op, llvm::Value* src);
  llvm::Value* CreateFloatBinaryOp(EFloatBinaryOp op, llvm::Value* src0, llvm::Value* src1);


  llvm::Value* CreateBitwiseUnaryOp(EBitwiseUnaryOp op, llvm::Value* src);
  llvm::Value* CreateBitwiseBinaryOp(EBitwiseBinaryOp op, llvm::Value* src0, llvm::Value* src1);

  // a * b + c
  llvm::Value* CreateFMad(llvm::Value* a, llvm::Value* b, llvm::Value* c);
  llvm::Value* CreateIMad(llvm::Value* a, llvm::Value* b, llvm::Value* c);
  llvm::Value* CreateUMad(llvm::Value* a, llvm::Value* b, llvm::Value* c);

  // UADDC
  // UDIV
  // UMUL
  // USUBB
  // MSad

  llvm::Value* CreateBFI();
  llvm::Value* CreateIBFE();

  // resource related

  llvm::Value* CreateSampleTexture(llvm::Value* sampler, llvm::Value* position, llvm::Value* );
  llvm::Value* CreateStoreTypedTexture();
  llvm::Value* CreateLoadTypedTexture();
  // llvm::Value* Create

private:
  AirType &types;
  AirBuilder &builder;
  llvm::LLVMContext &context;
  llvm::Module &module;

  llvm::AttributeList commonAttributes;
};

} // namespace dxmt