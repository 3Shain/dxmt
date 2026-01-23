
#pragma once

#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"

namespace llvm::air {

struct sample_level {
  Value *lod;
};

struct sample_min_lod_clamp {
  Value *lod;
};

struct sample_bias {
  Value *bias;
};

class Texture {
public:
  enum ResourceKind {
    texture_buffer,
    texture1d,
    texture1d_array,
    texture2d,
    texture2d_array,
    texture3d,
    texturecube,
    texturecube_array,
    texture2d_ms,
    texture2d_ms_array,
    depth2d,
    depth2d_array,
    depthcube,
    depthcube_array,
    depth_2d_ms,
    depth_2d_ms_array,
    // woops, some mistakes made before
    texture_1d = texture1d,
    texture_1d_array = texture1d_array,
    texture_2d = texture2d,
    texture_2d_array = texture2d_array,
    texture_3d = texture3d,
    texture_cube = texturecube,
    texture_cube_array = texturecube_array,
    texture_2d_ms = texture2d_ms,
    texture_2d_ms_array = texture2d_ms_array,
    depth_2d = depth2d,
    depth_2d_array = depth2d_array,
    depth_cube = depthcube,
    depth_cube_array = depthcube_array,
    depth2d_ms = depth_2d_ms,
    depth2d_ms_array = depth_2d_ms_array,
    last_resource_kind = depth2d_ms_array,
  };

  enum SampleType {
    sample_float,
    sample_int,
    sample_uint,
    sample_half,
  };

  enum MemoryAccess {
    access_sample,
    access_read,
    access_write,
    acesss_readwrite,
  };

  enum Query {
    width,
    height,
    depth,
    array_length,
    num_mip_levels,
    num_samples,
  };

  enum ResourceKind kind;
  enum SampleType sample_type;
  enum MemoryAccess memory_access;
};

enum class Signedness { DontCare, Signed, Unsigned };

enum class MemFlags {
  None = 0,
  Device = 1,
  Threadgroup = 2,
  Texture = 4,
  ObjectData = 4,
};

enum class ThreadScope {
  Thread = 0,
  Threadgroup = 1,
  Device = 2,
  Simdgroup = 4,
};

class AIRBuilder {
public:
  AIRBuilder(IRBuilderBase &ir_builder, raw_ostream &debug) : builder(ir_builder), debug(debug) {}

  /* Texture Operations */

  std::pair<Value *, Value *>
  CreateSample(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, const int32_t Offset[3]
  ) {
    return CreateSampleCommon(Texture, Handle, Sampler, Coord, ArrayIndex, Offset, false, getFloat(0), getFloat(0));
  }

  std::pair<Value *, Value *>
  CreateSample(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, const int32_t Offset[3],
      sample_level Level
  ) {
    return CreateSampleCommon(Texture, Handle, Sampler, Coord, ArrayIndex, Offset, true, Level.lod, getFloat(0));
  }

  std::pair<Value *, Value *>
  CreateSample(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, const int32_t Offset[3],
      sample_bias Bias
  ) {
    return CreateSampleCommon(Texture, Handle, Sampler, Coord, ArrayIndex, Offset, false, Bias.bias, getFloat(0));
  }

  std::pair<Value *, Value *>
  CreateSample(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, const int32_t Offset[3],
      sample_bias Bias, sample_min_lod_clamp MinLOD
  ) {
    return CreateSampleCommon(Texture, Handle, Sampler, Coord, ArrayIndex, Offset, false, Bias.bias, MinLOD.lod);
  }

  std::pair<Value *, Value *>
  CreateSample(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, const int32_t Offset[3],
      sample_min_lod_clamp MinLOD
  ) {
    return CreateSampleCommon(Texture, Handle, Sampler, Coord, ArrayIndex, Offset, false, getFloat(0), MinLOD.lod);
  }

  std::pair<Value *, Value *>
  CreateSampleCmp(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Reference,
      const int32_t Offset[3], sample_bias Bias, sample_min_lod_clamp MinLOD
  ) {
    return CreateSampleCmpCommon(
        Texture, Handle, Sampler, Coord, ArrayIndex, Reference, Offset, false, Bias.bias, MinLOD.lod
    );
  }

  std::pair<Value *, Value *>
  CreateSampleCmp(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Reference,
      const int32_t Offset[3], sample_level Level
  ) {
    return CreateSampleCmpCommon(
        Texture, Handle, Sampler, Coord, ArrayIndex, Reference, Offset, true, Level.lod, getFloat(0)
    );
  }

  std::pair<Value *, Value *>
  CreateSampleGrad(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *DerivX,
      Value *DerivY, const int32_t Offset[3]
  ) {
    return CreateSampleGrad(Texture, Handle, Sampler, Coord, ArrayIndex, DerivX, DerivY, getFloat(0), Offset);
  }

  std::pair<Value *, Value *> CreateSampleGrad(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *DerivX,
      Value *DerivY, Value *MinLOD, const int32_t Offset[3]
  );

  Value *CreateAtomicRMW(
      const Texture &Texture, Value *Handle, AtomicRMWInst::BinOp Op, Value *Pos, Value *ValVec4,
      Value *ArrayIndex = nullptr
  );

  std::pair<Value *, Value *> CreateRead(
      const Texture &Texture, Value *Handle, Value *Pos, Value *ArrayIndex, Value *SampleIndexOrCubeFace, Value *Level,
      bool DeviceCoherent = false
  );

  CallInst *CreateWrite(
      const Texture &Texture, Value *Handle, Value *Pos, Value *ArrayIndex, Value *CubeFace, Value *Level,
      Value *ValVec4, bool DeviceCoherent = false
  );

  std::pair<Value *, Value *> CreateAtomicCmpXchg(
      const Texture &Texture, Value *Handle, Value *Pos, Value *CmpVec4, Value *NewVec4, Value *ArrayIndex = nullptr
  );

  std::pair<Value *, Value *>
  CreateGather(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, const int32_t Offset[3],
      Value *Component
  ) {
    return CreateGather(Texture, Handle, Sampler, Coord, ArrayIndex, GetOffset(Texture, Offset), Component);
  }

  std::pair<Value *, Value *> CreateGather(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Offset,
      Value *Component
  );

  std::pair<Value *, Value *>
  CreateGatherCompare(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Reference,
      const int32_t Offset[3]
  ) {
    return CreateGatherCompare(Texture, Handle, Sampler, Coord, ArrayIndex, Reference, GetOffset(Texture, Offset));
  }

  std::pair<Value *, Value *> CreateGatherCompare(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Reference,
      Value *Offset
  );

  Value *CreateTextureQuery(const Texture &Texture, Value *Handle, Texture::Query Query, Value *Level);

  /**
  \returns (float clamped_lod, float unclamped_lod)
  */
  std::pair<Value *, Value *> CreateCalculateLOD(const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord);

  CallInst *CreateTextureFence(const Texture &Texture, Value *Handle);

  /* Fragment Shader */

  Value *CreateGetNumSamples();

  Value *CreateDerivative(Value *Val, bool YAxis);

  CallInst *CreateDiscard();

  /* Compute Shader */

  CallInst *CreateBarrier(MemFlags Flags);

  CallInst *CreateAtomicFence(MemFlags Flags, ThreadScope Scope, bool Relaxed = false);

  /* Math */

  Value *CreateFMA(Value *X, Value *Y, Value *Z);

  Value *CreateDotProduct(Value *LHS, Value *RHS);

  Value *CreateCountZero(Value *Val, bool TrailingZero);

  /* Miscs */

  Value *CreateConvertToFloat(Value *Val, Signedness SrcSign = Signedness::Signed);
  Value *CreateConvertToHalf(Value *Val, Signedness SrcSign = Signedness::Signed);
  Value *CreateConvertToSigned(Value *Val);
  Value *CreateConvertToUnsigned(Value *Val);

  CallInst *CreateDeviceCoherentLoad(Type *Ty, Value *Ptr);
  CallInst *CreateDeviceCoherentStore(Value *Val, Value *Ptr);

  /* Pull-based Interpolation */

  Value *CreateInterpolateAtCenter(Value *Interpoant, bool Perspective);

  Value *CreateInterpolateAtCentroid(Value *Interpoant, bool Perspective);

  Value *CreateInterpolateAtSample(Value *Interpoant, Value *SampleIndex, bool Perspective);

  Value *CreateInterpolateAtOffset(Value *Interpoant, Value *Offset, bool Perspective);

  /* Object Shader */

  CallInst *CreateSetMeshProperties(Value *GridSizeVec3);

  /* Mesh Shader */

  CallInst *CreateSetMeshRenderTargetArrayIndex(Value *Vertex, Value *ArrayIndex);

  CallInst *CreateSetMeshViewportArrayIndex(Value *Vertex, Value *ArrayIndex);

  CallInst *CreateSetMeshPosition(Value *Vertex, Value *Position);

  CallInst *CreateSetMeshClipDistance(Value *Vertex, Value *Index, Value *Value);

  CallInst *CreateSetMeshVertexData(Value *Vertex, Value *DataIndex, Value *DataValue);

  CallInst *
  CreateSetMeshVertexData(Value *Vertex, uint32_t DataIndex, Value *DataValue) {
    return CreateSetMeshVertexData(Vertex, getInt(DataIndex), DataValue);
  }

  CallInst *CreateSetMeshPrimitiveData(Value *Primitive, Value *DataIndex, Value *DataValue);

  CallInst *CreateSetMeshIndex(Value *Index, Value *Vertex);

  CallInst *CreateSetMeshPrimitiveCount(Value *Count);

  CallInst *CreateSetMeshPointSize(Value *Vertex, Value *Size);

  enum FPUnOp {
    saturate,
    log2,
    exp2,
    sqrt,
    rsqrt,
    fract,
    rint,
    floor,
    ceil,
    trunc,
    cos,
    sin,
    fabs,
  };

  Value *CreateFPUnOp(FPUnOp Op, Value *Operand, bool FastVariant = true);

  enum FPBinOp {
    fmax,
    fmin,
  };

  Value *CreateFPBinOp(FPBinOp Op, Value *LHS, Value *RHS, bool FastVariant = true);

  enum IntUnOp {
    reverse_bits,
    popcount,
  };

  Value *CreateIntUnOp(IntUnOp Op, Value *Operand);

  enum IntBinOp {
    max,
    min,
    mul_hi,
  };

  Value *CreateIntBinOp(IntBinOp Op, Value *LHS, Value *RHS, bool Signed = false);

  Value *CreateAtomicRMW(AtomicRMWInst::BinOp Op, Value *Ptr, Value *Val);

  llvm::Value *SanitizePosition(llvm::Value *Pos);

  /* Useful Helpers */

  ConstantFP *
  getFloat(float Value) {
    return ConstantFP::get(getContext(), APFloat{Value});
  }
  Constant *
  getFloat2(float Value1, float Value2) {
    return cast<Constant>(ConstantVector::get({getFloat(Value1), getFloat(Value2)}));
  }
  // Value *getFloat3(float Value1, float Value2, float Value3);
  // Value *getFloat4(float Value1, float Value2, float Value3, float Value4);

  ConstantFP *
  getHalf(uint16_t Value) {
    return ConstantFP::get(getContext(), APFloat{APFloat::IEEEhalf(), APInt{16, Value}});
  }

  ConstantInt *
  getInt(uint32_t Value) {
    return builder.getInt32(Value);
  }
  Constant *
  getInt2(uint32_t Value1, uint32_t Value2) {
    return cast<Constant>(ConstantVector::get({builder.getInt32(Value1), builder.getInt32(Value2)}));
  }
  Constant *
  getInt3(uint32_t Value1, uint32_t Value2, uint32_t Value3) {
    return cast<Constant>(
        ConstantVector::get({builder.getInt32(Value1), builder.getInt32(Value2), builder.getInt32(Value3)})
    );
  }
  Constant *
  getInt4(uint32_t Value1, uint32_t Value2, uint32_t Value3, uint32_t Value4) {
    return cast<Constant>(ConstantVector::get(
        {builder.getInt32(Value1), builder.getInt32(Value2), builder.getInt32(Value3), builder.getInt32(Value4)}
    ));
  }

  ConstantInt *
  getBool(bool Value) {
    return builder.getInt1(Value);
  };

  IRBuilderBase &builder;

  Module *
  getModule() const {
    assert(builder.GetInsertBlock()->getParent() && "");
    assert(builder.GetInsertBlock()->getParent()->getParent() && "");
    return builder.GetInsertBlock()->getParent()->getParent();
  }

  LLVMContext &
  getContext() const {
    return builder.getContext();
  };

  Type *
  getOrCreateStructType(const StringRef &Name) {
    StructType *Ty = StructType::getTypeByName(getContext(), Name);
    if (Ty)
      return Ty;
    return StructType::create(getContext(), Name);
  }

  Type *
  getIntTy() {
    return Type::getInt32Ty(getContext());
  };
  Type *
  getFloatTy() {
    return Type::getFloatTy(getContext());
  };
  Type *
  getHalfTy() {
    return Type::getHalfTy(getContext());
  };
  Type *
  getIntTy(uint32_t dim) {
    assert(dim > 0 && dim <= 4);
    if (dim == 1)
      return getIntTy();
    return FixedVectorType::get(getIntTy(), dim);
  };
  Type *
  getFloatTy(uint32_t dim) {
    assert(dim > 0 && dim <= 4);
    if (dim == 1)
      return getFloatTy();
    return FixedVectorType::get(getFloatTy(), dim);
  };
  Type *
  getHalfTy(uint32_t dim) {
    assert(dim > 0 && dim <= 4);
    if (dim == 1)
      return getHalfTy();
    return FixedVectorType::get(getHalfTy(), dim);
  };
  Type *
  getBoolTy() {
    return Type::getInt1Ty(getContext());
  };
  Type *
  getVoidTy() {
    return Type::getVoidTy(getContext());
  };
  Type *
  getByteTy() {
    return Type::getInt8Ty(getContext());
  };

  Type *getTextureHandleType(const Texture &Texture);
  Type *getTexelType(const Texture &Texture);
  Type *getTexelGatherType(const Texture &Texture);
  Type *getTextureSampleCoordType(const Texture &Texture);
  Type *getTextureRWPositionType(const Texture &Texture);

  Type *
  getTextureSampleResultType(const Texture &Texture) {
    return StructType::get(getContext(), {getTexelType(Texture), Type::getInt8Ty(getContext())});
  }

  Type *
  getTextureGatherResultType(const Texture &Texture) {
    return StructType::get(getContext(), {getTexelGatherType(Texture), Type::getInt8Ty(getContext())});
  }

  Type *
  getSamplerHandleType() {
    return getOrCreateStructType("struct._sampler_t")->getPointerTo(2);
  }
  Type *
  getMeshHandleType() {
    return getOrCreateStructType("struct._mesh_t")->getPointerTo(7);
  }
  Type *
  getMeshGridPropsType() {
    return getOrCreateStructType("struct._mesh_grid_properties_t")->getPointerTo(3);
  }

  Value *
  getMeshHandle() {
    Function *Fn = builder.GetInsertBlock()->getParent();
    if (Fn) {
      Type *MeshTy = getMeshHandleType();
      for (auto &Arg : Fn->args()) {
        if (Arg.getType() == MeshTy) {
          return &Arg;
        }
      }
    }
    return nullptr;
  };

  Value *
  getMeshGridProps() {
    Function *Fn = builder.GetInsertBlock()->getParent();
    if (Fn) {
      Type *PropsTy = getMeshGridPropsType();
      for (auto &Arg : Fn->args()) {
        if (Arg.getType() == PropsTy) {
          return &Arg;
        }
      }
    }
    return nullptr;
  };

  std::string getTypeOverloadSuffix(Type *Ty, Signedness Sign = Signedness::DontCare);

protected:
  raw_ostream &debug;

  Value *
  GetOffset(const Texture &Texture, const int32_t ImmOffset[3]) {
    switch (Texture.kind) {
    case Texture::texture1d:
    case Texture::texture1d_array:
      return getInt(ImmOffset[0]);
    case Texture::texture2d:
    case Texture::texture2d_array:
    case Texture::depth2d:
    case Texture::depth2d_array:
      return getInt2(ImmOffset[0], ImmOffset[1]);
    case Texture::texture3d:
      return getInt3(ImmOffset[0], ImmOffset[1], ImmOffset[2]);
    default:
      break;
    }
    return nullptr;
  }

  std::pair<Value *, Value *>
  CreateSampleCommon(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, const int32_t Offset[3],
      bool ArgsControlBit, Value *Args1, Value *Args2
  ) {
    return CreateSampleCommon(
        Texture, Handle, Sampler, Coord, ArrayIndex, GetOffset(Texture, Offset), ArgsControlBit, Args1, Args2
    );
  }

  std::pair<Value *, Value *> CreateSampleCommon(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Offset,
      bool ArgsControlBit, Value *Args1, Value *Args2
  );

  std::pair<Value *, Value *>
  CreateSampleCmpCommon(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Reference,
      const int32_t Offset[3], bool ArgsControlBit, Value *Args1, Value *Args2
  ) {
    return CreateSampleCmpCommon(
        Texture, Handle, Sampler, Coord, ArrayIndex, Reference, GetOffset(Texture, Offset), ArgsControlBit, Args1, Args2
    );
  }

  std::pair<Value *, Value *> CreateSampleCmpCommon(
      const Texture &Texture, Value *Handle, Value *Sampler, Value *Coord, Value *ArrayIndex, Value *Reference,
      Value *Offset, bool ArgsControlBit, Value *Args1, Value *Args2
  );

  Signedness
  getTexelSign(const Texture &Texture) {
    switch (Texture.sample_type) {
    case Texture::sample_int:
      return Signedness::Signed;
    case Texture::sample_uint:
      return Signedness::Unsigned;
    default:
      return Signedness::DontCare;
    }
  }
};

} // namespace llvm::air