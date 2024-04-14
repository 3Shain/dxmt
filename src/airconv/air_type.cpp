#include "air_type.hpp"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

using namespace llvm;

namespace dxmt::air {

llvm::StructType *
get_or_create_struct(llvm::LLVMContext &Ctx, llvm::StringRef Name) {
  using namespace llvm;
  StructType *ST = StructType::getTypeByName(Ctx, Name);
  if (ST)
    return ST;

  return StructType::create(Ctx, Name);
}

AirType::AirType(LLVMContext &context) : context(context) {

  _int = Type::getInt32Ty(context);
  _float = Type::getFloatTy(context);
  _bool = Type::getInt1Ty(context);
  _byte = Type::getInt8Ty(context);
  _long = Type::getInt64Ty(context);
  _long4 = FixedVectorType::get(_long, 4);


  _half = Type::getHalfTy(context);
  _half4 = FixedVectorType::get(_half, 4);

  auto tyShort = Type::getInt16Ty(context);

  _int4 = FixedVectorType::get(_int, 4);
  _int8 = FixedVectorType::get(_int, 8);
  _float4 = FixedVectorType::get(_float, 4);

  _int3 = FixedVectorType::get(_int, 3);
  auto tyShortV3 = FixedVectorType::get(tyShort, 3);
  _float3 = FixedVectorType::get(_float, 3);

  _int2 = FixedVectorType::get(_int, 2);
  auto tyShortV2 = FixedVectorType::get(tyShort, 2);
  _float2 = FixedVectorType::get(_float, 2);

  auto tyOpaque = get_or_create_struct(context, "opaque");
  _ptr_device = PointerType::get(tyOpaque, 1);
  _ptr_constant = PointerType::get(tyOpaque, 2);
  _ptr_threadgroup = PointerType::get(tyOpaque, 3);

  _texture1d = get_or_create_struct(context, "struct._texture_1d_t");
  _texture1d_array =
    get_or_create_struct(context, "struct._texture_1d_array_t");
  _texture2d = get_or_create_struct(context, "struct._texture_2d_t");
  _texture2d_array =
    get_or_create_struct(context, "struct._texture_2d_array_t");
  _texture2d_ms = get_or_create_struct(context, "struct._texture_2d_ms_t");
  _texture2d_ms_array =
    get_or_create_struct(context, "struct._texture_2d_ms_array_t");
  _texture_cube = get_or_create_struct(context, "struct._texture_cube_t");
  _texture_cube_array =
    get_or_create_struct(context, "struct._texture_cube_array_t");
  _texture3d = get_or_create_struct(context, "struct._texture_3d_t");
  _texture_buffer =
    get_or_create_struct(context, "struct._texture_buffer_1d_t");

  _texture2d = get_or_create_struct(context, "struct._depth_2d_t");
  _texture2d_array = get_or_create_struct(context, "struct._depth_2d_array_t");
  _texture2d_ms = get_or_create_struct(context, "struct._depth_2d_ms_t");
  _texture2d_ms_array =
    get_or_create_struct(context, "struct._depth_2d_ms_array_t");
  _texture_cube = get_or_create_struct(context, "struct._depth_cube_t");
  _texture_cube_array =
    get_or_create_struct(context, "struct._depth_cube_array_t");

  _sampler = get_or_create_struct(context, "struct._sampler_t");

  typeContext = {
    {"bool", _bool},
    {"int", _int},
    {"uint", _int},
    {"short", tyShort},
    {"float", _float},
    {"int2", _int2},
    {"short2", tyShortV2},
    {"uint2", _int2},
    {"ushort2", tyShortV2},
    {"float2", _float2},
    {"int3", _int3},
    {"short3", tyShortV3},
    {"uint3", _int3},
    {"ushort3", tyShortV3},
    {"float3", _float3},
    {"int4", _int4},
    {"uint4", _int4},
    {"float4", _float4},
    {"short4", FixedVectorType::get(tyShort, 4)},
    {"ushort4", FixedVectorType::get(tyShort, 4)},
    {"half", _half},
    {"half4", _half4},
  };
};

Type *AirType::getTypeByMetalTypeName(const std::string &name) {
  return typeContext[name];
}

} // namespace dxmt::air