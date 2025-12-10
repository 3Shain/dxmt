#include "air_builder.hpp"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <format>

namespace llvm::air {

struct TextureOperationInfo {
  const char *air_symbol_suffix;
  bool is_array;
  uint8_t coord_dimension;
  bool is_cube;
  bool is_depth;
  bool is_ms;
  bool is_mipmaped;
};

TextureOperationInfo TextureInfo[] = {
    // texture_buffer,
    {"texture_buffer_1d", false, 1, false, false, false, false},
    // texture1d,
    {"texture_1d", false, 1, false, false, false, true /* but only 0 is valid */},
    // texture1d_array,
    {"texture_1d_array", true, 1, false, false, false, true /* but only 0 is valid */},
    // texture2d,
    {"texture_2d", false, 2, false, false, false, true},
    // texture2d_array,
    {"texture_2d_array", true, 2, false, false, false, true},
    // texture3d,
    {"texture_3d", false, 3, false, false, false, true},
    // texturecube,
    {"texture_cube", false, 3, true, false, false, true},
    // texturecube_array,
    {"texture_cube_array", true, 3, true, false, false, true},
    // texture2d_ms,
    {"texture_2d_ms", false, 2, false, false, true, false},
    // texture2d_ms_array,
    {"texture_2d_ms_array", true, 2, false, false, true, false},
    // depth2d,
    {"depth_2d", false, 2, false, true, false, true},
    // depth2d_array,
    {"depth_2d_array", true, 2, false, true, false, true},
    // depthcube,
    {"depth_cube", false, 3, true, true, false, true},
    // depthcube_array,
    {"depth_cube_array", true, 3, true, true, false, true},
    // depth_2d_ms,
    {"depth_2d_ms", false, 2, false, true, true, false},
    // depth_2d_ms_array,
    {"depth_2d_ms_array", true, 2, false, true, true, false},
};

std::string
AIRBuilder::getTypeOverloadSuffix(Type *Ty, Signedness Sign) {
  std::string Ret = ".";
  uint32_t PointerAddrSpace = ~0u;
  if (auto TyPtr = dyn_cast<PointerType>(Ty)) {
    PointerAddrSpace = TyPtr->getAddressSpace();
    Ty = TyPtr->getNonOpaquePointerElementType();
  }
  Type *TyScaler = Ty->getScalarType();
  uint32_t VectorSize = 0;
  if (isa<FixedVectorType>(Ty)) {
    VectorSize = cast<FixedVectorType>(Ty)->getNumElements();
  };
  if (TyScaler->isFloatTy()) {
    if (Sign != Signedness::DontCare)
      Ret += "f.";
    if (PointerAddrSpace != ~0u)
      Ret += std::format("p{}", PointerAddrSpace);
    if (VectorSize)
      Ret += std::format("v{}", VectorSize);
    Ret += "f32";
  } else if (TyScaler->isHalfTy()) {
    if (Sign != Signedness::DontCare)
      Ret += "f.";
    if (PointerAddrSpace != ~0u)
      Ret += std::format("p{}", PointerAddrSpace);
    if (VectorSize)
      Ret += std::format("v{}", VectorSize);
    Ret += "f16";
  } else if (TyScaler->isIntegerTy()) {
    if (Sign == Signedness::Signed)
      Ret += "s.";
    else if (Sign == Signedness::Unsigned)
      Ret += "u.";
    if (PointerAddrSpace != ~0u)
      Ret += std::format("p{}", PointerAddrSpace);
    if (VectorSize)
      Ret += std::format("v{}", VectorSize);
    Ret += std::format("i{}", cast<IntegerType>(TyScaler)->getBitWidth());
  } else {
    Ret += "unknown_type_overload";
  }
  return Ret;
}

Value *
AIRBuilder::CreateAtomicRMW(
    const Texture &Texture, Value *Handle, AtomicRMWInst::BinOp Op, Value *Pos, Value *ValVec4, Value *ArrayIndex
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  if (Texture.sample_type != Texture::sample_int && Texture.sample_type != Texture::sample_uint) {
    debug << "invalid operation: atomic operation on non integer texture.\n";
    return nullptr; // TODO:
  }

  if (TexInfo.is_depth) {
    debug << "invalid operation: atomic operation on depth texture.\n";
    return nullptr; // TODO:
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getTextureRWPositionType(Texture));
  Ops.push_back(Pos);

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  Tys.push_back(getTexelType(Texture));
  Ops.push_back(ValVec4);

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(0)); // memory order relaxed

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(Texture.memory_access));

  std::string FnName = "air.atomic_";

  if (Op == llvm::AtomicRMWInst::Xchg) {
    FnName += "exchange_explicit_";
    FnName += TexInfo.air_symbol_suffix;
    FnName += getTypeOverloadSuffix(getTexelType(Texture), Signedness::Unsigned);
  } else {
    Signedness Sign = Signedness::Unsigned;
    FnName += "fetch_";
    switch (Op) {
    case llvm::AtomicRMWInst::Add:
      FnName += "add";
      break;
    case llvm::AtomicRMWInst::Sub:
      FnName += "sub";
      break;
    case llvm::AtomicRMWInst::And:
      FnName += "and";
      break;
    case llvm::AtomicRMWInst::Or:
      FnName += "or";
      break;
    case llvm::AtomicRMWInst::Xor:
      FnName += "xor";
      break;
    case llvm::AtomicRMWInst::Max:
      Sign = Signedness::Signed;
      FnName += "max";
      break;
    case llvm::AtomicRMWInst::Min:
      Sign = Signedness::Signed;
      FnName += "min";
      break;
    case llvm::AtomicRMWInst::UMax:
      FnName += "max";
      break;
    case llvm::AtomicRMWInst::UMin:
      FnName += "min";
      break;
    default:
      FnName += "<invalid atomic op>";
      break;
    }
    FnName += "_explicit_";
    FnName += TexInfo.air_symbol_suffix;
    FnName += getTypeOverloadSuffix(getTexelType(Texture), Sign);
  }

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(getTexelType(Texture), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

std::pair<Value *, Value *>
AIRBuilder::CreateAtomicCmpXchg(
    const Texture &Texture, Value *Handle, Value *Pos, Value *CmpVec4, Value *NewVec4, Value *ArrayIndex
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  if (Texture.sample_type != Texture::sample_int && Texture.sample_type != Texture::sample_uint) {
    debug << "invalid operation: atomic operation on non integer texture.\n";
    return {nullptr, nullptr}; // TODO:
  }

  if (TexInfo.is_depth) {
    debug << "invalid operation: atomic operation on depth texture.\n";
    return {nullptr, nullptr}; // TODO:
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Type *TyTexel = getTexelType(Texture);

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getTextureRWPositionType(Texture));
  Ops.push_back(Pos);

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  AllocaInst *Alloca = builder.CreateAlloca(TyTexel, 0u);
  Value *Ptr = builder.CreateConstGEP1_64(TyTexel, Alloca, 0u);
  builder.CreateStore(CmpVec4, Ptr);

  Tys.push_back(TyTexel->getPointerTo());
  Ops.push_back(Alloca);

  Tys.push_back(TyTexel);
  Ops.push_back(NewVec4);

  Tys.push_back(getIntTy()); // success
  Ops.push_back(getInt(0));  // memory order relaxed
  Tys.push_back(getIntTy()); // failure
  Ops.push_back(getInt(0));  // memory order relaxed

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(Texture.memory_access));

  std::string FnName = "air.atomic_compare_exchange_weak_explicit_";
  FnName += TexInfo.air_symbol_suffix;
  FnName += getTypeOverloadSuffix(getTexelType(Texture), Signedness::Unsigned);

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(getBoolTy(), Tys, false), Attrs);

  auto Ret = builder.CreateCall(Fn, Ops);

  return {builder.CreateLoad(TyTexel, Ptr), Ret};
}

Type *
AIRBuilder::getTextureHandleType(const Texture &Texture) {
  assert(Texture.kind <= Texture::last_resource_kind);
  return getOrCreateStructType(std::format("struct._{}_t", TextureInfo[Texture.kind].air_symbol_suffix))
      ->getPointerTo(1);
};

Type *
AIRBuilder::getTexelType(const Texture &Texture) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &info = TextureInfo[Texture.kind];
  if (info.is_depth)
    return getFloatTy(); // depth texture must return a single float
  if (Texture.sample_type == Texture::sample_float)
    return getFloatTy(4);
  if (Texture.sample_type == Texture::sample_int)
    return getIntTy(4);
  if (Texture.sample_type == Texture::sample_uint)
    return getIntTy(4);
  llvm_unreachable("unhandled sample type");
}

Type *
AIRBuilder::getTexelGatherType(const Texture &Texture) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &info = TextureInfo[Texture.kind];
  if (info.is_depth)
    return getFloatTy(4); // depth texture gather must return float4
  return getTexelType(Texture);
}

Type *
AIRBuilder::getTextureSampleCoordType(const Texture &Texture) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &info = TextureInfo[Texture.kind];
  return getFloatTy(info.coord_dimension);
}

Type *
AIRBuilder::getTextureRWPositionType(const Texture &Texture) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &info = TextureInfo[Texture.kind];
  if (info.is_cube) {
    return getIntTy(2);
  }
  return getIntTy(info.coord_dimension);
}

std::pair<Value *, Value *>
AIRBuilder::CreateSampleCommon(
    const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Offset,
    bool ArgsControlBit, Value *Args1, Value *Args2
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {2U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::Convergent)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getSamplerHandleType());
  Ops.push_back(Sampler);

  if (TexInfo.is_depth) {
    Tys.push_back(getIntTy());
    Ops.push_back(getInt(1));
  }

  Tys.push_back(getTextureSampleCoordType(Texture));
  Ops.push_back(Coord);

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  switch (Texture.kind) {
  case Texture::texture1d:
  case Texture::texture1d_array:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(false));
    Tys.push_back(getIntTy());
    Ops.push_back(Offset);
    break;
  case Texture::texture2d:
  case Texture::texture2d_array:
  case Texture::depth2d:
  case Texture::depth2d_array:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(true));
    Tys.push_back(getIntTy(2));
    Ops.push_back(Offset);
    break;
  case Texture::texture3d:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(true));
    Tys.push_back(getIntTy(3));
    Ops.push_back(Offset);
    break;
  default:
    break;
  }

  Tys.push_back(getBoolTy());
  Ops.push_back(getBool(ArgsControlBit));
  Tys.push_back(getFloatTy());
  Ops.push_back(Args1);
  Tys.push_back(getFloatTy());
  Ops.push_back(Args2);

  /* access: always 0 = sample */
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(0));

  std::string FnName = "air.sample_";
  FnName += TexInfo.air_symbol_suffix;
  FnName += getTypeOverloadSuffix(getTexelType(Texture), getTexelSign(Texture));

  auto Fn = getModule()->getOrInsertFunction(
      FnName, FunctionType::get(getTextureSampleResultType(Texture), Tys, false), Attrs
  );

  auto Result = builder.CreateCall(Fn, Ops);

  return {builder.CreateExtractValue(Result, {0u}), builder.CreateExtractValue(Result, {1u})};
};

std::pair<Value *, Value *>
AIRBuilder::CreateSampleCmpCommon(
    const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Reference,
    Value *Offset, bool ArgsControlBit, Value *Args1, Value *Args2
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  if (!TexInfo.is_depth) {
    debug << "invalid operation: compare on non depth texture.\n";
    return {nullptr, nullptr}; // TODO
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {2U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::Convergent)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getSamplerHandleType());
  Ops.push_back(Sampler);

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(1));

  Tys.push_back(getTextureSampleCoordType(Texture));
  Ops.push_back(Coord);

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  Tys.push_back(getFloatTy());
  Ops.push_back(Reference);

  switch (Texture.kind) {
  case Texture::depth2d:
  case Texture::depth2d_array:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(true));
    Tys.push_back(getIntTy(2));
    Ops.push_back(Offset);
    break;
  default:
    break;
  }

  Tys.push_back(getBoolTy());
  Ops.push_back(getBool(ArgsControlBit));
  Tys.push_back(getFloatTy());
  Ops.push_back(Args1);
  Tys.push_back(getFloatTy());
  Ops.push_back(Args2);

  /* access: always 0 = sample */
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(0));

  std::string FnName = "air.sample_compare_";
  FnName += TexInfo.air_symbol_suffix;
  FnName += getTypeOverloadSuffix(getTexelType(Texture), getTexelSign(Texture));

  auto Fn = getModule()->getOrInsertFunction(
      FnName, FunctionType::get(getTextureSampleResultType(Texture), Tys, false), Attrs
  );

  auto Result = builder.CreateCall(Fn, Ops);

  return {builder.CreateExtractValue(Result, {0u}), builder.CreateExtractValue(Result, {1u})};
};

std::pair<Value *, Value *>
AIRBuilder::CreateSampleGrad(
    const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *DerivX,
    Value *DerivY, Value *MinLOD, const int32_t Offset[3]
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {2U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getSamplerHandleType());
  Ops.push_back(Sampler);

  if (TexInfo.is_depth) {
    Tys.push_back(getIntTy());
    Ops.push_back(getInt(1));
  }

  Tys.push_back(getTextureSampleCoordType(Texture));
  Ops.push_back(Coord);

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  Tys.push_back(getTextureSampleCoordType(Texture));
  Ops.push_back(DerivX);
  Tys.push_back(getTextureSampleCoordType(Texture));
  Ops.push_back(DerivY);

  Tys.push_back(getFloatTy());
  Ops.push_back(MinLOD);

  switch (Texture.kind) {
  case Texture::texture1d:
  case Texture::texture1d_array:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(false));
    Tys.push_back(getIntTy());
    Ops.push_back(getInt(Offset[0])); // seems to be ignored?
    break;
  case Texture::texture2d:
  case Texture::texture2d_array:
  case Texture::depth2d:
  case Texture::depth2d_array:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(true));
    Tys.push_back(getIntTy(2));
    Ops.push_back(getInt2(Offset[0], Offset[1]));
    break;
  case Texture::texture3d:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(true));
    Tys.push_back(getIntTy(3));
    Ops.push_back(getInt3(Offset[0], Offset[1], Offset[2]));
    break;
  default:
    break;
  }

  /* access: always 0 = sample */
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(0));

  std::string FnName = "air.sample_";
  FnName += TexInfo.air_symbol_suffix;
  FnName += "_grad";
  FnName += getTypeOverloadSuffix(getTexelType(Texture), getTexelSign(Texture));

  auto Fn = getModule()->getOrInsertFunction(
      FnName, FunctionType::get(getTextureSampleResultType(Texture), Tys, false), Attrs
  );

  auto Result = builder.CreateCall(Fn, Ops);

  return {builder.CreateExtractValue(Result, {0u}), builder.CreateExtractValue(Result, {1u})};
}

std::pair<Value *, Value *>
AIRBuilder::CreateRead(
    const Texture &Texture, Value *Handle, Value *Pos, Value *ArrayIndex, Value *SampleIndexOrCubeFace, Value *Level,
    bool DeviceCoherent
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  if (TexInfo.is_depth) {
    Tys.push_back(getIntTy());
    Ops.push_back(getInt(1));
  }

  Tys.push_back(getTextureRWPositionType(Texture));
  Ops.push_back(Pos);

  if (TexInfo.is_cube) {
    Tys.push_back(getIntTy());
    Ops.push_back(SampleIndexOrCubeFace);
  }

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  if (TexInfo.is_ms) {
    Tys.push_back(getIntTy());
    Ops.push_back(SampleIndexOrCubeFace);
  }

  if (TexInfo.is_mipmaped) {
    Tys.push_back(getIntTy());
    Ops.push_back(Level);
  }

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(Texture.memory_access));

  std::string FnName = "air.read_";
  FnName += TexInfo.air_symbol_suffix;
  if (DeviceCoherent)
    FnName += ".device_coherent";
  FnName += getTypeOverloadSuffix(getTexelType(Texture), getTexelSign(Texture));

  auto Fn = getModule()->getOrInsertFunction(
      FnName, FunctionType::get(getTextureSampleResultType(Texture), Tys, false), Attrs
  );

  auto Result = builder.CreateCall(Fn, Ops);

  return {builder.CreateExtractValue(Result, {0u}), builder.CreateExtractValue(Result, {1u})};
}

CallInst *
AIRBuilder::CreateWrite(
    const Texture &Texture, Value *Handle, Value *Pos, Value *ArrayIndex, Value *CubeFace, Value *Level, Value *ValVec4,
    bool DeviceCoherent
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getTextureRWPositionType(Texture));
  Ops.push_back(Pos);

  if (TexInfo.is_cube) {
    Tys.push_back(getIntTy());
    Ops.push_back(CubeFace);
  }

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  Tys.push_back(getTexelType(Texture));
  Ops.push_back(ValVec4);

  if (TexInfo.is_mipmaped) {
    Tys.push_back(getIntTy());
    Ops.push_back(Level);
  }

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(Texture.memory_access));

  std::string FnName = "air.write_";
  FnName += TexInfo.air_symbol_suffix;
  if (DeviceCoherent)
    FnName += ".device_coherent";
  FnName += getTypeOverloadSuffix(getTexelType(Texture), getTexelSign(Texture));

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(getVoidTy(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

std::pair<Value *, Value *>
AIRBuilder::CreateGather(
    const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Offset,
    Value *Component
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {2U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getSamplerHandleType());
  Ops.push_back(Sampler);

  if (TexInfo.is_depth) {
    Tys.push_back(getIntTy());
    Ops.push_back(getInt(1));
  }

  Tys.push_back(getTextureSampleCoordType(Texture));
  Ops.push_back(Coord);

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  switch (Texture.kind) {
  case Texture::texture2d:
  case Texture::texture2d_array:
  case Texture::depth2d:
  case Texture::depth2d_array:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(true));
    Tys.push_back(getIntTy(2));
    Ops.push_back(Offset);
    break;
  default: // cube doesn't support offset, and others don't support gather
    break;
  }

  if (!TexInfo.is_depth) {
    Tys.push_back(getIntTy());
    Ops.push_back(Component);
  }

  /* access: always 0 = sample */
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(0));

  std::string FnName = "air.gather_";
  FnName += TexInfo.air_symbol_suffix;
  FnName += getTypeOverloadSuffix(getTexelGatherType(Texture), getTexelSign(Texture));

  auto Fn = getModule()->getOrInsertFunction(
      FnName, FunctionType::get(getTextureGatherResultType(Texture), Tys, false), Attrs
  );

  auto Result = builder.CreateCall(Fn, Ops);

  return {builder.CreateExtractValue(Result, {0u}), builder.CreateExtractValue(Result, {1u})};
};

std::pair<Value *, Value *>
AIRBuilder::CreateGatherCompare(
    const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Reference,
    Value *Offset
) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  if (!TexInfo.is_depth) {
    debug << "invalid operation: compare on non depth texture.\n";
    return {nullptr, nullptr}; // TODO
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {2U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getSamplerHandleType());
  Ops.push_back(Sampler);

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(1));

  Tys.push_back(getTextureSampleCoordType(Texture));
  Ops.push_back(Coord);

  if (TexInfo.is_array) {
    Tys.push_back(getIntTy());
    Ops.push_back(ArrayIndex);
  }

  Tys.push_back(getFloatTy());
  Ops.push_back(Reference);

  switch (Texture.kind) {
  case Texture::depth2d:
  case Texture::depth2d_array:
    Tys.push_back(getBoolTy());
    Ops.push_back(getBool(true));
    Tys.push_back(getIntTy(2));
    Ops.push_back(Offset);
    break;
  default: // cube doesn't support offset, and others don't support gather_c
    break;
  }

  /* access: always 0 = sample */
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(0));

  std::string FnName = "air.gather_compare_";
  FnName += TexInfo.air_symbol_suffix;
  FnName += ".f32";

  auto Fn = getModule()->getOrInsertFunction(
      FnName, FunctionType::get(getTextureGatherResultType(Texture), Tys, false), Attrs
  );

  auto Result = builder.CreateCall(Fn, Ops);

  return {builder.CreateExtractValue(Result, {0u}), builder.CreateExtractValue(Result, {1u})};
};

Value *
AIRBuilder::CreateTextureQuery(const Texture &Texture, Value *Handle, Texture::Query Query, Value *Level) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  static const char *QUERYS[] = {
      "get_width", "get_height", "get_depth", "get_array_size", "get_num_mip_levels", "get_num_samples",
  };

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  switch (Query) {
  case Texture::width:
  case Texture::height:
  case Texture::depth:
    if (TexInfo.is_mipmaped) {
      Tys.push_back(getIntTy());
      Ops.push_back(Level);
    }
    break;
  default:
    break;
  }

  std::string FnName = "air.";
  FnName += QUERYS[Query];
  FnName += "_";
  FnName += TexInfo.air_symbol_suffix;

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(getIntTy(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

std::pair<Value *, Value *>
AIRBuilder::CreateCalculateLOD(const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {2U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::Convergent)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  Tys.push_back(getSamplerHandleType());
  Ops.push_back(Sampler);

  Tys.push_back(getTextureSampleCoordType(Texture));
  Ops.push_back(Coord);

  /* access: always 0 = sample */
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(0));

  std::string FnName = "air.calculate_clamped_lod_";
  FnName += TexInfo.air_symbol_suffix;

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(getFloatTy(), Tys, false), Attrs);

  std::string FnUnclampedName = "air.calculate_unclamped_lod_";
  FnUnclampedName += TexInfo.air_symbol_suffix;

  auto FnUnclamped =
      getModule()->getOrInsertFunction(FnUnclampedName, FunctionType::get(getFloatTy(), Tys, false), Attrs);

  return {builder.CreateCall(Fn, Ops), builder.CreateCall(FnUnclamped, Ops)};
}

CallInst *
AIRBuilder::CreateTextureFence(const Texture &Texture, Value *Handle) {
  assert(Texture.kind <= Texture::last_resource_kind);
  auto &TexInfo = TextureInfo[Texture.kind];

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::MustProgress)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getTextureHandleType(Texture));
  Ops.push_back(Handle);

  std::string FnName = "air.fence_";
  FnName += TexInfo.air_symbol_suffix;

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(getVoidTy(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

Value *
AIRBuilder::CreateGetNumSamples() {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context,
      {
          {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
          {~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
      }
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(0));

  auto Fn =
      getModule()->getOrInsertFunction("air.get_num_samples.i32", FunctionType::get(getIntTy(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

Value *
AIRBuilder::CreateDerivative(Value *Val, bool YAxis) {
  if (!Val->getType()->getScalarType()->isFloatingPointTy()) {
    debug << "invalid operation: derivative on non-fp type.\n";
    return nullptr; // TODO
  }
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(Val->getType());
  Ops.push_back(Val);

  std::string FnName = "air.";
  FnName += YAxis ? "dfdy" : "dfdx";
  FnName += getTypeOverloadSuffix(Val->getType(), Signedness::DontCare);

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(Val->getType(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

CallInst *
AIRBuilder::CreateDiscard() {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  auto Fn = getModule()->getOrInsertFunction("air.discard_fragment", FunctionType::get(getVoidTy(), {}, false), Attrs);

  return builder.CreateCall(Fn, {});
}

CallInst *
AIRBuilder::CreateBarrier(MemFlags Flags) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::Convergent)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(uint32_t(Flags)));
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(1)); // always 1 ?

  auto Fn = getModule()->getOrInsertFunction("air.wg.barrier", FunctionType::get(getVoidTy(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

CallInst *
AIRBuilder::CreateAtomicFence(MemFlags Flags, ThreadScope Scope, bool Relaxed) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::MustProgress)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(getIntTy());
  Ops.push_back(getInt(uint32_t(Flags)));
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(Relaxed ? 0 : 5));
  Tys.push_back(getIntTy());
  Ops.push_back(getInt(uint32_t(Scope)));

  auto Fn = getModule()->getOrInsertFunction("air.atomic.fence", FunctionType::get(getVoidTy(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

Value *
AIRBuilder::CreateFMA(Value *X, Value *Y, Value *Z) {
  if (!X->getType()->getScalarType()->isFloatingPointTy()) {
    debug << "invalid operation: fma on non-fp type.\n";
    return nullptr; // TODO
  }
  if (X->getType() != Y->getType() || X->getType() != Z->getType()) {
    debug << "invalid operation: fma has unmatched operand types.\n";
    return nullptr; // TODO
  }
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(X->getType());
  Ops.push_back(X);
  Tys.push_back(Y->getType());
  Ops.push_back(Y);
  Tys.push_back(Z->getType());
  Ops.push_back(Z);

  std::string FnName = "air.fma";
  FnName += getTypeOverloadSuffix(X->getType());

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(X->getType(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

Value *
AIRBuilder::CreateDotProduct(Value *LHS, Value *RHS) {
  if (!LHS->getType()->getScalarType()->isFloatingPointTy()) {
    debug << "invalid operation: dotproduct on non-fp type.\n";
    return nullptr; // TODO
  }
  if (LHS->getType() != RHS->getType()) {
    debug << "invalid operation: dotproduct has unmatched operand types.\n";
    return nullptr; // TODO
  }

  // just in case
  if (!LHS->getType()->isVectorTy()) {
    return builder.CreateFMul(LHS, RHS);
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(LHS->getType());
  Ops.push_back(LHS);
  Tys.push_back(RHS->getType());
  Ops.push_back(RHS);

  std::string FnName = "air.dot";
  FnName += getTypeOverloadSuffix(LHS->getType());

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(getFloatTy(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

Value *
AIRBuilder::CreateCountZero(Value *Val, bool TrailingZero) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(Val->getType());
  Ops.push_back(Val);
  Tys.push_back(getBoolTy());
  Ops.push_back(getBool(false));

  std::string FnName = "air.";
  FnName += TrailingZero ? "ctz" : "clz";
  FnName += getTypeOverloadSuffix(Val->getType());

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(Val->getType(), Tys, false), Attrs);

  return builder.CreateCall(Fn, Ops);
}

Value *
AIRBuilder::CreateConvertToFloat(Value *Val, Signedness SrcSign) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  Type *TySrc = Val->getType();
  Type *TyDst = isa<FixedVectorType>(TySrc) ? getFloatTy(cast<FixedVectorType>(TySrc)->getNumElements()) : getFloatTy();

  std::string FnName = "air.convert";
  FnName += getTypeOverloadSuffix(TyDst, Signedness::Signed);
  FnName += getTypeOverloadSuffix(TySrc, SrcSign);

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(TyDst, {TySrc}, false), Attrs);

  return builder.CreateCall(Fn, {Val});
}

Value *
AIRBuilder::CreateConvertToHalf(Value *Val, Signedness SrcSign) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  Type *TySrc = Val->getType();
  Type *TyDst = isa<FixedVectorType>(TySrc) ? getHalfTy(cast<FixedVectorType>(TySrc)->getNumElements()) : getHalfTy();

  std::string FnName = "air.convert";
  FnName += getTypeOverloadSuffix(TyDst, Signedness::Signed);
  FnName += getTypeOverloadSuffix(TySrc, SrcSign);

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(TyDst, {TySrc}, false), Attrs);

  return builder.CreateCall(Fn, {Val});
}

Value *
AIRBuilder::CreateConvertToSigned(Value *Val) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  Type *TySrc = Val->getType();
  Type *TyDst = isa<FixedVectorType>(TySrc) ? getIntTy(cast<FixedVectorType>(TySrc)->getNumElements()) : getIntTy();

  std::string FnName = "air.convert";
  FnName += getTypeOverloadSuffix(TyDst, Signedness::Signed);
  FnName += getTypeOverloadSuffix(TySrc, Signedness::Signed);

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(TyDst, {TySrc}, false), Attrs);

  return builder.CreateCall(Fn, {Val});
}

Value *
AIRBuilder::CreateConvertToUnsigned(Value *Val) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  Type *TySrc = Val->getType();
  Type *TyDst = isa<FixedVectorType>(TySrc) ? getIntTy(cast<FixedVectorType>(TySrc)->getNumElements()) : getIntTy();

  std::string FnName = "air.convert";
  FnName += getTypeOverloadSuffix(TyDst, Signedness::Unsigned);
  FnName += getTypeOverloadSuffix(TySrc, Signedness::Unsigned);

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(TyDst, {TySrc}, false), Attrs);

  return builder.CreateCall(Fn, {Val});
}

CallInst *
AIRBuilder::CreateDeviceCoherentLoad(Type *Ty, Value *Ptr) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoFree)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoSync)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  Type *TyPtr = Ptr->getType();

  std::string FnName = "air.load.device_coherent";
  FnName += getTypeOverloadSuffix(Ty, Signedness::DontCare);
  FnName += getTypeOverloadSuffix(TyPtr, Signedness::DontCare);

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(Ty, {TyPtr}, false), Attrs);

  return builder.CreateCall(Fn, {Ptr});
}

CallInst *
AIRBuilder::CreateDeviceCoherentStore(Value *Val, Value *Ptr) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {2U, Attribute::get(Context, Attribute::AttrKind::WriteOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoFree)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoSync)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  Type *Ty = Val->getType();
  Type *TyPtr = Ptr->getType();

  std::string FnName = "air.store.device_coherent";
  FnName += getTypeOverloadSuffix(Ty, Signedness::DontCare);
  FnName += getTypeOverloadSuffix(TyPtr, Signedness::DontCare);

  auto Fn = getModule()->getOrInsertFunction(FnName, FunctionType::get(getVoidTy(), {Ty, TyPtr}, false), Attrs);

  return builder.CreateCall(Fn, {Val, Ptr});
}

Value *
AIRBuilder::CreateInterpolateAtCenter(Value *Interpoant, bool Perspective) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      Perspective ? "air.interpolate_center_perspective.v4f32" : "air.interpolate_center_no_perspective.v4f32",
      FunctionType::get(getFloatTy(4), {Interpoant->getType()}, false), Attrs
  );

  return builder.CreateCall(Fn, {Interpoant});
}

Value *
AIRBuilder::CreateInterpolateAtCentroid(Value *Interpoant, bool Perspective) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      Perspective ? "air.interpolate_centroid_perspective.v4f32" : "air.interpolate_centroid_no_perspective.v4f32",
      FunctionType::get(getFloatTy(4), {Interpoant->getType()}, false), Attrs
  );

  return builder.CreateCall(Fn, {Interpoant});
}

Value *
AIRBuilder::CreateInterpolateAtSample(Value *Interpoant, Value *SampleIndex, bool Perspective) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      Perspective ? "air.interpolate_sample_perspective.v4f32" : "air.interpolate_sample_no_perspective.v4f32",
      FunctionType::get(getFloatTy(4), {Interpoant->getType(), getIntTy()}, false), Attrs
  );

  return builder.CreateCall(Fn, {Interpoant, SampleIndex});
}

Value *
AIRBuilder::CreateInterpolateAtOffset(Value *Interpoant, Value *Offset, bool Perspective) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {1U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      Perspective ? "air.interpolate_offset_perspective.v4f32" : "air.interpolate_offset_no_perspective.v4f32",
      FunctionType::get(getFloatTy(4), {Interpoant->getType(), getIntTy(2)}, false), Attrs
  );

  return builder.CreateCall(Fn, {Interpoant, Offset});
}

CallInst *
AIRBuilder::CreateSetMeshProperties(Value *GridSizeVec3) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::MustProgress)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      "air.set_threadgroups_per_grid_mesh_properties",
      FunctionType::get(getVoidTy(), {getMeshGridPropsType(), getIntTy(3)}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshGridProps(), GridSizeVec3});
}

CallInst *
AIRBuilder::CreateSetMeshRenderTargetArrayIndex(Value *Vertex, Value *ArrayIndex) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      "air.set_render_target_array_index_mesh.i32",
      FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy(), getIntTy()}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), Vertex, ArrayIndex});
}

CallInst *
AIRBuilder::CreateSetMeshViewportArrayIndex(Value *Vertex, Value *ArrayIndex) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      "air.set_viewport_array_index_mesh.i32",
      FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy(), getIntTy()}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), Vertex, ArrayIndex});
}

CallInst *
AIRBuilder::CreateSetMeshPosition(Value *Vertex, Value *Position) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      "air.set_position_mesh", FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy(), getFloatTy(4)}, false),
      Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), Vertex, SanitizePosition(Position)});
}

CallInst *
AIRBuilder::CreateSetMeshClipDistance(Value *Vertex, Value *Index, Value *Value) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      "air.set_clip_distance_mesh",
      FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy(), getIntTy(), getFloatTy()}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), Index, Vertex, Value});
}

CallInst *
AIRBuilder::CreateSetMeshVertexData(Value *Vertex, Value *DataIndex, Value *DataValue) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  std::string FnName = "air.set_vertex_data_mesh";
  FnName += getTypeOverloadSuffix(DataValue->getType());

  auto Fn = getModule()->getOrInsertFunction(
      FnName,
      FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy(), getIntTy(), DataValue->getType()}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), DataIndex, Vertex, DataValue});
}

CallInst *
AIRBuilder::CreateSetMeshPrimitiveData(Value *Primitive, Value *DataIndex, Value *DataValue) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  std::string FnName = "air.set_primitive_data_mesh";
  FnName += getTypeOverloadSuffix(DataValue->getType());

  auto Fn = getModule()->getOrInsertFunction(
      FnName,
      FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy(), getIntTy(), DataValue->getType()}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), DataIndex, Primitive, DataValue});
}

CallInst *
AIRBuilder::CreateSetMeshIndex(Value *Index, Value *Vertex) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      "air.set_index_mesh", FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy(), getByteTy()}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), Index, builder.CreateZExtOrTrunc(Vertex, getByteTy())});
}

CallInst *
AIRBuilder::CreateSetMeshPrimitiveCount(Value *Count) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      "air.set_primitive_count_mesh", FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy()}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), Count});
}

CallInst *
AIRBuilder::CreateSetMeshPointSize(Value *Vertex, Value *Size) {
  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ArgMemOnly)}}
  );

  std::string FnName = "air.set_point_size_mesh";

  auto Fn = getModule()->getOrInsertFunction(
      FnName, FunctionType::get(getVoidTy(), {getMeshHandleType(), getIntTy(), getFloatTy()}, false), Attrs
  );

  return builder.CreateCall(Fn, {getMeshHandle(), Vertex, Size});
}

Value *
AIRBuilder::CreateFPUnOp(FPUnOp Op, Value *Operand) {
  static char const *FnNames[] = {
      "saturate", "log2", "exp2", "sqrt", "rsqrt", "fract", "rint", "floor", "ceil", "trunc", "cos", "sin",
  };

  if (uint32_t(Op) >= std::size(FnNames)) {
    debug << "invalid operation: unknown fp unary operation.\n";
    return nullptr; // TODO
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)}}
  );
  auto OperandType = Operand->getType();

  std::string FnName = "air.";
  if (builder.getFastMathFlags().any())
    FnName += "fast_";
  FnName += FnNames[Op];
  FnName += getTypeOverloadSuffix(OperandType);
  auto Fn = getModule()->getOrInsertFunction(FnName, llvm::FunctionType::get(OperandType, {OperandType}, false), Attrs);
  return builder.CreateCall(Fn, {Operand});
}

Value *
AIRBuilder::CreateFPBinOp(FPBinOp Op, Value *LHS, Value *RHS) {
  static char const *FnNames[] = {
      "fmax",
      "fmin",
  };

  if (uint32_t(Op) >= std::size(FnNames)) {
    debug << "invalid operation: unknown fp binary operation.\n";
    return nullptr; // TODO
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)}}
  );
  auto OperandType = LHS->getType();

  std::string FnName = "air.";
  if (builder.getFastMathFlags().any())
    FnName += "fast_";
  FnName += FnNames[Op];
  FnName += getTypeOverloadSuffix(OperandType);
  auto Fn = getModule()->getOrInsertFunction(
      FnName, llvm::FunctionType::get(OperandType, {OperandType, OperandType}, false), Attrs
  );
  return builder.CreateCall(Fn, {LHS, RHS});
}

Value *
AIRBuilder::CreateIntUnOp(IntUnOp Op, Value *Operand) {
  static char const *FnNames[] = {
      "reverse_bits",
      "popcount",
  };

  if (uint32_t(Op) >= std::size(FnNames)) {
    debug << "invalid operation: unknown integer unary operation.\n";
    return nullptr; // TODO
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)}}
  );
  auto OperandType = Operand->getType();

  std::string FnName = "air.";
  FnName += FnNames[Op];
  FnName += getTypeOverloadSuffix(OperandType);
  auto Fn = getModule()->getOrInsertFunction(FnName, llvm::FunctionType::get(OperandType, {OperandType}, false), Attrs);
  return builder.CreateCall(Fn, {Operand});
}

Value *
AIRBuilder::CreateIntBinOp(IntBinOp Op, Value *LHS, Value *RHS, bool Signed) {
  static char const *FnNames[] = {
      "max",
      "min",
  };

  if (uint32_t(Op) >= std::size(FnNames)) {
    debug << "invalid operation: unknown integer binary operation.\n";
    return nullptr; // TODO
  }

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)}}
  );
  auto OperandType = LHS->getType();

  std::string FnName = "air.";
  FnName += FnNames[Op];
  FnName += getTypeOverloadSuffix(OperandType, Signed ? Signedness::Signed : Signedness::Unsigned);
  auto Fn = getModule()->getOrInsertFunction(
      FnName, llvm::FunctionType::get(OperandType, {OperandType, OperandType}, false), Attrs
  );
  return builder.CreateCall(Fn, {LHS, RHS});
}

Value *
AIRBuilder::CreateAtomicRMW(AtomicRMWInst::BinOp Op, Value *Ptr, Value *Val) {
  std::string FnName = "air.atomic.";
  auto TyPtr = dyn_cast<PointerType>(Ptr->getType());
  if (!TyPtr) {
    debug << "invalid operation: atomicrmw: not a pointer.\n";
    return nullptr; // TODO
  }
  if (TyPtr->getNonOpaquePointerElementType() != Val->getType()) {
    debug << "invalid operation: atomicrmw: mismatched atomic operands.\n";
    return nullptr; // TODO
  }
  Value *MemFlags = nullptr;
  switch (TyPtr->getAddressSpace()) {
  case 1:
    FnName += "global.";
    MemFlags = getInt(3);
    break;
  case 3:
    FnName += "local.";
    MemFlags = getInt(1);
    break;
  default:
    debug << "invalid operation: atomicrmw: not a valid address space.\n";
    return nullptr; // TODO
  }
  Signedness Sign = Signedness::Unsigned;
  switch (Op) {
  case llvm::AtomicRMWInst::Add:
    FnName += "add";
    break;
  case llvm::AtomicRMWInst::Sub:
    FnName += "sub";
    break;
  case llvm::AtomicRMWInst::And:
    FnName += "and";
    break;
  case llvm::AtomicRMWInst::Or:
    FnName += "or";
    break;
  case llvm::AtomicRMWInst::Xor:
    FnName += "xor";
    break;
  case llvm::AtomicRMWInst::Max:
    Sign = Signedness::Signed;
    FnName += "max";
    break;
  case llvm::AtomicRMWInst::Min:
    Sign = Signedness::Signed;
    FnName += "min";
    break;
  case llvm::AtomicRMWInst::UMax:
    FnName += "max";
    break;
  case llvm::AtomicRMWInst::UMin:
    FnName += "min";
    break;
  case llvm::AtomicRMWInst::Xchg:
    FnName += "xchg";
    Sign = Signedness::DontCare;
    break;
  default:
    FnName += "<invalid atomic op>";
    break;
  }
  auto TyOp = TyPtr->getNonOpaquePointerElementType();
  FnName += getTypeOverloadSuffix(TyPtr->getNonOpaquePointerElementType(), Sign);

  auto &Context = getContext();
  auto Attrs = AttributeList::get(
      Context, {{1U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  auto Fn = getModule()->getOrInsertFunction(
      FnName, llvm::FunctionType::get(TyOp, {TyPtr, TyOp, getIntTy(), getIntTy(), getBoolTy()}, false), Attrs
  );
  return builder.CreateCall(Fn, {Ptr, Val, getInt(0), MemFlags, getBool(true)});
}

llvm::Value *
AIRBuilder::SanitizePosition(llvm::Value *Pos) {
  // isfinite(Pos)
  auto Mask = builder.CreateICmpNE(
      builder.CreateAnd(builder.CreateBitCast(Pos, getIntTy(4)), 0x7f800000ull),
      getInt4(0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000)
  );
  auto ValidPos = builder.CreateAnd(
      builder.CreateAnd(builder.CreateExtractElement(Mask, 0ull), builder.CreateExtractElement(Mask, 1ull)),
      builder.CreateAnd(builder.CreateExtractElement(Mask, 2ull), builder.CreateExtractElement(Mask, 3ull))
  );
  auto PosClipped = llvm::ConstantVector::get({getFloat(0.0f), getFloat(0.0f), getFloat(1.0f), getFloat(0.0f)});
  return builder.CreateSelect(ValidPos, Pos, PosClipped);
}

} // namespace llvm::air