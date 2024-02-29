#pragma once

#include "llvm/ADT/Twine.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include <map>

#include "air_constants.hpp"

namespace dxmt::air {

class AirType {

public:
  llvm::Type *getTypeByMetalTypeName(const std::string &name);
  llvm::Type *getTypeByAirTypeEnum(air::EType type);
  AirType(llvm::LLVMContext &context);

  llvm::Type *tyBool;
  llvm::Type *tyFloat;
  llvm::Type *tyInt32;
  llvm::Type *tyFloatV4;
  llvm::Type *tyInt32V4;
  llvm::Type* tyConstantPtr;
  llvm::Type* tyDevicePtr;
  std::map<std::string, llvm::Type *> typeContext;

private:

  llvm::LLVMContext &Context;
};
} // namespace dxmt