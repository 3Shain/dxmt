#pragma once

#include "llvm/ADT/Twine.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "air_constants.h"
#include "air_type.h"

namespace dxmt {

class AirOp {

public:
  AirOp(AirType &types, llvm::LLVMContext &context, llvm::Module &module);

  llvm::FunctionCallee GetUnaryOp(air::EIntrinsicOp op, air::EType overload);

  llvm::Twine GetOverloadSymbolSegment(air::EType overload);

  llvm::FunctionCallee GetDotProduct();

private:
  AirType &types;
  llvm::LLVMContext &context;
  llvm::Module &module;
};

} // namespace dxmt