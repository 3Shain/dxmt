/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#pragma once

#include "air_builder.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include <string>

namespace dxmt::air {

/** a.k.a. MTLAttributeFormat */
enum class MTLVertexFormat {
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

class VertexPuller {
public:
  VertexPuller(llvm::air::AIRBuilder &air, llvm::Module &module) : air(air), ir(air.builder), module(module) {}

  llvm::Value *PullVec4(MTLVertexFormat Format, llvm::Value *BaseAddress, llvm::Value *ByteOffset);

private:
  llvm::Value *PullVec4Checked(MTLVertexFormat Format, llvm::Value *BaseAddress, llvm::Value *ByteOffset);

  llvm::Value *UnpackFVec4(MTLVertexFormat Format, llvm::Value *BaseAddress, llvm::Value *AlignedOffset);

  llvm::Value *LoadFromDeviceBuffer(
      llvm::Type *value_type, llvm::Value *BaseAddress, llvm::Value *AlignedOffset, uint32_t ElementOffset,
      uint32_t align
  );

  llvm::Value *CallUnpackImpl(std::string Op, llvm::Value *Src, llvm::Type *TySrc, llvm::Type *TyDst);

  llvm::air::AIRBuilder &air;
  llvm::IRBuilderBase &ir;
  llvm::Module &module;
};

} // namespace dxmt::air
