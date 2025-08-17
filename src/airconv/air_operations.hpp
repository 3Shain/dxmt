#pragma once

#include "air_signature.hpp"
#include "air_type.hpp"
#include "monad.hpp"
#include "nt/air_builder.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Error.h"

namespace dxmt::air {

using pvalue = llvm::Value *;
using epvalue = llvm::Expected<pvalue>;

struct AIRBuilderContext;

using AIRBuilderResult = ReaderIO<AIRBuilderContext, pvalue>;

struct AIRBuilderContext {
  llvm::LLVMContext &llvm;
  llvm::Module &module;
  llvm::IRBuilder<> &builder;
  AirType &types;
  llvm::air::AIRBuilder& air;
};

template <typename S> AIRBuilderResult make_op(S &&fs) {
  return AIRBuilderResult(std::forward<S>(fs));
}

enum class MTLAttributeFormat {
  Invalid = 0,
  UChar2 = 1,
  // UChar3 = 2,
  UChar4 = 3,
  Char2 = 4,
  // Char3 = 5,
  Char4 = 6,
  UChar2Normalized = 7,
  // UChar3Normalized = 8,
  UChar4Normalized = 9,
  Char2Normalized = 10,
  // Char3Normalized = 11,
  Char4Normalized = 12,
  UShort2 = 13,
  // UShort3 = 14,
  UShort4 = 15,
  Short2 = 16,
  // Short3 = 17,
  Short4 = 18,
  UShort2Normalized = 19,
  // UShort3Normalized = 20,
  UShort4Normalized = 21,
  Short2Normalized = 22,
  // Short3Normalized = 23,
  Short4Normalized = 24,
  Half2 = 25,
  // Half3 = 26,
  Half4 = 27,
  Float = 28,
  Float2 = 29,
  Float3 = 30,
  Float4 = 31,
  Int = 32,
  Int2 = 33,
  Int3 = 34,
  Int4 = 35,
  UInt = 36,
  UInt2 = 37,
  UInt3 = 38,
  UInt4 = 39,
  Int1010102Normalized = 40,
  UInt1010102Normalized = 41,
  UChar4Normalized_BGRA = 42,
  UChar = 45,
  Char = 46,
  UCharNormalized = 47,
  CharNormalized = 48,
  UShort = 49,
  Short = 50,
  UShortNormalized = 51,
  ShortNormalized = 52,
  Half = 53,
  FloatRG11B10 = 54,
  FloatRGB9E5 = 55,
};

AIRBuilderResult pull_vec4_from_addr(
  MTLAttributeFormat format, pvalue base_addr, pvalue byte_offset
);

}; // namespace dxmt::air