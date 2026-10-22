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

#include "air_vertex_puller.hpp"
#include "../air_signature.hpp"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/Alignment.h"

namespace dxmt::air {

llvm::Value *
VertexPuller::PullVec4(MTLVertexFormat format, llvm::Value *BaseAddress, llvm::Value *ByteOffset) {
  using namespace llvm;

  auto current = ir.GetInsertBlock();

  auto pull_vertex = BasicBlock::Create(ir.getContext(), "vertex_pull_start", current->getParent());
  auto continuation = BasicBlock::Create(ir.getContext(), "vertex_pull_done", current->getParent());

  ir.CreateCondBr(ir.CreateIsNull(BaseAddress), continuation, pull_vertex);
  ir.SetInsertPoint(pull_vertex);

  auto value = PullVec4Checked(format, BaseAddress, ByteOffset);

  ir.CreateBr(continuation);
  ir.SetInsertPoint(continuation);

  auto phi = ir.CreatePHI(value->getType(), 2);
  phi->addIncoming(ConstantAggregateZero::get(value->getType()), current);
  phi->addIncoming(value, pull_vertex);

  return phi;
}

llvm::Value *
VertexPuller::PullVec4Checked(MTLVertexFormat format, llvm::Value *BaseAddress, llvm::Value *ByteOffset) {
  using namespace llvm;

  llvm::Value *value = nullptr;

  switch (format) {
  case MTLVertexFormat::Char:
    value = LoadFromDeviceBuffer(ir.getInt8Ty(), BaseAddress, ByteOffset, 0, 1);
    value = ir.CreateSExt(value, ir.getInt32Ty());
    value = ir.CreateInsertElement(PoisonValue::get(FixedVectorType::get(ir.getInt32Ty(), 4)), value, 0ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 1ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::Char2:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt8Ty(), 2), BaseAddress, ByteOffset, 0, 1);
    value = ir.CreateSExt(value, FixedVectorType::get(ir.getInt32Ty(), 2));
    value = ir.CreateShuffleVector(value, {0, 1, -1, -1});
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::Char4:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt8Ty(), 4), BaseAddress, ByteOffset, 0, 1);
    value = ir.CreateSExt(value, FixedVectorType::get(ir.getInt32Ty(), 4));
    break;
  case MTLVertexFormat::UChar:
    value = LoadFromDeviceBuffer(ir.getInt8Ty(), BaseAddress, ByteOffset, 0, 1);
    value = ir.CreateZExt(value, ir.getInt32Ty());
    value = ir.CreateInsertElement(PoisonValue::get(FixedVectorType::get(ir.getInt32Ty(), 4)), value, 0ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 1ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::UChar2:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt8Ty(), 2), BaseAddress, ByteOffset, 0, 1);
    value = ir.CreateZExt(value, FixedVectorType::get(ir.getInt32Ty(), 2));
    value = ir.CreateShuffleVector(value, {0, 1, -1, -1});
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::UChar4:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt8Ty(), 4), BaseAddress, ByteOffset, 0, 1);
    value = ir.CreateZExt(value, FixedVectorType::get(ir.getInt32Ty(), 4));
    break;
  case MTLVertexFormat::Short:
    value = LoadFromDeviceBuffer(ir.getInt16Ty(), BaseAddress, ByteOffset, 0, 2);
    value = ir.CreateSExt(value, ir.getInt32Ty());
    value = ir.CreateInsertElement(PoisonValue::get(FixedVectorType::get(ir.getInt32Ty(), 4)), value, 0ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 1ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::Short2:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt16Ty(), 2), BaseAddress, ByteOffset, 0, 2);
    value = ir.CreateSExt(value, FixedVectorType::get(ir.getInt32Ty(), 2));
    value = ir.CreateShuffleVector(value, {0, 1, -1, -1});
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::Short4:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt16Ty(), 4), BaseAddress, ByteOffset, 0, 2);
    value = ir.CreateSExt(value, FixedVectorType::get(ir.getInt32Ty(), 4));
    break;
  case MTLVertexFormat::UShort:
    value = LoadFromDeviceBuffer(ir.getInt16Ty(), BaseAddress, ByteOffset, 0, 2);
    value = ir.CreateZExt(value, ir.getInt32Ty());
    value = ir.CreateInsertElement(PoisonValue::get(FixedVectorType::get(ir.getInt32Ty(), 4)), value, 0ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 1ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::UShort2:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt16Ty(), 2), BaseAddress, ByteOffset, 0, 2);
    value = ir.CreateZExt(value, FixedVectorType::get(ir.getInt32Ty(), 2));
    value = ir.CreateShuffleVector(value, {0, 1, -1, -1});
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::UShort4:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt16Ty(), 4), BaseAddress, ByteOffset, 0, 2);
    value = ir.CreateZExt(value, FixedVectorType::get(ir.getInt32Ty(), 4));
    break;
  case MTLVertexFormat::Half:
    value = LoadFromDeviceBuffer(ir.getHalfTy(), BaseAddress, ByteOffset, 0, 2);
    value = air.CreateConvertToFloat(value);
    value = ir.CreateInsertElement(PoisonValue::get(FixedVectorType::get(ir.getFloatTy(), 4)), value, 0ull);
    value = ir.CreateInsertElement(value, air.getFloat(0.0f), 1ull);
    value = ir.CreateInsertElement(value, air.getFloat(0.0f), 2ull);
    value = ir.CreateInsertElement(value, air.getFloat(1.0f), 3ull);
    break;
  case MTLVertexFormat::Half2:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getHalfTy(), 2), BaseAddress, ByteOffset, 0, 2);
    value = air.CreateConvertToFloat(value);
    value = ir.CreateShuffleVector(value, {0, 1, -1, -1});
    value = ir.CreateInsertElement(value, air.getFloat(0.0f), 2ull);
    value = ir.CreateInsertElement(value, air.getFloat(1.0f), 3ull);
    break;
  case MTLVertexFormat::Half4:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getHalfTy(), 4), BaseAddress, ByteOffset, 0, 2);
    value = air.CreateConvertToFloat(value);
    break;
  case MTLVertexFormat::Float:
    value = LoadFromDeviceBuffer(ir.getFloatTy(), BaseAddress, ByteOffset, 0, 4);
    value = ir.CreateInsertElement(PoisonValue::get(FixedVectorType::get(ir.getFloatTy(), 4)), value, 0ull);
    value = ir.CreateInsertElement(value, air.getFloat(0.0f), 1ull);
    value = ir.CreateInsertElement(value, air.getFloat(0.0f), 2ull);
    value = ir.CreateInsertElement(value, air.getFloat(1.0f), 3ull);
    break;
  case MTLVertexFormat::Float2:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getFloatTy(), 2), BaseAddress, ByteOffset, 0, 4);
    value = ir.CreateShuffleVector(value, {0, 1, -1, -1});
    value = ir.CreateInsertElement(value, air.getFloat(0.0f), 2ull);
    value = ir.CreateInsertElement(value, air.getFloat(1.0f), 3ull);
    break;
  case MTLVertexFormat::Float3:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getFloatTy(), 3), BaseAddress, ByteOffset, 0, 4);
    value = ir.CreateShuffleVector(value, {0, 1, 2, -1});
    value = ir.CreateInsertElement(value, air.getFloat(1.0f), 3ull);
    break;
  case MTLVertexFormat::Float4:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getFloatTy(), 4), BaseAddress, ByteOffset, 0, 4);
    break;
  case MTLVertexFormat::UInt:
  case MTLVertexFormat::Int:
    value = LoadFromDeviceBuffer(ir.getInt32Ty(), BaseAddress, ByteOffset, 0, 4);
    value = ir.CreateInsertElement(PoisonValue::get(FixedVectorType::get(ir.getInt32Ty(), 4)), value, 0ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 1ull);
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::UInt2:
  case MTLVertexFormat::Int2:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt32Ty(), 2), BaseAddress, ByteOffset, 0, 4);
    value = ir.CreateShuffleVector(value, {0, 1, -1, -1});
    value = ir.CreateInsertElement(value, air.getInt(0), 2ull);
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::UInt3:
  case MTLVertexFormat::Int3:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt32Ty(), 3), BaseAddress, ByteOffset, 0, 4);
    value = ir.CreateShuffleVector(value, {0, 1, 2, -1});
    value = ir.CreateInsertElement(value, air.getInt(1), 3ull);
    break;
  case MTLVertexFormat::UInt4:
  case MTLVertexFormat::Int4:
    value = LoadFromDeviceBuffer(FixedVectorType::get(ir.getInt32Ty(), 4), BaseAddress, ByteOffset, 0, 4);
    break;
  case MTLVertexFormat::UCharNormalized:
  case MTLVertexFormat::UChar2Normalized:
  case MTLVertexFormat::UChar4Normalized:
  case MTLVertexFormat::UChar4Normalized_BGRA:
  case MTLVertexFormat::CharNormalized:
  case MTLVertexFormat::Char2Normalized:
  case MTLVertexFormat::Char4Normalized:
  case MTLVertexFormat::UShortNormalized:
  case MTLVertexFormat::UShort2Normalized:
  case MTLVertexFormat::UShort4Normalized:
  case MTLVertexFormat::ShortNormalized:
  case MTLVertexFormat::Short2Normalized:
  case MTLVertexFormat::Short4Normalized:
  case MTLVertexFormat::FloatRG11B10:
  case MTLVertexFormat::FloatRGB9E5:
  case MTLVertexFormat::Int1010102Normalized:
  case MTLVertexFormat::UInt1010102Normalized:
    value = UnpackFVec4(format, BaseAddress, ByteOffset);
    break;
  case MTLVertexFormat::Invalid:
    break;
  }
  assert(value);

  return value;
}

llvm::Value *
VertexPuller::UnpackFVec4(MTLVertexFormat format, llvm::Value *BaseAddress, llvm::Value *AlignedOffset) {
  using namespace llvm;

  if (format == MTLVertexFormat::UShort4Normalized) {
    llvm::Value *_xy = CallUnpackImpl(
        "unorm2x16.v2f32", LoadFromDeviceBuffer(ir.getInt32Ty(), BaseAddress, AlignedOffset, 0, 2), ir.getInt32Ty(),
        FixedVectorType::get(ir.getFloatTy(), 2)
    );
    llvm::Value *_zw = CallUnpackImpl(
        "unorm2x16.v2f32", LoadFromDeviceBuffer(ir.getInt32Ty(), BaseAddress, AlignedOffset, 1, 2), ir.getInt32Ty(),
        FixedVectorType::get(ir.getFloatTy(), 2)
    );
    return ir.CreateShuffleVector(_xy, _zw, {0, 1, 2, 3});
  }
  if (format == MTLVertexFormat::Short4Normalized) {
    llvm::Value *_xy = CallUnpackImpl(
        "snorm2x16.v2f32", LoadFromDeviceBuffer(ir.getInt32Ty(), BaseAddress, AlignedOffset, 0, 2), ir.getInt32Ty(),
        FixedVectorType::get(ir.getFloatTy(), 2)
    );
    llvm::Value *_zw = CallUnpackImpl(
        "snorm2x16.v2f32", LoadFromDeviceBuffer(ir.getInt32Ty(), BaseAddress, AlignedOffset, 1, 2), ir.getInt32Ty(),
        FixedVectorType::get(ir.getFloatTy(), 2)
    );
    return ir.CreateShuffleVector(_xy, _zw, {0, 1, 2, 3});
  }

  std::string op;
  llvm::Type *src_type = nullptr;
  uint32_t align = 0;
  llvm::Type *dst_type = nullptr;
  switch (format) {
  case MTLVertexFormat::UCharNormalized:
    op = "unorm1x8.f32";
    src_type = ir.getInt8Ty();
    align = 1;
    dst_type = ir.getFloatTy();
    break;
  case MTLVertexFormat::UChar2Normalized:
    op = "unorm2x8.v2f32";
    src_type = ir.getInt16Ty();
    align = 1;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 2);
    break;
  case MTLVertexFormat::UChar4Normalized:
  case MTLVertexFormat::UChar4Normalized_BGRA:
    op = "unorm4x8.v4f32";
    src_type = ir.getInt32Ty();
    align = 1;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 4);
    break;
  case MTLVertexFormat::CharNormalized:
    op = "snorm1x8.f32";
    src_type = ir.getInt8Ty();
    align = 1;
    dst_type = ir.getFloatTy();
    break;
  case MTLVertexFormat::Char2Normalized:
    op = "snorm2x8.v2f32";
    src_type = ir.getInt16Ty();
    align = 1;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 2);
    break;
  case MTLVertexFormat::Char4Normalized:
    op = "snorm4x8.v4f32";
    src_type = ir.getInt32Ty();
    align = 1;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 4);
    break;
  case MTLVertexFormat::UShortNormalized:
    op = "unorm1x16.f32";
    src_type = ir.getInt16Ty();
    align = 2;
    dst_type = ir.getFloatTy();
    break;
  case MTLVertexFormat::UShort2Normalized:
    op = "unorm2x16.v2f32";
    src_type = ir.getInt32Ty();
    align = 2;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 2);
    break;
  case MTLVertexFormat::ShortNormalized:
    op = "snorm1x16.f32";
    src_type = ir.getInt16Ty();
    align = 2;
    dst_type = ir.getFloatTy();
    break;
  case MTLVertexFormat::Short2Normalized:
    op = "snorm2x16.v2f32";
    src_type = ir.getInt32Ty();
    align = 2;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 2);
    break;
  case MTLVertexFormat::UInt1010102Normalized:
    op = "unorm.rgb10a2.v4f32";
    src_type = ir.getInt32Ty();
    align = 4;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 4);
    break;
  case MTLVertexFormat::Int1010102Normalized:
    assert(0 && "unused & Metal 4 only");
    op = "snorm.rgb10a2.v4f32";
    src_type = ir.getInt32Ty();
    align = 4;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 4);
    break;
  case MTLVertexFormat::FloatRG11B10:
    op = "unorm.rg11b10f.v3f32";
    src_type = ir.getInt32Ty();
    align = 4;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 3);
    break;
  case MTLVertexFormat::FloatRGB9E5:
    op = "unorm.rgb9e5.v3f32";
    src_type = ir.getInt32Ty();
    align = 4;
    dst_type = FixedVectorType::get(ir.getFloatTy(), 3);
    break;
  default:
    break;
  }
  assert(src_type && dst_type);
  llvm::Value *ret =
      CallUnpackImpl(op, LoadFromDeviceBuffer(src_type, BaseAddress, AlignedOffset, 0, align), src_type, dst_type);
  if (format == MTLVertexFormat::UChar4Normalized_BGRA) {
    ret = ir.CreateShuffleVector(ret, {2, 1, 0, 3});
  }
  if (dst_type == ir.getFloatTy()) {
    ret = ir.CreateInsertElement(PoisonValue::get(FixedVectorType::get(ir.getFloatTy(), 4)), ret, 0ull);
    ret = ir.CreateInsertElement(ret, air.getFloat(0.0f), 1ull);
    ret = ir.CreateInsertElement(ret, air.getFloat(0.0f), 2ull);
    ret = ir.CreateInsertElement(ret, air.getFloat(1.0f), 3ull);
  } else if (dst_type == FixedVectorType::get(ir.getFloatTy(), 2)) {
    ret = ir.CreateShuffleVector(ret, {0, 1, -1, -1});
    ret = ir.CreateInsertElement(ret, air.getFloat(0.0f), 2ull);
    ret = ir.CreateInsertElement(ret, air.getFloat(1.0f), 3ull);
  } else if (dst_type == FixedVectorType::get(ir.getFloatTy(), 3)) {
    ret = ir.CreateShuffleVector(ret, {0, 1, 2, -1});
    ret = ir.CreateInsertElement(ret, air.getFloat(1.0f), 3ull);
  }
  return ret;
}

llvm::Value *
VertexPuller::LoadFromDeviceBuffer(
    llvm::Type *value_type, llvm::Value *BaseAddress, llvm::Value *AlignedOffset, uint32_t ElementOffset, uint32_t align
) {
  using namespace llvm;
  return ir.CreateAlignedLoad(
      value_type,
      ir.CreateConstGEP1_32(
          value_type,
          ir.CreateBitCast(
              ir.CreateGEP(ir.getInt8Ty(), BaseAddress, {AlignedOffset}),
              value_type->getPointerTo((uint32_t)air::AddressSpace::device)
          ),
          ElementOffset
      ),
      llvm::MaybeAlign(align)
  );
}

llvm::Value *
VertexPuller::CallUnpackImpl(std::string Op, llvm::Value *Src, llvm::Type *TySrc, llvm::Type *TyDst) {
  using namespace llvm;
  auto &context = ir.getContext();
  auto att = AttributeList::get(
      context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(context, Attribute::AttrKind::MustProgress)},
                {~0U, Attribute::get(context, Attribute::AttrKind::NoFree)},
                {~0U, Attribute::get(context, Attribute::AttrKind::NoSync)},
                {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)}}
  );
  auto fn = module.getOrInsertFunction("air.unpack." + Op, FunctionType::get(TyDst, {TySrc}, false), att);
  return ir.CreateCall(fn, {Src});
}

} // namespace dxmt::air
