#include "air_type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

using namespace llvm;

namespace dxmt {

AirType::AirType(LLVMContext &context) : Context(context) {

  tyInt32 = Type::getInt32Ty(context);
  tyFloat = Type::getFloatTy(context);
  tyBool = Type::getInt1Ty(context);

  auto tyHalf = Type::getHalfTy(context);
  auto tyShort = Type::getInt16Ty(context);

  tyInt32V4 = FixedVectorType::get(tyInt32, 4);
  tyFloatV4 = FixedVectorType::get(tyFloat, 4);

  auto tyInt32V3 = FixedVectorType::get(tyInt32, 3);
  auto tyShortV3 = FixedVectorType::get(tyShort, 3);
  auto tyFloatV3 = FixedVectorType::get(tyFloat, 3);

  auto tyInt32V2 = FixedVectorType::get(tyInt32, 2);
  auto tyShortV2 = FixedVectorType::get(tyShort, 2);
  auto tyFloatV2 = FixedVectorType::get(tyFloat, 2);

  auto tyOpaque = StructType::create(context, "opaque");
  tyDevicePtr = PointerType::get(tyOpaque, 1);
  tyConstantPtr = PointerType::get(tyOpaque, 2);
  //   auto tyTGSMPtr = PointerType::get(context, 3); // TODO: really?

  typeContext = {
      {"bool", tyBool},
      {"int", tyInt32},
      {"uint", tyInt32},
      {"short", tyShort},
      {"float", tyFloat},
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
      {"int4", tyInt32V4},
      {"uint4", tyInt32V4},
      {"float4", tyFloatV4},
      {"short4", FixedVectorType::get(tyShort, 4)},
      {"ushort4", FixedVectorType::get(tyShort, 4)},
      {"half", tyHalf},
      {"half4", FixedVectorType::get(tyHalf, 4)},

      {"texture1d", StructType::get(context, {tyDevicePtr}, false)},
      {"texture1d_array", StructType::get(context, {tyDevicePtr}, false)},
      {"texture2d", StructType::get(context, {tyDevicePtr}, false)},
      {"texture2d_array", StructType::get(context, {tyDevicePtr}, false)},
      {"texture2d_ms", StructType::get(context, {tyDevicePtr}, false)},
      {"texture2d_ms_array", StructType::get(context, {tyDevicePtr}, false)},
      {"texture3d", StructType::get(context, {tyDevicePtr}, false)},
      {"texturecube", StructType::get(context, {tyDevicePtr}, false)},
      {"texturecube_array", StructType::get(context, {tyDevicePtr}, false)},
      {"texture_buffer", StructType::get(context, {tyDevicePtr}, false)},

      {"sampler", StructType::get(context, {tyConstantPtr}, false)},
  };
};

Type *AirType::getTypeByMetalTypeName(const std::string &name) {
  return typeContext[name];
}

Type *AirType::getTypeByAirTypeEnum(air::EType type) {
  return typeContext[air::getAirTypeName(type)]; // TODO: might fail here!
}

} // namespace dxmt