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

  llvm::Type *_bool;
  llvm::Type *_float;
  llvm::Type *_float4;
  llvm::Type *_int;
  llvm::Type *_int4;
  llvm::Type *_half;
  llvm::Type *_half4;

  llvm::Type *_sampler;

  llvm::Type *_texture1d;
  llvm::Type *_texture1d_array;
  llvm::Type *_texture2d;
  llvm::Type *_texture2d_array;
  llvm::Type *_texture2d_ms;
  llvm::Type *_texture2d_ms_array;
  llvm::Type *_texture3d;
  llvm::Type *_texture_cube;
  llvm::Type *_texture_cube_array; // probably unused in dxbc
  llvm::Type *_texture_buffer;

  llvm::Type *_depth2d;
  llvm::Type *_depth2d_array;
  llvm::Type *_depth2d_ms;
  llvm::Type *_depth2d_ms_array;
  llvm::Type *_depth_cube;
  llvm::Type *_depth_cube_array;

  llvm::Type* _ptr_constant;
  llvm::Type* _ptr_device;
  llvm::Type* _ptr_threadgroup;
  std::map<std::string, llvm::Type *> typeContext;

private:

  llvm::LLVMContext &Context;
};
} // namespace dxmt