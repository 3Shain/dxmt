#include "air_type.hpp"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

using namespace llvm;

namespace dxmt::air {

AirType::AirType(LLVMContext &context) : Context(context) {

  _int = Type::getInt32Ty(context);
  _float = Type::getFloatTy(context);
  _bool = Type::getInt1Ty(context);

  _half = Type::getHalfTy(context);
  _half4 = FixedVectorType::get(_half, 4);

  auto tyShort = Type::getInt16Ty(context);

  _int4 = FixedVectorType::get(_int, 4);
  _float4 = FixedVectorType::get(_float, 4);

  auto tyInt32V3 = FixedVectorType::get(_int, 3);
  auto tyShortV3 = FixedVectorType::get(tyShort, 3);
  auto tyFloatV3 = FixedVectorType::get(_float, 3);

  auto tyInt32V2 = FixedVectorType::get(_int, 2);
  auto tyShortV2 = FixedVectorType::get(tyShort, 2);
  auto tyFloatV2 = FixedVectorType::get(_float, 2);

  auto tyOpaque = StructType::create(context, "opaque");
  _ptr_device = PointerType::get(tyOpaque, 1);
  _ptr_constant = PointerType::get(tyOpaque, 2);

  _texture1d = StructType::create(context, "struct._texture_1d_t");
  _texture1d_array = StructType::create(context, "struct._texture_1d_array_t");
  _texture2d = StructType::create(context, "struct._texture_2d_t");
  _texture2d_array = StructType::create(context, "struct._texture_2d_array_t");
  _texture2d_ms = StructType::create(context, "struct._texture_2d_ms_t");
  _texture2d_ms_array = StructType::create(context, "struct._texture_2d_ms_array_t");
  _texture_cube = StructType::create(context, "struct._texture_cube_t");
  _texture_cube_array = StructType::create(context, "struct._texture_cube_array_t");
  _texture3d = StructType::create(context, "struct._texture_3d_t");
  _texture_buffer = StructType::create(context, "struct._texture_buffer_1d_t");

  _texture2d = StructType::create(context, "struct._depth_2d_t");
  _texture2d_array = StructType::create(context, "struct._depth_2d_array_t");
  _texture2d_ms = StructType::create(context, "struct._depth_2d_ms_t");
  _texture2d_ms_array = StructType::create(context, "struct._depth_2d_ms_array_t");
  _texture_cube = StructType::create(context, "struct._depth_cube_t");
  _texture_cube_array = StructType::create(context, "struct._depth_cube_array_t");

  _sampler = StructType::create(context, "struct._sampler_t");

  typeContext = {
      {"bool", _bool},
      {"int", _int},
      {"uint", _int},
      {"short", tyShort},
      {"float", _float},
      {"int2", tyInt32V2},
      {"short2", tyShortV2},
      {"uint2", tyInt32V2},
      {"ushort2", tyShortV2},
      {"float2", tyFloatV2},
      {"int3", tyInt32V3},
      {"short3", tyShortV3},
      {"uint3", tyInt32V3},
      {"ushort3", tyShortV3},
      {"float3", tyFloatV3},
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

Type *AirType::getTypeByAirTypeEnum(air::EType type) {
  return typeContext[air::getAirTypeName(type)]; // TODO: might fail here!
}

} // namespace dxmt