#include "dxbc_converter_base.hpp"
#include "../dxbc_converter.hpp"
#include "air_builder.hpp"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/AtomicOrdering.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"

namespace dxmt::dxbc {

llvm::ArrayType *
GetArrayType(llvm::Value *Array) {
  return llvm::cast<llvm::ArrayType>(llvm::cast<llvm::PointerType>(Array->getType())->getNonOpaquePointerElementType());
}

llvm::Value *
Converter::LoadOperandIndex(const IndexByTempComponent &SrcOpIndex) {
  auto Comp = SrcOpIndex.component;
  SrcOperandTemp RelReg{
      ._ = {{Comp, Comp, Comp, Comp}, false, false, OperandDataType::Integer},
      .phase = SrcOpIndex.phase,
      .regid = SrcOpIndex.regid,
  };
  auto Rel = LoadOperand(RelReg, 0b1);
  return ir.CreateAdd(Rel, ir.getInt32(SrcOpIndex.offset));
}

llvm::Value *
Converter::LoadOperandIndex(const IndexByIndexableTempComponent &SrcOpIndex) {
  auto Comp = SrcOpIndex.component;
  SrcOperandIndexableTemp RelReg{
      ._ = {{Comp, Comp, Comp, Comp}, false, false, OperandDataType::Integer},
      .phase = SrcOpIndex.phase,
      .regindex = SrcOpIndex.regid,
      .regfile = SrcOpIndex.regfile,
  };
  auto Rel = LoadOperand(RelReg, 0b1);
  return ir.CreateAdd(Rel, ir.getInt32(SrcOpIndex.offset));
}

llvm::Value *
Converter::LoadOperand(const SrcOperandConstantBuffer &SrcOp, mask_t Mask) {

  auto RangeId = SrcOp.rangeid;

  auto V = res.cb_range_map[RangeId](nullptr).build(ctx);
  if (auto err = V.takeError()) {
    return ApplySrcModifier(SrcOp._, llvm::ConstantAggregateZero::get(air.getIntTy(4)), Mask);
  }
  auto Handle = V.get();

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(air.getIntTy(4), Handle, {LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyIntVec4, Handle, {LoadOperandIndex(SrcOp.regindex)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandImmediateConstantBuffer &SrcOp, mask_t Mask) {
  auto Handle = res.icb;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandImmediate32 &SrcOp, mask_t Mask) {
  llvm::SmallVector<llvm::Constant *> Constants;
  switch (SrcOp._.read_type) {
  case OperandDataType::Float:
    switch (Mask) {
    case 0b1:
      return air.getFloat(SrcOp.fvalue[0]);
    case 0b10:
      return air.getFloat(SrcOp.fvalue[1]);
    case 0b100:
      return air.getFloat(SrcOp.fvalue[2]);
    case 0b1000:
      return air.getFloat(SrcOp.fvalue[3]);
    default:
      for (int i = 0; i < 4; i++) {
        if (Mask & (1 << i)) {
          Constants.push_back(air.getFloat(SrcOp.fvalue[i]));
        }
      }
      break;
    }
    break;
  case OperandDataType::Integer:
    switch (Mask) {
    case 0b1:
      return air.getInt(SrcOp.uvalue[0]);
    case 0b10:
      return air.getInt(SrcOp.uvalue[1]);
    case 0b100:
      return air.getInt(SrcOp.uvalue[2]);
    case 0b1000:
      return air.getInt(SrcOp.uvalue[3]);
    default:
      for (int i = 0; i < 4; i++) {
        if (Mask & (1 << i)) {
          Constants.push_back(air.getInt(SrcOp.uvalue[i]));
        }
      }
      break;
    }
    break;
  case OperandDataType::Half16X16:
    // in case f16to32 reads a literal (doesn't make much sense)
    switch (Mask) {
    case 0b1:
      return air.getHalf(SrcOp.uvalue[0]);
    case 0b10:
      return air.getHalf(SrcOp.uvalue[1]);
    case 0b100:
      return air.getHalf(SrcOp.uvalue[2]);
    case 0b1000:
      return air.getHalf(SrcOp.uvalue[3]);
    default:
      for (int i = 0; i < 4; i++) {
        if (Mask & (1 << i)) {
          Constants.push_back(air.getHalf(SrcOp.uvalue[i]));
        }
      }
      break;
    }
    break;
  default:
    llvm_unreachable("unhandled operand data type");
  }
  return llvm::ConstantVector::get(Constants);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandInput &SrcOp, mask_t Mask) {

  auto Handle = res.input.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), ir.getInt32(SrcOp.regid), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), ir.getInt32(SrcOp.regid)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandTemp &SrcOp, mask_t Mask) {

  auto Handle = res.temp.ptr_int4;

  if (SrcOp.phase != ~0u) {
    Handle = res.phases[SrcOp.phase].temp.ptr_int4;
  }
  auto TyHandle = GetArrayType(Handle);
  auto TyInt = air.getIntTy();

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto Ptr = ir.CreateGEP(
        TyHandle, Handle,
        {ir.getInt32(0), ir.CreateAdd(ir.CreateShl(ir.getInt32(SrcOp.regid), 2), ir.getInt32(Comp))}
    );
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  llvm::Value *ValueIntVec4 = llvm::PoisonValue::get(air.getIntTy(4));
  for (auto [DstComp, _] : EnumerateComponents(MemoryAccessMask(Mask, SrcOp._.swizzle))) {
    auto Ptr = ir.CreateGEP(
        TyHandle, Handle,
        {ir.getInt32(0), ir.CreateAdd(ir.CreateShl(ir.getInt32(SrcOp.regid), 2), ir.getInt32(DstComp))}
    );
    ValueIntVec4 = ir.CreateInsertElement(ValueIntVec4, ir.CreateLoad(TyInt, Ptr), DstComp);
  }
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandAttribute &SrcOp, mask_t Mask) {
  using shader::common::InputAttribute;
  llvm::Value *Ret = llvm::PoisonValue::get(air.getIntTy(4));
  switch (SrcOp.attribute) {
  case InputAttribute::VertexId:
  case InputAttribute::InstanceId:
    assert(0 && "never reached: should be handled separately");
    break;
  case InputAttribute::ThreadId: {
    Ret = res.thread_id_arg;
    break;
  }
  case InputAttribute::ThreadIdInGroup: {
    Ret = res.thread_id_in_group_arg;
    break;
  }
  case InputAttribute::ThreadGroupId: {
    Ret = res.thread_group_id_arg;
    break;
  }
  case InputAttribute::ThreadIdInGroupFlatten: {
    Ret = res.thread_id_in_group_flat_arg;
    break;
  }

  case InputAttribute::OutputControlPointId:
  case InputAttribute::ForkInstanceId:
  case InputAttribute::JoinInstanceId: {
    Ret = res.thread_id_in_patch;
    break;
  }
  case InputAttribute::CoverageMask: {
    Ret = ctx.pso_sample_mask != 0xffffffff ? ir.CreateAnd(res.coverage_mask_arg, ctx.pso_sample_mask)
                                            : res.coverage_mask_arg;
    break;
  }
  case InputAttribute::Domain: {
    Ret = res.domain;
    break;
  }
  case InputAttribute::PrimitiveId: {
    Ret = res.patch_id;
    break;
  }
  case InputAttribute::GSInstanceId: {
    Ret = res.gs_instance_id;
    break;
  }
  }

  return ApplySrcModifier(SrcOp._, Ret, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandIndexableTemp &SrcOp, mask_t Mask) {

  indexable_register_file regfile;

  if (SrcOp.phase != ~0u) {
    regfile = res.phases[SrcOp.phase].indexable_temp_map[SrcOp.regfile];
  } else {
    regfile = res.indexable_temp_map[SrcOp.regfile];
  }

  auto Handle = regfile.ptr_int_vec;
  auto TyHandle = GetArrayType(Handle);
  auto Index = LoadOperandIndex(SrcOp.regindex);
  auto TyInt = air.getIntTy();

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto Ptr = ir.CreateGEP(
        TyHandle, Handle,
        {ir.getInt32(0), ir.CreateAdd(ir.CreateMul(Index, ir.getInt32(regfile.vec_size)), ir.getInt32(Comp))}
    );
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec = air.getIntTy(regfile.vec_size);
  llvm::Value *ValueIntVec = llvm::PoisonValue::get(TyIntVec);
  for (auto [DstComp, _] : EnumerateComponents(MemoryAccessMask(Mask, SrcOp._.swizzle))) {
    auto Ptr = ir.CreateGEP(
        TyHandle, Handle,
        {ir.getInt32(0), ir.CreateAdd(ir.CreateMul(Index, ir.getInt32(regfile.vec_size)), ir.getInt32(DstComp))}
    );
    ValueIntVec = ir.CreateInsertElement(ValueIntVec, ir.CreateLoad(TyInt, Ptr), DstComp);
  }
  return ApplySrcModifier(SrcOp._, ValueIntVec, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandIndexableInput &SrcOp, mask_t Mask) {

  auto Handle = res.input.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandInputICP &SrcOp, mask_t Mask) {
  /* applies to both hull and domain shader, in this case input is a "2d" array */
  auto Handle = res.input.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  auto CPId = LoadOperandIndex(SrcOp.cpid);
  auto RegId = ir.getInt32(SrcOp.regid);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), CPId, RegId, ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), CPId, RegId});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandInputPC &SrcOp, mask_t Mask) {

  auto Handle = res.patch_constant_output.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex), ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), LoadOperandIndex(SrcOp.regindex)});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::LoadOperand(const SrcOperandInputOCP &SrcOp, mask_t Mask) {
  /* applies to both hull and domain shader, in this case input is a "2d" array */
  auto Handle = res.output.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  auto CPId = LoadOperandIndex(SrcOp.cpid);
  auto RegId = ir.getInt32(SrcOp.regid);

  if (auto Comp = ComponentFromScalarMask(Mask, SrcOp._.swizzle); Comp >= 0) {
    auto TyInt = air.getIntTy();
    auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), CPId, RegId, ir.getInt32(Comp)});
    auto ValueInt = ir.CreateLoad(TyInt, Ptr);
    return ApplySrcModifier(SrcOp._, ValueInt, Mask);
  }

  auto TyIntVec4 = air.getIntTy(4);
  auto Ptr = ir.CreateGEP(TyHandle, Handle, {ir.getInt32(0), CPId, RegId});
  auto ValueIntVec4 = ir.CreateLoad(TyIntVec4, Ptr);
  return ApplySrcModifier(SrcOp._, ValueIntVec4, Mask);
}

llvm::Value *
Converter::ApplySrcModifier(SrcOperandCommon C, llvm::Value *Value, mask_t Mask) {
  using namespace llvm::air;

  Value = MaskSwizzle(Value, Mask, C.swizzle);
  switch (C.read_type) {
  case OperandDataType::Float:
    Value = BitcastToFloat(Value);
    if (C.abs)
      Value = air.CreateFPUnOp(AIRBuilder::fabs, Value);
    if (C.neg)
      Value = ir.CreateFNeg(Value);
    break;
  case OperandDataType::Integer:
    Value = BitcastToInt32(Value);
    if (C.neg)
      Value = ir.CreateNeg(Value);
    break;
  case OperandDataType::Half16X16:
    Value = TruncAndBitcastToHalf(Value);
    if (C.neg)
      Value = ir.CreateFNeg(Value);
    break;
  }
  return Value;
}

llvm::Optional<TextureResourceHandle>
Converter::LoadTexture(const SrcOperandResource &SrcOp) {
  using namespace llvm::air;

  auto &[res, res_handle_fn, md_fn, global_coherent] = ctx.resource.srv_range_map[SrcOp.range_id];
  auto res_handle = res_handle_fn(nullptr).build(ctx);
  if (res_handle.takeError())
    return {};
  auto md = md_fn(nullptr).build(ctx);
  if (md.takeError())
    return {};

  Texture texture;
  texture.kind = res.resource_kind;
  texture.memory_access = (Texture::MemoryAccess)res.memory_access;
  texture.sample_type = std::visit(
      patterns{
          [](air::MSLInt) { return Texture::sample_int; }, [](air::MSLUint) { return Texture::sample_uint; },
          [](auto) { return Texture::sample_float; }
      },
      res.component_type
  );

  return llvm::Optional<TextureResourceHandle>(
      {texture, res.resource_kind_logical, res_handle.get(), md.get(), SrcOp.read_swizzle,
       global_coherent && SupportsMemoryCoherency()}
  );
}

llvm::Optional<TextureResourceHandle>
Converter::LoadTexture(const SrcOperandUAV &SrcOp) {
  using namespace llvm::air;

  auto &[res, res_handle_fn, md_fn, global_coherent] = ctx.resource.uav_range_map[SrcOp.range_id];
  auto res_handle = res_handle_fn(nullptr).build(ctx);
  if (res_handle.takeError())
    return {};
  auto md = md_fn(nullptr).build(ctx);
  if (md.takeError())
    return {};

  Texture texture;
  texture.kind = res.resource_kind;
  texture.memory_access = (Texture::MemoryAccess)res.memory_access;
  texture.sample_type = std::visit(
      patterns{
          [](air::MSLInt) { return Texture::sample_int; }, [](air::MSLUint) { return Texture::sample_uint; },
          [](auto) { return Texture::sample_float; }
      },
      res.component_type
  );

  return llvm::Optional<TextureResourceHandle>(
      {texture, res.resource_kind_logical, res_handle.get(), md.get(), SrcOp.read_swizzle,
       global_coherent && SupportsMemoryCoherency()}
  );
}

llvm::Optional<TextureResourceHandle>
Converter::LoadTexture(const AtomicDstOperandUAV &DstOp) {
  using namespace llvm::air;

  auto &[res, res_handle_fn, md_fn, global_coherent] = ctx.resource.uav_range_map[DstOp.range_id];
  auto res_handle = res_handle_fn(nullptr).build(ctx);
  if (res_handle.takeError())
    return {};
  auto md = md_fn(nullptr).build(ctx);
  if (md.takeError())
    return {};

  Texture texture;
  texture.kind = res.resource_kind;
  texture.memory_access = (Texture::MemoryAccess)res.memory_access;
  texture.sample_type = std::visit(
      patterns{
          [](air::MSLInt) { return Texture::sample_int; }, [](air::MSLUint) { return Texture::sample_uint; },
          [](auto) { return Texture::sample_float; }
      },
      res.component_type
  );

  return llvm::Optional<TextureResourceHandle>(
      {texture, res.resource_kind_logical, res_handle.get(), md.get(), swizzle_identity,
       global_coherent && SupportsMemoryCoherency()}
  );
}

llvm::Optional<BufferResourceHandle>
Converter::LoadBuffer(const SrcOperandResource &SrcOp) {
  using namespace llvm::air;

  if (!res.srv_buf_range_map.contains(SrcOp.range_id))
    return {};

  auto &[stride, res_handle_fn, md_fn, global_coherent] = res.srv_buf_range_map[SrcOp.range_id];
  auto res_handle = res_handle_fn(nullptr).build(ctx);
  if (res_handle.takeError())
    return {};

  auto md = md_fn(nullptr).build(ctx);
  if (md.takeError())
    return {};

  return llvm::Optional<BufferResourceHandle>(
      {res_handle.get(), md.get(), stride, SrcOp.read_swizzle, global_coherent && SupportsMemoryCoherency()}
  );
}

llvm::Optional<BufferResourceHandle>
Converter::LoadBuffer(const SrcOperandUAV &SrcOp) {
  using namespace llvm::air;

  if (!res.uav_buf_range_map.contains(SrcOp.range_id))
    return {};

  auto &[stride, res_handle_fn, md_fn, global_coherent] = res.uav_buf_range_map[SrcOp.range_id];
  auto res_handle = res_handle_fn(nullptr).build(ctx);
  if (res_handle.takeError())
    return {};

  auto md = md_fn(nullptr).build(ctx);
  if (md.takeError())
    return {};

  return llvm::Optional<BufferResourceHandle>(
      {res_handle.get(), md.get(), stride, SrcOp.read_swizzle, global_coherent && SupportsMemoryCoherency()}
  );
}

llvm::Optional<AtomicBufferResourceHandle>
Converter::LoadBuffer(const AtomicDstOperandUAV &DstOp) {
  using namespace llvm::air;

  if (!res.uav_buf_range_map.contains(DstOp.range_id))
    return {};

  auto &[stride, res_handle_fn, md_fn, global_coherent] = res.uav_buf_range_map[DstOp.range_id];
  auto res_handle = res_handle_fn(nullptr).build(ctx);
  if (res_handle.takeError())
    return {};

  auto md = md_fn(nullptr).build(ctx);
  if (md.takeError())
    return {};

  return llvm::Optional<AtomicBufferResourceHandle>(
      {res_handle.get(), md.get(), stride, DstOp.mask, global_coherent && SupportsMemoryCoherency()}
  );
}

llvm::Optional<BufferResourceHandle>
Converter::LoadBuffer(const SrcOperandTGSM &SrcOp) {
  auto [stride, tgsm_h] = res.tgsm_map[SrcOp.id];

  llvm::Value *IntPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(tgsm_h->getValueType(), tgsm_h, ir.getInt32(0));

  IntPtr = ir.CreatePointerCast(IntPtr, ir.getInt32Ty()->getPointerTo(tgsm_h->getAddressSpace()));

  return llvm::Optional<BufferResourceHandle>({IntPtr, nullptr, stride, SrcOp.read_swizzle, false});
}
llvm::Optional<AtomicBufferResourceHandle>
Converter::LoadBuffer(const AtomicOperandTGSM &DstOp) {
  auto [stride, tgsm_h] = res.tgsm_map[DstOp.id];

  llvm::Value *IntPtr = llvm::ConstantExpr::getInBoundsGetElementPtr(tgsm_h->getValueType(), tgsm_h, ir.getInt32(0));

  IntPtr = ir.CreatePointerCast(IntPtr, ir.getInt32Ty()->getPointerTo(tgsm_h->getAddressSpace()));

  return llvm::Optional<AtomicBufferResourceHandle>({IntPtr, nullptr, stride, DstOp.mask, false});
}

llvm::Optional<UAVCounterHandle>
Converter::LoadCounter(const AtomicDstOperandUAV &SrcOp) {
  using namespace llvm::air;

  if (!res.uav_counter_range_map.contains(SrcOp.range_id))
    return {};

  auto handle = res.uav_counter_range_map[SrcOp.range_id](nullptr).build(ctx);
  if (handle.takeError())
    return {};

  return llvm::Optional<UAVCounterHandle>({handle.get()});
}

llvm::Optional<SamplerHandle>
Converter::LoadSampler(const SrcOperandSampler &SrcOp) {
  using namespace llvm::air;

  auto &[sampler_handle_fn, sampler_cube_fn, sampler_metadata] = res.sampler_range_map[SrcOp.range_id];
  auto smp = sampler_handle_fn(nullptr).build(ctx);
  if (smp.takeError())
    return {};
  auto smpcube = sampler_cube_fn(nullptr).build(ctx);
  if (smpcube.takeError())
    return {};
  auto md = sampler_metadata(nullptr).build(ctx);
  if (md.takeError())
    return {};

  auto Bias = ir.CreateBitCast(ir.CreateTrunc(md.get(), ctx.types._int), ctx.types._float);

  return llvm::Optional<SamplerHandle>({smp.get(), smpcube.get(), Bias});
}

void
Converter::StoreOperand(const DstOperandOutput &DstOp, llvm::Value *Value) {
  if (ctx.shader_type == microsoft::D3D11_SB_HULL_SHADER && DstOp.phase == ~0u)
    return StoreOperandHull(DstOp, Value);

  auto ValueInt = ZExtAndBitcastToInt32(Value);

  auto Handle = res.output.ptr_int4;
  if (DstOp.phase != ~0u) {
    Handle = res.patch_constant_output.ptr_int4;
  }
  auto TyHandle = GetArrayType(Handle);

  if ((DstOp._.mask & kMaskAll) == kMaskAll) {
    auto Ptr = ir.CreateInBoundsGEP(TyHandle, Handle, {ir.getInt32(0), ir.getInt32(DstOp.regid)});
    ir.CreateStore(VectorSplat(4, ValueInt), Ptr);
    return;
  }
  for (auto [DstComp, SrcComp] : EnumerateComponents(DstOp._.mask)) {
    auto Ptr = ir.CreateInBoundsGEP(TyHandle, Handle, {ir.getInt32(0), ir.getInt32(DstOp.regid), ir.getInt32(DstComp)});
    ir.CreateStore(ExtractElement(ValueInt, SrcComp), Ptr);
  }
}

void
Converter::StoreOperandHull(const DstOperandOutput &DstOp, llvm::Value *Value) {
  auto ValueInt = ZExtAndBitcastToInt32(Value);

  auto Handle = res.output.ptr_int4;
  auto TyHandle = GetArrayType(Handle);

  auto CPId = res.thread_id_in_patch;
  auto RegId = ir.getInt32(DstOp.regid);

  if ((DstOp._.mask & kMaskAll) == kMaskAll) {
    auto Ptr = ir.CreateInBoundsGEP(TyHandle, Handle, {ir.getInt32(0), CPId, RegId});
    ir.CreateStore(VectorSplat(4, ValueInt), Ptr);
    return;
  }
  for (auto [DstComp, SrcComp] : EnumerateComponents(DstOp._.mask)) {
    auto Ptr = ir.CreateInBoundsGEP(TyHandle, Handle, {ir.getInt32(0), CPId, RegId, ir.getInt32(DstComp)});
    ir.CreateStore(ExtractElement(ValueInt, SrcComp), Ptr);
  }
}

void
Converter::StoreOperand(const DstOperandOutputCoverageMask &DstOp, llvm::Value *Value) {
  auto ValueInt = ZExtAndBitcastToInt32(Value);
  auto Ptr = ir.CreateConstGEP1_32(ir.getInt32Ty(), res.coverage_mask_reg, 0);
  ir.CreateStore(ExtractElement(ValueInt, 0), Ptr);
}

void
Converter::StoreOperand(const DstOperandOutputDepth &DstOp, llvm::Value *Value) {
  auto ValueFloat = ZExtAndBitcastToFloat(Value);
  auto Ptr = ir.CreateConstGEP1_32(ir.getFloatTy(), res.depth_output_reg, 0);
  ir.CreateStore(ExtractElement(ValueFloat, 0), Ptr);
}

void
Converter::StoreOperand(const DstOperandIndexableOutput &DstOp, llvm::Value *Value) {
  if (ctx.shader_type == microsoft::D3D11_SB_HULL_SHADER && DstOp.phase == ~0u)
    return StoreOperandHull(DstOp, Value);

  auto ValueInt = ZExtAndBitcastToInt32(Value);

  auto Handle = res.output.ptr_int4;
  if (DstOp.phase != ~0u) {
    Handle = res.patch_constant_output.ptr_int4;
  }
  auto TyHandle = GetArrayType(Handle);
  auto Index = LoadOperandIndex(DstOp.regindex);

  if ((DstOp._.mask & kMaskAll) == kMaskAll) {
    auto Ptr = ir.CreateInBoundsGEP(TyHandle, Handle, {ir.getInt32(0), Index});
    ir.CreateStore(VectorSplat(4, ValueInt), Ptr);
    return;
  }
  for (auto [DstComp, SrcComp] : EnumerateComponents(DstOp._.mask)) {
    auto Ptr = ir.CreateInBoundsGEP(TyHandle, Handle, {ir.getInt32(0), Index, ir.getInt32(DstComp)});
    ir.CreateStore(ExtractElement(ValueInt, SrcComp), Ptr);
  }
}

void
Converter::StoreOperandHull(const DstOperandIndexableOutput &DstOp, llvm::Value *Value) {
  auto ValueInt = ZExtAndBitcastToInt32(Value);

  auto Handle = res.output.ptr_int4;
  auto TyHandle = GetArrayType(Handle);
  auto Index = ir.CreateAdd(
      ir.CreateMul(res.thread_id_in_patch, ir.getInt32(res.output_element_count)), LoadOperandIndex(DstOp.regindex)
  );

  if ((DstOp._.mask & kMaskAll) == kMaskAll) {
    auto Ptr = ir.CreateInBoundsGEP(TyHandle, Handle, {ir.getInt32(0), Index});
    ir.CreateStore(VectorSplat(4, ValueInt), Ptr);
    return;
  }
  for (auto [DstComp, SrcComp] : EnumerateComponents(DstOp._.mask)) {
    auto Ptr = ir.CreateInBoundsGEP(TyHandle, Handle, {ir.getInt32(0), Index, ir.getInt32(DstComp)});
    ir.CreateStore(ExtractElement(ValueInt, SrcComp), Ptr);
  }
}

void
Converter::StoreOperand(const DstOperandTemp &DstOp, llvm::Value *Value) {
  auto ValueInt = ZExtAndBitcastToInt32(Value);

  auto Handle = res.temp.ptr_int4;

  if (DstOp.phase != ~0u) {
    Handle = res.phases[DstOp.phase].temp.ptr_int4;
  }
  auto TyHandle = GetArrayType(Handle);

  for (auto [DstComp, SrcComp] : EnumerateComponents(DstOp._.mask)) {
    auto Ptr = ir.CreateInBoundsGEP(
        TyHandle, Handle,
        {ir.getInt32(0), ir.CreateAdd(ir.CreateShl(ir.getInt32(DstOp.regid), 2), ir.getInt32(DstComp))}
    );
    ir.CreateStore(ExtractElement(ValueInt, SrcComp), Ptr);
  }
}

void
Converter::StoreOperand(const DstOperandIndexableTemp &DstOp, llvm::Value *Value) {
  auto ValueInt = ZExtAndBitcastToInt32(Value);

  indexable_register_file regfile;
  if (DstOp.phase != ~0u) {
    regfile = res.phases[DstOp.phase].indexable_temp_map[DstOp.regfile];
  } else {
    regfile = res.indexable_temp_map[DstOp.regfile];
  }

  auto Handle = regfile.ptr_int_vec;
  auto TyHandle = GetArrayType(Handle);
  auto Index = LoadOperandIndex(DstOp.regindex);

  for (auto [DstComp, SrcComp] : EnumerateComponents(DstOp._.mask)) {
    auto Ptr = ir.CreateInBoundsGEP(
        TyHandle, Handle,
        {ir.getInt32(0), ir.CreateAdd(ir.CreateMul(Index, ir.getInt32(regfile.vec_size)), ir.getInt32(DstComp))}
    );
    ir.CreateStore(ExtractElement(ValueInt, SrcComp), Ptr);
  }
}

llvm::Value *
Converter::BitcastToFloat(llvm::Value *Value) {
  auto Ty = Value->getType();
  if (Ty->getScalarType()->isFloatTy()) {
    return Value;
  }
  if (auto TyVec = llvm::dyn_cast<llvm::FixedVectorType>(Ty)) {
    return ir.CreateBitCast(Value, air.getFloatTy(TyVec->getNumElements()));
  }
  return ir.CreateBitCast(Value, ir.getFloatTy());
}

llvm::Value *
Converter::BitcastToInt32(llvm::Value *Value) {
  auto Ty = Value->getType();
  if (Ty->getScalarType()->isIntegerTy(32)) {
    return Value;
  }
  if (auto TyVec = llvm::dyn_cast<llvm::FixedVectorType>(Ty)) {
    return ir.CreateBitCast(Value, air.getIntTy(TyVec->getNumElements()));
  }
  return ir.CreateBitCast(Value, ir.getInt32Ty());
}

llvm::Value *
Converter::TruncAndBitcastToHalf(llvm::Value *Value) {
  auto Ty = Value->getType();
  if (Ty->getScalarType()->isHalfTy()) {
    return Value;
  }
  if (auto TyVec = llvm::dyn_cast<llvm::FixedVectorType>(Ty)) {
    auto ElementCount = TyVec->getNumElements();
    auto TyShortVec = llvm::FixedVectorType::get(ir.getInt16Ty(), ElementCount);
    if (!Ty->getScalarType()->isIntegerTy(32)) {
      Value = ir.CreateBitCast(Value, air.getIntTy(ElementCount));
    }
    return ir.CreateBitCast(ir.CreateTrunc(Value, TyShortVec), air.getHalfTy(ElementCount));
  }
  if (!Ty->getScalarType()->isIntegerTy(32)) {
    Value = ir.CreateBitCast(Value, ir.getInt32Ty());
  }
  return ir.CreateBitCast(ir.CreateTrunc(Value, ir.getInt16Ty()), air.getHalfTy());
}

llvm::Value *
Converter::ZExtAndBitcastToInt32(llvm::Value *Value) {
  auto Ty = Value->getType();
  if (Ty->getScalarType()->isHalfTy()) {
    if (auto TyVec = llvm::dyn_cast<llvm::FixedVectorType>(Ty)) {
      auto ElementCount = TyVec->getNumElements();
      auto TyShortVec = llvm::FixedVectorType::get(ir.getInt16Ty(), ElementCount);
      return ir.CreateZExt(ir.CreateBitCast(Value, TyShortVec), air.getIntTy(ElementCount));
    }
    return ir.CreateZExt(ir.CreateBitCast(Value, ir.getInt16Ty()), ir.getInt32Ty());
  }
  return BitcastToInt32(Value);
}

llvm::Value *
Converter::ZExtAndBitcastToFloat(llvm::Value *Value) {
  auto Ty = Value->getType();
  if (Ty->getScalarType()->isHalfTy()) {
    // doesn't make much sense but just in case
    Value = ZExtAndBitcastToInt32(Value);
  }
  return BitcastToFloat(Value);
}

llvm::Value *
Converter::MaskSwizzle(llvm::Value *Value, mask_t Mask, Swizzle Swizzle) {
  if (!isa<llvm::FixedVectorType>(Value->getType())) {
    switch (Mask) {
    case 0b1:
    case 0b10:
    case 0b100:
    case 0b1000:
      return Value;
    case 0b1100:
    case 0b0110:
    case 0b0011:
    case 0b1010:
    case 0b0101:
    case 0b1001:
      return ir.CreateVectorSplat(2, Value);
    case 0b1101:
    case 0b1011:
    case 0b1110:
    case 0b0111:
      return ir.CreateVectorSplat(3, Value);
    case 0b1111:
      return ir.CreateVectorSplat(4, Value);
    default:
      break;
    }
    return Value;
  }
  switch (Mask) {
  case 0b1:
    return ir.CreateExtractElement(Value, Swizzle[0]);
  case 0b10:
    return ir.CreateExtractElement(Value, Swizzle[1]);
  case 0b100:
    return ir.CreateExtractElement(Value, Swizzle[2]);
  case 0b1000:
    return ir.CreateExtractElement(Value, Swizzle[3]);
  case 0b1100:
    return ir.CreateShuffleVector(Value, {Swizzle[2], Swizzle[3]});
  case 0b0110:
    return ir.CreateShuffleVector(Value, {Swizzle[1], Swizzle[2]});
  case 0b0011:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[1]});
  case 0b1010:
    return ir.CreateShuffleVector(Value, {Swizzle[1], Swizzle[3]});
  case 0b0101:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[2]});
  case 0b1001:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[3]});
  case 0b1101:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[2], Swizzle[3]});
  case 0b1011:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[1], Swizzle[3]});
  case 0b1110:
    return ir.CreateShuffleVector(Value, {Swizzle[1], Swizzle[2], Swizzle[3]});
  case 0b0111:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[1], Swizzle[2]});
  case 0b1111:
    return ir.CreateShuffleVector(Value, {Swizzle[0], Swizzle[1], Swizzle[2], Swizzle[3]});
  default:
    break;
  }
  return Value;
}

void
Converter::operator()(const InstMov &mov) {
  auto FM = UseFastMath(mov._.precise_mask);
  mask_t Mask = GetMask(mov.dst);
  auto Value = LoadOperand(mov.src, Mask);
  StoreOperand(mov.dst, Value, mov._.saturate);
}

void
Converter::operator()(const InstMovConditional &movc) {
  auto FM = UseFastMath(movc._.precise_mask);
  mask_t Mask = GetMask(movc.dst);
  auto Src0 = LoadOperand(movc.src0, Mask);
  auto Src1 = LoadOperand(movc.src1, Mask);
  auto Cond = LoadOperand(movc.src_cond, Mask);
  auto CondZero = llvm::Constant::getNullValue(Cond->getType());
  auto Result = ir.CreateSelect(ir.CreateICmpNE(Cond, CondZero), Src0, Src1);
  StoreOperand(movc.dst, Result, movc._.saturate);
}
void
Converter::operator()(const InstSwapConditional &swapc) {
  mask_t Mask0 = GetMask(swapc.dst0);
  mask_t Mask1 = GetMask(swapc.dst1);
  mask_t MaskCombined = Mask0 | Mask1;

  auto Src0 = LoadOperand(swapc.src0, MaskCombined);
  auto Src1 = LoadOperand(swapc.src1, MaskCombined);
  auto Cond = LoadOperand(swapc.src_cond, MaskCombined);

  auto Result0 = ir.CreateSelect(
      ir.CreateIsNotNull(ExtractFromCombinedMask(Cond, MaskCombined, Mask0)),
      ExtractFromCombinedMask(Src1, MaskCombined, Mask0), ExtractFromCombinedMask(Src0, MaskCombined, Mask0)
  );
  auto Result1 = ir.CreateSelect(
      ir.CreateIsNotNull(ExtractFromCombinedMask(Cond, MaskCombined, Mask1)),
      ExtractFromCombinedMask(Src0, MaskCombined, Mask1), ExtractFromCombinedMask(Src1, MaskCombined, Mask1)
  );

  StoreOperand(swapc.dst0, Result0);
  StoreOperand(swapc.dst1, Result1);
}

void
Converter::operator()(const InstDotProduct &dot) {
  auto FM = UseFastMath(dot._.precise_mask);
  static mask_t DimensionMask[] = {kMaskVecXY, kMaskVecXYZ, kMaskAll};
  if (dot.dimension < 2 || dot.dimension > 4) {
    return;
  }
  mask_t Mask = DimensionMask[dot.dimension - 2];
  auto LHS = LoadOperand(dot.src0, Mask);
  auto RHS = LoadOperand(dot.src1, Mask);
  auto Result = air.CreateDotProduct(LHS, RHS);
  StoreOperand(dot.dst, Result, dot._.saturate);
}

void
Converter::operator()(const InstFloatUnaryOp &unary) {
  using namespace llvm::air;

  auto FM = UseFastMath(unary._.precise_mask);
  mask_t Mask = GetMask(unary.dst);
  auto Value = LoadOperand(unary.src, Mask);

  switch (unary.op) {
  case FloatUnaryOp::Log2:
    Value = air.CreateFPUnOp(AIRBuilder::log2, Value);
    break;
  case FloatUnaryOp::Exp2:
    Value = air.CreateFPUnOp(AIRBuilder::exp2, Value);
    break;
  case FloatUnaryOp::Rcp:
    Value = ir.CreateFDiv(llvm::ConstantFP::get(Value->getType(), 1.0), Value);
    break;
  case FloatUnaryOp::Rsq:
    Value = air.CreateFPUnOp(AIRBuilder::rsqrt, Value);
    break;
  case FloatUnaryOp::Sqrt:
    Value = air.CreateFPUnOp(AIRBuilder::sqrt, Value);
    break;
  case FloatUnaryOp::Fraction:
    Value = air.CreateFPUnOp(AIRBuilder::fract, Value);
    break;
  case FloatUnaryOp::RoundNearestEven:
    Value = air.CreateFPUnOp(AIRBuilder::rint, Value);
    break;
  case FloatUnaryOp::RoundNegativeInf:
    Value = air.CreateFPUnOp(AIRBuilder::floor, Value);
    break;
  case FloatUnaryOp::RoundPositiveInf:
    Value = air.CreateFPUnOp(AIRBuilder::ceil, Value);
    break;
  case FloatUnaryOp::RoundZero:
    Value = air.CreateFPUnOp(AIRBuilder::trunc, Value);
    break;
  }

  StoreOperand(unary.dst, Value, unary._.saturate);
}

void
Converter::operator()(const InstFloatBinaryOp &bin) {
  using namespace llvm::air;

  auto FM = UseFastMath(bin._.precise_mask);
  mask_t Mask = GetMask(bin.dst);
  auto LHS = LoadOperand(bin.src0, Mask);
  auto RHS = LoadOperand(bin.src1, Mask);

  llvm::Value *Result = llvm::PoisonValue::get(LHS->getType());

  switch (bin.op) {
  case FloatBinaryOp::Add:
    Result = ir.CreateFAdd(LHS, RHS);
    break;
  case FloatBinaryOp::Mul: {
    Result = ir.CreateFMul(LHS, RHS);
    break;
  }
  case FloatBinaryOp::Div:
    Result = ir.CreateFDiv(LHS, RHS);
    break;
  case FloatBinaryOp::Min: {
    Result = air.CreateFPBinOp(AIRBuilder::fmin, LHS, RHS);
    break;
  }
  case FloatBinaryOp::Max: {
    Result = air.CreateFPBinOp(AIRBuilder::fmax, LHS, RHS);
    break;
  }
  }

  StoreOperand(bin.dst, Result, bin._.saturate);
}

void
Converter::operator()(const InstIntegerUnaryOp &unary) {
  using namespace llvm::air;

  mask_t Mask = GetMask(unary.dst);
  auto Value = LoadOperand(unary.src, Mask);

  switch (unary.op) {
  case IntegerUnaryOp::Neg:
    Value = ir.CreateNeg(Value);
    break;
  case IntegerUnaryOp::Not:
    Value = ir.CreateNot(Value);
    break;
  case IntegerUnaryOp::ReverseBits:
    Value = air.CreateIntUnOp(AIRBuilder::reverse_bits, Value);
    break;
  case IntegerUnaryOp::CountBits:
    Value = air.CreateIntUnOp(AIRBuilder::popcount, Value);
    break;
  case IntegerUnaryOp::FirstHiBitSigned:
    Value = ir.CreateSelect(
        ir.CreateIsNotNeg(Value), air.CreateCountZero(Value, false), air.CreateCountZero(ir.CreateNot(Value), true)
    );
    Value = MaxIfInMask(~((uint32_t)0x1f), Value);
    break;
  case IntegerUnaryOp::FirstHiBit:
    Value = air.CreateCountZero(Value, false);
    Value = MaxIfInMask(~((uint32_t)0x1f), Value);
    break;
  case IntegerUnaryOp::FirstLowBit:
    Value = air.CreateCountZero(Value, true);
    Value = MaxIfInMask(~((uint32_t)0x1f), Value);
    break;
  }

  StoreOperand(unary.dst, Value);
}

void
Converter::operator()(const InstIntegerBinaryOp &bin) {
  using namespace llvm::air;

  mask_t Mask = GetMask(bin.dst);
  auto LHS = LoadOperand(bin.src0, Mask);
  auto RHS = LoadOperand(bin.src1, Mask);

  llvm::Value *Result = llvm::PoisonValue::get(LHS->getType());

  switch (bin.op) {
  case IntegerBinaryOp::UMin:
    Result = air.CreateIntBinOp(AIRBuilder::min, LHS, RHS);
    break;
  case IntegerBinaryOp::UMax:
    Result = air.CreateIntBinOp(AIRBuilder::max, LHS, RHS);
    break;
  case IntegerBinaryOp::IMin:
    Result = air.CreateIntBinOp(AIRBuilder::min, LHS, RHS, true);
    break;
  case IntegerBinaryOp::IMax:
    Result = air.CreateIntBinOp(AIRBuilder::max, LHS, RHS, true);
    break;
  case IntegerBinaryOp::IShl:
    Result = ir.CreateShl(LHS, ir.CreateAnd(RHS, 0x1f));
    break;
  case IntegerBinaryOp::IShr:
    Result = ir.CreateAShr(LHS, ir.CreateAnd(RHS, 0x1f));
    break;
  case IntegerBinaryOp::UShr:
    Result = ir.CreateLShr(LHS, ir.CreateAnd(RHS, 0x1f));
    break;
  case IntegerBinaryOp::Xor:
    Result = ir.CreateXor(LHS, RHS);
    break;
  case IntegerBinaryOp::Or:
    Result = ir.CreateOr(LHS, RHS);
    break;
  case IntegerBinaryOp::And:
    Result = ir.CreateAnd(LHS, RHS);
    break;
  case IntegerBinaryOp::Add:
    Result = ir.CreateAdd(LHS, RHS);
    break;
  }

  StoreOperand(bin.dst, Result);
}

void
Converter::operator()(const InstPartialDerivative &deriv) {
  auto FM = UseFastMath(deriv._.precise_mask);
  mask_t Mask = GetMask(deriv.dst);
  auto SrcValue = LoadOperand(deriv.src, Mask);
  auto Result = air.CreateDerivative(SrcValue, deriv.ddy);
  StoreOperand(deriv.dst, Result, deriv._.saturate);
}

void
Converter::operator()(const InstConvert &convert) {
  using namespace llvm::air;

  mask_t Mask = GetMask(convert.dst);
  auto Value = LoadOperand(convert.src, Mask);

  switch (convert.op) {
  case ConversionOp::HalfToFloat:
    Value = air.CreateConvertToFloat(Value);
    break;
  case ConversionOp::FloatToHalf:
    Value = air.CreateConvertToHalf(Value);
    break;
  case ConversionOp::FloatToSigned:
    Value = air.CreateConvertToSigned(Value);
    break;
  case ConversionOp::SignedToFloat:
    Value = air.CreateConvertToFloat(Value);
    break;
  case ConversionOp::FloatToUnsigned:
    Value = air.CreateConvertToUnsigned(Value);
    break;
  case ConversionOp::UnsignedToFloat:
    Value = air.CreateConvertToFloat(Value, Signedness::Unsigned);
    break;
  }

  StoreOperand(convert.dst, Value);
}

void
Converter::operator()(const InstFloatCompare &cmp) {
  auto FM = UseFastMath(cmp._.precise_mask);
  mask_t Mask = GetMask(cmp.dst);
  auto LHS = LoadOperand(cmp.src0, Mask);
  auto RHS = LoadOperand(cmp.src1, Mask);

  llvm::CmpInst::Predicate Pred = {};

  switch (cmp.cmp) {
  case FloatComparison::Equal:
    Pred = llvm::CmpInst::FCMP_OEQ;
    break;
  case FloatComparison::NotEqual:
    Pred = llvm::CmpInst::FCMP_UNE;
    break;
  case FloatComparison::GreaterEqual:
    Pred = llvm::CmpInst::FCMP_OGE;
    break;
  case FloatComparison::LessThan:
    Pred = llvm::CmpInst::FCMP_OLT;
    break;
  }

  llvm::Value * Result = ir.CreateFCmp(Pred, LHS, RHS);

  StoreOperand(cmp.dst, ir.CreateSExt(Result, Result->getType()->getWithNewBitWidth(32)));
}

void
Converter::operator()(const InstFloatMAD &mad) {
  auto FM = UseFastMath(mad._.precise_mask);
  mask_t Mask = GetMask(mad.dst);
  auto A = LoadOperand(mad.src0, Mask);
  auto B = LoadOperand(mad.src1, Mask);
  auto C = LoadOperand(mad.src2, Mask);

  StoreOperand(mad.dst, air.CreateFMA(A, B, C), mad._.saturate);
}

void
Converter::operator()(const InstSinCos &sincos) {
  using namespace llvm::air;

  auto FM = UseFastMath(sincos._.precise_mask);
  mask_t MaskCos = GetMask(sincos.dst_cos);
  mask_t MaskSin = GetMask(sincos.dst_sin);
  mask_t MaskCombined = MaskCos | MaskSin;

  auto Src = LoadOperand(sincos.src, MaskCombined);

  auto SrcCos = ExtractFromCombinedMask(Src, MaskCombined, MaskCos);
  auto SrcSin = ExtractFromCombinedMask(Src, MaskCombined, MaskSin);

  if (!IsNull(sincos.dst_cos))
    StoreOperand(sincos.dst_cos, air.CreateFPUnOp(AIRBuilder::cos, SrcCos), sincos._.saturate);
  if (!IsNull(sincos.dst_sin))
    StoreOperand(sincos.dst_sin, air.CreateFPUnOp(AIRBuilder::sin, SrcSin), sincos._.saturate);
}

void
Converter::operator()(const InstIntegerCompare &cmp) {
  mask_t Mask = GetMask(cmp.dst);
  auto LHS = LoadOperand(cmp.src0, Mask);
  auto RHS = LoadOperand(cmp.src1, Mask);

  llvm::CmpInst::Predicate Pred = llvm::CmpInst::ICMP_EQ;

  switch (cmp.cmp) {
  case IntegerComparison::Equal:
    Pred = llvm::CmpInst::ICMP_EQ;
    break;
  case IntegerComparison::NotEqual:
    Pred = llvm::CmpInst::ICMP_NE;
    break;
  case IntegerComparison::SignedLessThan:
    Pred = llvm::CmpInst::ICMP_SLT;
    break;
  case IntegerComparison::SignedGreaterEqual:
    Pred = llvm::CmpInst::ICMP_SGE;
    break;
  case IntegerComparison::UnsignedLessThan:
    Pred = llvm::CmpInst::ICMP_ULT;
    break;
  case IntegerComparison::UnsignedGreaterEqual:
    Pred = llvm::CmpInst::ICMP_UGE;
    break;
  }

  auto Result = ir.CreateICmp(Pred, LHS, RHS);

  StoreOperand(cmp.dst, ir.CreateSExt(Result, Result->getType()->getWithNewBitWidth(32)));
}

void
Converter::operator()(const InstIntegerMAD &mad) {
  mask_t Mask = GetMask(mad.dst);
  auto A = LoadOperand(mad.src0, Mask);
  auto B = LoadOperand(mad.src1, Mask);
  auto C = LoadOperand(mad.src2, Mask);

  auto Result = ir.CreateAdd(ir.CreateMul(A, B), C);

  StoreOperand(mad.dst, Result);
}

void
Converter::operator()(const InstIntegerBinaryOpWithTwoDst &bin) {
  using namespace llvm;
  using namespace llvm::air;

  mask_t MaskHi = GetMask(bin.dst_hi);
  mask_t MaskLo = GetMask(bin.dst_low);
  mask_t MaskCombined = MaskHi | MaskLo;

  auto Src0 = LoadOperand(bin.src0, MaskCombined);
  auto Src1 = LoadOperand(bin.src1, MaskCombined);

  auto Src0Hi = ExtractFromCombinedMask(Src0, MaskCombined, MaskHi);
  auto Src0Lo = ExtractFromCombinedMask(Src0, MaskCombined, MaskLo);
  auto Src1Hi = ExtractFromCombinedMask(Src1, MaskCombined, MaskHi);
  auto Src1Lo = ExtractFromCombinedMask(Src1, MaskCombined, MaskLo);

  switch (bin.op) {
  case IntegerBinaryOpWithTwoDst::IMul:
  case IntegerBinaryOpWithTwoDst::UMul: {
    bool Signed = bin.op == IntegerBinaryOpWithTwoDst::IMul;
    if (!IsNull(bin.dst_hi))
      StoreOperand(bin.dst_hi, air.CreateIntBinOp(AIRBuilder::mul_hi, Src0Hi, Src1Hi, Signed));
    if (!IsNull(bin.dst_low))
      StoreOperand(bin.dst_low, ir.CreateMul(Src0Lo, Src1Lo));
    break;
  }
  case IntegerBinaryOpWithTwoDst::UDiv:
    if (!IsNull(bin.dst_hi))
      StoreOperand(bin.dst_hi, ir.CreateUDiv(Src0Hi, Src1Hi));
    if (!IsNull(bin.dst_low))
      StoreOperand(bin.dst_low, ir.CreateURem(Src0Lo, Src1Lo));
    break;
  case IntegerBinaryOpWithTwoDst::UAddCarry: {
    if (!IsNull(bin.dst_hi))
      StoreOperand(bin.dst_hi, ir.CreateAdd(Src0Hi, Src1Hi));
    if (!IsNull(bin.dst_low)) {
      auto ResultTuple = ir.CreateBinaryIntrinsic(Intrinsic::uadd_with_overflow, Src0Lo, Src1Lo);
      StoreOperand(
          bin.dst_low, ir.CreateZExt(
                           ir.CreateExtractValue(ResultTuple, 1),
                           ResultTuple->getType()->getStructElementType(1)->getWithNewBitWidth(32)
                       )
      );
    }
    break;
  }
  case IntegerBinaryOpWithTwoDst::USubBorrow:
    if (!IsNull(bin.dst_hi))
      StoreOperand(bin.dst_hi, ir.CreateSub(Src0Hi, Src1Hi));
    if (!IsNull(bin.dst_low)) {
      auto ResultTuple = ir.CreateBinaryIntrinsic(Intrinsic::usub_with_overflow, Src0Lo, Src1Lo);
      StoreOperand(
          bin.dst_low, ir.CreateZExt(
                           ir.CreateExtractValue(ResultTuple, 1),
                           ResultTuple->getType()->getStructElementType(1)->getWithNewBitWidth(32)
                       )
      );
    }
    break;
  }
}

void
Converter::operator()(const InstExtractBits &extract) {
  mask_t Mask = GetMask(extract.dst);
  auto Src0 = LoadOperand(extract.src0, Mask);
  auto Src1 = LoadOperand(extract.src1, Mask);
  auto Src2 = LoadOperand(extract.src2, Mask);

  auto Width = ir.CreateAnd(Src0, 0x1F);
  auto Offset = ir.CreateAnd(Src1, 0X1F);
  auto WidthAddOffset = ir.CreateAdd(Width, Offset);
  auto Constant_32 = llvm::ConstantInt::get(Width->getType(), 32);
  auto ClampedSrc2 = ir.CreateShl(Src2, ir.CreateSub(Constant_32, WidthAddOffset));

  llvm::Value *NeedClamp, *NoClamp, *Clamped;

  if (extract.is_signed) {
    NeedClamp = ir.CreateICmpSLT(WidthAddOffset, Constant_32);
    NoClamp = ir.CreateAShr(Src2, Offset);
    Clamped = ir.CreateAShr(ClampedSrc2, ir.CreateSub(Constant_32, Width));
  } else {
    NeedClamp = ir.CreateICmpULT(WidthAddOffset, Constant_32);
    NoClamp = ir.CreateLShr(Src2, Offset);
    Clamped = ir.CreateLShr(ClampedSrc2, ir.CreateSub(Constant_32, Width));
  }
  auto Result = ir.CreateSelect(
      ir.CreateIsNull(Width), llvm::ConstantInt::get(Width->getType(), 0), ir.CreateSelect(NeedClamp, Clamped, NoClamp)
  );
  StoreOperand(extract.dst, Result);
}
void
Converter::operator()(const InstBitFiledInsert &bfi) {
  mask_t Mask = GetMask(bfi.dst);
  auto Src0 = LoadOperand(bfi.src0, Mask);
  auto Src1 = LoadOperand(bfi.src1, Mask);
  auto Src2 = LoadOperand(bfi.src2, Mask);
  auto Src3 = LoadOperand(bfi.src3, Mask);

  auto Width = ir.CreateAnd(Src0, 0x1F);
  auto Offset = ir.CreateAnd(Src1, 0X1F);
  auto Constant_1 = llvm::ConstantInt::get(Width->getType(), 1);
  auto Bitmask = ir.CreateShl(ir.CreateSub(ir.CreateShl(Constant_1, Width), Constant_1), Offset);

  auto Result =
      ir.CreateOr(ir.CreateAnd(ir.CreateShl(Src2, Offset), Bitmask), ir.CreateAnd(Src3, ir.CreateNot(Bitmask)));
  StoreOperand(bfi.dst, Result);
}

void
Converter::operator()(const InstLoad &load) {
  using namespace llvm::air;

  auto Tex = LoadTexture(load.src_resource);
  if (!Tex)
    return;
  llvm::Value *Address = nullptr;
  llvm::Value *ArrayIndex = nullptr;
  llvm::Value *SampleIndex = nullptr;
  llvm::Constant *Offset = nullptr;

  switch (Tex->Logical) {
  case Texture::texture_buffer:
    Address = LoadOperand(load.src_address, kMaskComponentX);
    Address = ir.CreateAdd(Address, DecodeTextureBufferOffset(Tex->Metadata));
    Offset = air.getInt(load.offsets[0]);
    break;
  case Texture::texture1d:
    Address = LoadOperand(load.src_address, kMaskComponentX);
    Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
    Offset = air.getInt2(load.offsets[0], 0);
    break;
  case Texture::texture1d_array:
    Address = LoadOperand(load.src_address, kMaskComponentX);
    Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
    ArrayIndex = LoadOperand(load.src_address, kMaskComponentY);
    Offset = air.getInt2(load.offsets[0], 0);
    break;
  case Texture::texture2d:
  case Texture::depth2d:
    Address = LoadOperand(load.src_address, kMaskVecXY);
    Offset = air.getInt2(load.offsets[0], load.offsets[1]);
    break;
  case Texture::texture2d_array:
  case Texture::depth2d_array:
    Address = LoadOperand(load.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(load.src_address, kMaskComponentZ);
    Offset = air.getInt2(load.offsets[0], load.offsets[1]);
    break;
  case Texture::texture3d:
    Address = LoadOperand(load.src_address, kMaskVecXYZ);
    Offset = air.getInt3(load.offsets[0], load.offsets[1], load.offsets[2]);
    break;
  case Texture::texture2d_ms:
  case Texture::depth_2d_ms:
    Address = LoadOperand(load.src_address, kMaskVecXY);
    SampleIndex = LoadOperand(load.src_sample_index.value(), kMaskComponentX);
    Offset = air.getInt2(load.offsets[0], load.offsets[1]);
    break;
  case Texture::texture2d_ms_array:
  case Texture::depth_2d_ms_array:
    Address = LoadOperand(load.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(load.src_address, kMaskComponentZ);
    SampleIndex = LoadOperand(load.src_sample_index.value(), kMaskComponentX);
    Offset = air.getInt2(load.offsets[0], load.offsets[1]);
    break;
  default: // invalid type
    return;
  }

  if (!Offset->isNullValue())
    Address = ir.CreateAdd(Address, Offset);

  llvm::Value *LOD = LoadOperand(load.src_address, kMaskComponentW);

  auto [Value, Residency] =
      air.CreateRead(Tex->Texture, Tex->Handle, Address, ArrayIndex, SampleIndex, LOD, Tex->GlobalCoherent);

  StoreOperand(load.dst, MaskSwizzle(Value, GetMask(load.dst), Tex->Swizzle));
}
void
Converter::operator()(const InstLoadUAVTyped &load) {
  using namespace llvm::air;

  auto Tex = LoadTexture(load.src_uav);
  if (!Tex)
    return;
  llvm::Value *Address = nullptr;
  llvm::Value *ArrayIndex = nullptr;
  llvm::Value *SampleIndex = nullptr;

  switch (Tex->Logical) {
  case Texture::texture_buffer:
    Address = LoadOperand(load.src_address, kMaskComponentX);
    Address = ir.CreateAdd(Address, DecodeTextureBufferOffset(Tex->Metadata));
    break;
  case Texture::texture1d:
    Address = LoadOperand(load.src_address, kMaskComponentX);
    Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
    break;
  case Texture::texture1d_array:
    Address = LoadOperand(load.src_address, kMaskComponentX);
    Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
    ArrayIndex = LoadOperand(load.src_address, kMaskComponentY);
    break;
  case Texture::texture2d:
  case Texture::depth2d:
    Address = LoadOperand(load.src_address, kMaskVecXY);
    break;
  case Texture::texture2d_array:
  case Texture::depth2d_array:
    Address = LoadOperand(load.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(load.src_address, kMaskComponentZ);
    break;
  case Texture::texture3d:
    Address = LoadOperand(load.src_address, kMaskVecXYZ);
    break;
  default: // invalid type
    return;
  }

  if (Tex->Texture.memory_access == Texture::acesss_readwrite)
    air.CreateTextureFence(Tex->Texture, Tex->Handle);

  auto [Value, Residency] =
      air.CreateRead(Tex->Texture, Tex->Handle, Address, ArrayIndex, SampleIndex, air.getInt(0), Tex->GlobalCoherent);

  StoreOperand(load.dst, MaskSwizzle(Value, GetMask(load.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstStoreUAVTyped &store) {
  using namespace llvm::air;

  auto Tex = LoadTexture(store.dst);
  if (!Tex.hasValue())
    return;
  llvm::Value *Address = nullptr;
  llvm::Value *ArrayIndex = nullptr;

  switch (Tex->Logical) {
  case Texture::texture_buffer:
    Address = LoadOperand(store.src_address, kMaskComponentX);
    Address = ir.CreateAdd(Address, DecodeTextureBufferOffset(Tex->Metadata));
    break;
  case Texture::texture1d:
    Address = LoadOperand(store.src_address, kMaskComponentX);
    Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
    break;
  case Texture::texture1d_array:
    Address = LoadOperand(store.src_address, kMaskComponentX);
    Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
    ArrayIndex = LoadOperand(store.src_address, kMaskComponentY);
    break;
  case Texture::texture2d:
    Address = LoadOperand(store.src_address, kMaskVecXY);
    break;
  case Texture::texture2d_array:
    Address = LoadOperand(store.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(store.src_address, kMaskComponentZ);
    break;
  case Texture::texture3d:
    Address = LoadOperand(store.src_address, kMaskVecXYZ);
    break;
  default: // invalid type
    return;
  }

  auto Value = LoadOperand(store.src, kMaskAll);

  air.CreateWrite(Tex->Texture, Tex->Handle, Address, ArrayIndex, nullptr, ir.getInt32(0), Value, Tex->GlobalCoherent);
}

llvm::Value *
Converter::ClampArrayIndex(llvm::Value *ShaderValue, llvm::Value *Metadata) {
  using namespace llvm::air;
  auto rounded = air.CreateFPUnOp(AIRBuilder::rint, ShaderValue);
  auto integer = air.CreateConvertToSigned(rounded);
  auto positive_integer = air.CreateIntBinOp(AIRBuilder::max, integer, ir.getInt32(0), true);
  auto max_index = ir.CreateSub(DecodeTextureArrayLength(Metadata), ir.getInt32(1));
  return air.CreateIntBinOp(AIRBuilder::min, positive_integer, max_index, true);
}

void
Converter::operator()(const InstSample &sample) {
  using namespace llvm::air;

  auto Tex = LoadTexture(sample.src_resource);
  if (!Tex)
    return;

  auto Sampler = LoadSampler(sample.src_sampler);
  if (!Sampler)
    return;

  auto SamplerHandle = Sampler->Handle;

  auto MinLODClamp = DecodeTextureMinLODClamp(Tex->Metadata);
  if (sample.min_lod_clamp) {
    auto ShaderClamp = LoadOperand(sample.min_lod_clamp.value(), kMaskComponentX);
    MinLODClamp = air.CreateFPBinOp(AIRBuilder::fmax, MinLODClamp, ShaderClamp);
  }

  llvm::Value *Coord = nullptr;
  llvm::Value *ArrayIndex = nullptr;

  switch (Tex->Logical) {
  case Texture::texture1d:
    Coord = LoadOperand(sample.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    break;
  case Texture::texture1d_array:
    Coord = LoadOperand(sample.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentY);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::depth2d:
  case Texture::texture2d:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    break;
  case Texture::depth2d_array:
  case Texture::texture2d_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentZ);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::texture3d:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    break;
  case Texture::texturecube:
  case Texture::depthcube:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    SamplerHandle = Sampler->HandleCube;
    break;
  case Texture::texturecube_array:
  case Texture::depthcube_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentW);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    SamplerHandle = Sampler->HandleCube;
    break;
  default:
    return;
  }

  auto [Value, Residency] = air.CreateSample(
      Tex->Texture, Tex->Handle, SamplerHandle, Coord, ArrayIndex, sample.offsets, sample_bias{Sampler->Bias},
      sample_min_lod_clamp{MinLODClamp}
  );

  StoreOperand(sample.dst, MaskSwizzle(Value, GetMask(sample.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstSampleLOD &sample) {
  using namespace llvm::air;

  auto Tex = LoadTexture(sample.src_resource);
  if (!Tex)
    return;

  auto Sampler = LoadSampler(sample.src_sampler);
  if (!Sampler)
    return;

  auto SamplerHandle = Sampler->Handle;

  llvm::Value *Coord = nullptr;
  llvm::Value *ArrayIndex = nullptr;

  switch (Tex->Logical) {
  case Texture::texture1d:
    Coord = LoadOperand(sample.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    break;
  case Texture::texture1d_array:
    Coord = LoadOperand(sample.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentY);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::depth2d:
  case Texture::texture2d:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    break;
  case Texture::depth2d_array:
  case Texture::texture2d_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentZ);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::texture3d:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    break;
  case Texture::texturecube:
  case Texture::depthcube:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    SamplerHandle = Sampler->HandleCube;
    break;
  case Texture::texturecube_array:
  case Texture::depthcube_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentW);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    SamplerHandle = Sampler->HandleCube;
    break;
  default:
    return;
  }

  llvm::Value *LOD = LoadOperand(sample.src_lod, kMaskComponentX);

  auto [Value, Residency] =
      air.CreateSample(Tex->Texture, Tex->Handle, SamplerHandle, Coord, ArrayIndex, sample.offsets, sample_level{LOD});

  StoreOperand(sample.dst, MaskSwizzle(Value, GetMask(sample.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstSampleBias &sample) {
  using namespace llvm::air;

  auto Tex = LoadTexture(sample.src_resource);
  if (!Tex)
    return;

  auto Sampler = LoadSampler(sample.src_sampler);
  if (!Sampler)
    return;

  auto SamplerHandle = Sampler->Handle;

  auto MinLODClamp = DecodeTextureMinLODClamp(Tex->Metadata);
  if (sample.min_lod_clamp) {
    auto ShaderClamp = LoadOperand(sample.min_lod_clamp.value(), kMaskComponentX);
    MinLODClamp = air.CreateFPBinOp(AIRBuilder::fmax, MinLODClamp, ShaderClamp);
  }

  llvm::Value *Coord = nullptr;
  llvm::Value *ArrayIndex = nullptr;

  switch (Tex->Logical) {
  case Texture::texture1d:
    Coord = LoadOperand(sample.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    break;
  case Texture::texture1d_array:
    Coord = LoadOperand(sample.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentY);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::depth2d:
  case Texture::texture2d:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    break;
  case Texture::depth2d_array:
  case Texture::texture2d_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentZ);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::texture3d:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    break;
  case Texture::texturecube:
  case Texture::depthcube:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    SamplerHandle = Sampler->HandleCube;
    break;
  case Texture::texturecube_array:
  case Texture::depthcube_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentW);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    SamplerHandle = Sampler->HandleCube;
    break;
  default:
    return;
  }

  auto Bias = ir.CreateFAdd(Sampler->Bias, LoadOperand(sample.src_bias, kMaskComponentX));

  auto [Value, Residency] = air.CreateSample(
      Tex->Texture, Tex->Handle, SamplerHandle, Coord, ArrayIndex, sample.offsets, sample_bias{Bias},
      sample_min_lod_clamp{MinLODClamp}
  );

  StoreOperand(sample.dst, MaskSwizzle(Value, GetMask(sample.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstSampleDerivative &sample) {
  using namespace llvm::air;

  auto Tex = LoadTexture(sample.src_resource);
  if (!Tex)
    return;

  auto Sampler = LoadSampler(sample.src_sampler);
  if (!Sampler)
    return;

  auto SamplerHandle = Sampler->Handle;

  auto MinLODClamp = DecodeTextureMinLODClamp(Tex->Metadata);
  if (sample.min_lod_clamp) {
    auto ShaderClamp = LoadOperand(sample.min_lod_clamp.value(), kMaskComponentX);
    MinLODClamp = air.CreateFPBinOp(AIRBuilder::fmax, MinLODClamp, ShaderClamp);
  }

  llvm::Value *Coord = nullptr;
  llvm::Value *ArrayIndex = nullptr;
  llvm::Value *DDX = nullptr;
  llvm::Value *DDY = nullptr;

  switch (Tex->Logical) {
  case Texture::texture1d:
    Coord = LoadOperand(sample.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    DDX = LoadOperand(sample.src_x_derivative, kMaskComponentX);
    DDX = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), DDX, 0ull);
    DDY = LoadOperand(sample.src_y_derivative, kMaskComponentX);
    DDY = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), DDY, 0ull);
    break;
  case Texture::texture1d_array:
    Coord = LoadOperand(sample.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    DDX = LoadOperand(sample.src_x_derivative, kMaskComponentX);
    DDX = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), DDX, 0ull);
    DDY = LoadOperand(sample.src_y_derivative, kMaskComponentX);
    DDY = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), DDY, 0ull);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentY);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::depth2d:
  case Texture::texture2d:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    DDX = LoadOperand(sample.src_x_derivative, kMaskVecXY);
    DDY = LoadOperand(sample.src_y_derivative, kMaskVecXY);
    break;
  case Texture::depth2d_array:
  case Texture::texture2d_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    DDX = LoadOperand(sample.src_x_derivative, kMaskVecXY);
    DDY = LoadOperand(sample.src_y_derivative, kMaskVecXY);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentZ);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::texture3d:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    DDX = LoadOperand(sample.src_x_derivative, kMaskVecXYZ);
    DDY = LoadOperand(sample.src_y_derivative, kMaskVecXYZ);
    break;
  case Texture::texturecube:
  case Texture::depthcube:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    DDX = LoadOperand(sample.src_x_derivative, kMaskVecXYZ);
    DDY = LoadOperand(sample.src_y_derivative, kMaskVecXYZ);
    SamplerHandle = Sampler->HandleCube;
    break;
  case Texture::texturecube_array:
  case Texture::depthcube_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    DDX = LoadOperand(sample.src_x_derivative, kMaskVecXYZ);
    DDY = LoadOperand(sample.src_y_derivative, kMaskVecXYZ);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentW);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    SamplerHandle = Sampler->HandleCube;
    break;
  default:
    return;
  }

  auto [Value, Residency] = air.CreateSampleGrad(
      Tex->Texture, Tex->Handle, SamplerHandle, Coord, ArrayIndex, DDX, DDY, MinLODClamp, sample.offsets
  );

  StoreOperand(sample.dst, MaskSwizzle(Value, GetMask(sample.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstSampleCompare &sample) {
  using namespace llvm::air;

  auto Tex = LoadTexture(sample.src_resource);
  if (!Tex)
    return;

  auto Sampler = LoadSampler(sample.src_sampler);
  if (!Sampler)
    return;

  auto SamplerHandle = Sampler->Handle;

  auto MinLODClamp = DecodeTextureMinLODClamp(Tex->Metadata);
  if (sample.min_lod_clamp) {
    auto ShaderClamp = LoadOperand(sample.min_lod_clamp.value(), kMaskComponentX);
    MinLODClamp = air.CreateFPBinOp(AIRBuilder::fmax, MinLODClamp, ShaderClamp);
  }

  auto Reference = LoadOperand(sample.src_reference, kMaskComponentX);

  llvm::Value *Coord = nullptr;
  llvm::Value *ArrayIndex = nullptr;

  switch (Tex->Logical) {
  case Texture::depth2d:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    break;
  case Texture::depth2d_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentZ);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    break;
  case Texture::texture3d:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    break;
  case Texture::depthcube:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    SamplerHandle = Sampler->HandleCube;
    break;
  case Texture::depthcube_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentW);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    SamplerHandle = Sampler->HandleCube;
    break;
  default:
    return;
  }

  auto [Value, Residency] = //
      sample.level_zero     //
          ? air.CreateSampleCmp(
                Tex->Texture, Tex->Handle, SamplerHandle, Coord, ArrayIndex, Reference, sample.offsets,
                sample_level{air.getFloat(0)}
            )
          : air.CreateSampleCmp(
                Tex->Texture, Tex->Handle, SamplerHandle, Coord, ArrayIndex, Reference, sample.offsets,
                sample_bias{Sampler->Bias}, sample_min_lod_clamp{MinLODClamp}
            );

  StoreOperand(sample.dst, MaskSwizzle(Value, GetMask(sample.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstGather &sample) {
  using namespace llvm::air;

  auto Tex = LoadTexture(sample.src_resource);
  if (!Tex)
    return;

  auto Sampler = LoadSampler(sample.src_sampler);
  if (!Sampler)
    return;

  auto SamplerHandle = Sampler->Handle;

  auto Component = ir.getInt32(sample.src_sampler.gather_channel);

  llvm::Value *Coord = nullptr;
  llvm::Value *ArrayIndex = nullptr;
  llvm::Value *Offset = nullptr;

  switch (Tex->Logical) {
  case Texture::depth2d:
  case Texture::texture2d:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    Offset = LoadOperand(sample.offset, kMaskVecXY);
    break;
  case Texture::depth2d_array:
  case Texture::texture2d_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentZ);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    Offset = LoadOperand(sample.offset, kMaskVecXY);
    break;
  case Texture::texturecube:
  case Texture::depthcube:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    SamplerHandle = Sampler->HandleCube;
    Offset = LoadOperand(sample.offset, kMaskVecXYZ);
    break;
  case Texture::texturecube_array:
  case Texture::depthcube_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentW);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    SamplerHandle = Sampler->HandleCube;
    Offset = LoadOperand(sample.offset, kMaskVecXYZ);
    break;
  default:
    return;
  }

  auto [Value, Residency] =
      air.CreateGather(Tex->Texture, Tex->Handle, SamplerHandle, Coord, ArrayIndex, Offset, Component);

  StoreOperand(sample.dst, MaskSwizzle(Value, GetMask(sample.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstGatherCompare &sample) {
  using namespace llvm::air;

  auto Tex = LoadTexture(sample.src_resource);
  if (!Tex)
    return;

  auto Sampler = LoadSampler(sample.src_sampler);
  if (!Sampler)
    return;

  auto SamplerHandle = Sampler->Handle;

  auto Reference = LoadOperand(sample.src_reference, kMaskComponentX);

  llvm::Value *Coord = nullptr;
  llvm::Value *ArrayIndex = nullptr;
  llvm::Value *Offset = nullptr;

  switch (Tex->Logical) {
  case Texture::depth2d:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    Offset = LoadOperand(sample.offset, kMaskVecXY);
    break;
  case Texture::depth2d_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXY);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentZ);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    Offset = LoadOperand(sample.offset, kMaskVecXY);
    break;
  case Texture::depthcube:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    SamplerHandle = Sampler->HandleCube;
    Offset = LoadOperand(sample.offset, kMaskVecXYZ);
    break;
  case Texture::depthcube_array:
    Coord = LoadOperand(sample.src_address, kMaskVecXYZ);
    ArrayIndex = LoadOperand(sample.src_address, kMaskComponentW);
    ArrayIndex = ClampArrayIndex(ArrayIndex, Tex->Metadata);
    SamplerHandle = Sampler->HandleCube;
    Offset = LoadOperand(sample.offset, kMaskVecXYZ);
    break;
  default:
    return;
  }

  auto [Value, Residency] =
      air.CreateGatherCompare(Tex->Texture, Tex->Handle, SamplerHandle, Coord, ArrayIndex, Reference, Offset);

  StoreOperand(sample.dst, MaskSwizzle(Value, GetMask(sample.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstCalcLOD &lod) {
  using namespace llvm::air;

  auto Tex = LoadTexture(lod.src_resource);
  if (!Tex)
    return;

  auto Sampler = LoadSampler(lod.src_sampler);
  if (!Sampler)
    return;

  auto SamplerHandle = Sampler->Handle;

  auto MinLODClamp = DecodeTextureMinLODClamp(Tex->Metadata);

  llvm::Value *Coord = nullptr;

  switch (Tex->Logical) {
  case Texture::texture1d:
  case Texture::texture1d_array:
    Coord = LoadOperand(lod.src_address, kMaskComponentX);
    Coord = ir.CreateInsertElement(llvm::ConstantFP::getNullValue(air.getFloatTy(2)), Coord, 0ull);
    break;
  case Texture::depth2d:
  case Texture::texture2d:
  case Texture::depth2d_array:
  case Texture::texture2d_array:
    Coord = LoadOperand(lod.src_address, kMaskVecXY);
    break;
  case Texture::texture3d:
    Coord = LoadOperand(lod.src_address, kMaskVecXYZ);
    break;
  case Texture::texturecube:
  case Texture::depthcube:
  case Texture::texturecube_array:
  case Texture::depthcube_array:
    Coord = LoadOperand(lod.src_address, kMaskVecXYZ);
    SamplerHandle = Sampler->HandleCube;
    break;
  default:
    return;
  }

  auto [Clamped, Unclamped] = air.CreateCalculateLOD(Tex->Texture, Tex->Handle, SamplerHandle, Coord);

  Clamped = air.CreateFPBinOp(AIRBuilder::fmax, MinLODClamp, Clamped);

  StoreOperand(
      lod.dst,
      MaskSwizzle(
          ir.CreateInsertElement(
              ir.CreateInsertElement(llvm::ConstantAggregateZero::get(air.getFloatTy(4)), Clamped, (uint64_t)0),
              Unclamped, 1
          ),
          GetMask(lod.dst), Tex->Swizzle
      )
  );
}

void
Converter::operator()(const InstResourceInfo &resinfo) {
  using namespace llvm::air;

  auto Tex = LoadTexture(resinfo.src_resource);
  if (!Tex)
    return;

  llvm::Value *Level = LoadOperand(resinfo.src_mip_level, kMaskComponentX);

  llvm::Value *X = ir.getInt32(0);
  llvm::Value *Y = ir.getInt32(0);
  llvm::Value *Z = ir.getInt32(0);

  switch (Tex->Logical) {
  case Texture::texture1d:
    X = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::width, Level);
    break;
  case Texture::texture1d_array:
    X = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::width, Level);
    Y = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::array_length, ir.getInt32(0));
    break;
  case Texture::depth2d:
  case Texture::texture2d:
  case Texture::texturecube:
  case Texture::depthcube:
  case Texture::depth_2d_ms:
  case Texture::texture2d_ms:
    X = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::width, Level);
    Y = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::height, Level);
    break;
  case Texture::depth2d_array:
  case Texture::texture2d_array:
  case Texture::texturecube_array:
  case Texture::depthcube_array:
  case Texture::depth_2d_ms_array:
  case Texture::texture2d_ms_array:
    X = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::width, Level);
    Y = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::height, Level);
    Z = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::array_length, ir.getInt32(0));
    break;
  case Texture::texture3d:
    X = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::width, Level);
    Y = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::height, Level);
    Z = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::depth, Level);
    break;
  default:
    return;
  }

  llvm::Value *MipCount = ir.getInt32(1);

  switch (Tex->Logical) {
  case llvm::air::Texture::texture_buffer:
  case llvm::air::Texture::texture2d_ms:
  case llvm::air::Texture::texture2d_ms_array:
  case llvm::air::Texture::depth_2d_ms:
  case llvm::air::Texture::depth_2d_ms_array:
    break;
  default:
    MipCount = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::num_mip_levels, ir.getInt32(0));
    break;
  }

  switch (resinfo.modifier) {
  case InstResourceInfo::M::none: {
    X = air.CreateConvertToFloat(X);
    Y = air.CreateConvertToFloat(Y);
    Z = air.CreateConvertToFloat(Z);
    MipCount = air.CreateConvertToFloat(MipCount);
    break;
  }
  case InstResourceInfo::M::rcp: {
    X = air.CreateConvertToFloat(X);
    Y = air.CreateConvertToFloat(Y);
    Z = air.CreateConvertToFloat(Z);
    X = ir.CreateFDiv(air.getFloat(1.0f), X);
    Y = ir.CreateFDiv(air.getFloat(1.0f), Y);
    Z = ir.CreateFDiv(air.getFloat(1.0f), Z);
    MipCount = air.CreateConvertToFloat(MipCount);
    break;
  }
  case InstResourceInfo::M::uint:
    break;
  }

  llvm::Value *Value = llvm::ConstantAggregateZero::get(
      resinfo.modifier == InstResourceInfo::M::uint ? air.getIntTy(4) : air.getFloatTy(4)
  );
  Value = ir.CreateInsertElement(Value, X, (uint64_t)0);
  Value = ir.CreateInsertElement(Value, Y, 1);
  Value = ir.CreateInsertElement(Value, Z, 2);
  Value = ir.CreateInsertElement(Value, MipCount, 3);

  StoreOperand(resinfo.dst, MaskSwizzle(Value, GetMask(resinfo.dst), Tex->Swizzle));
}

void
Converter::operator()(const InstSampleInfo &sample) {
  using namespace llvm::air;

  llvm::Optional<TextureResourceHandle> Tex =
      sample.src ? LoadTexture(sample.src.value()) : llvm::Optional<TextureResourceHandle>{};

  llvm::Value *SampleCount = nullptr;

  if (!Tex) {
    if (sample.src.has_value()) {
      SampleCount = ir.getInt32(0);
    } else {
      SampleCount = air.CreateGetNumSamples();
    }
  } else {
    SampleCount = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::num_samples, ir.getInt32(0));
  }

  if (!sample.uint_result) {
    SampleCount = air.CreateConvertToFloat(SampleCount);
  }

  llvm::Value *ValueVec4 = llvm::ConstantAggregateZero::get(sample.uint_result ? air.getIntTy(4) : air.getFloatTy(4));
  ValueVec4 = ir.CreateInsertElement(ValueVec4, SampleCount, (uint64_t)0);

  StoreOperand(sample.dst, MaskSwizzle(ValueVec4, GetMask(sample.dst), sample.read_swizzle));
}

void
Converter::operator()(const InstSamplePos &sample) {
  using namespace llvm::air;

  llvm::Optional<TextureResourceHandle> Tex =
      sample.src ? LoadTexture(sample.src.value()) : llvm::Optional<TextureResourceHandle>{};

  llvm::Value *SampleCount = nullptr;

  if (!Tex) {
    if (sample.src.has_value()) {
      SampleCount = ir.getInt32(0);
    } else {
      SampleCount = air.CreateGetNumSamples();
    }
  } else {
    SampleCount = air.CreateTextureQuery(Tex->Texture, Tex->Handle, Texture::num_samples, ir.getInt32(0));
  }

  llvm::Value *Index = LoadOperand(sample.src_sample_index, kMaskComponentX);

  llvm::Value *ValueVec4 = llvm::ConstantAggregateZero::get(air.getFloatTy(2));
  llvm::Value *Pos = GetSamplePos(SampleCount, Index);
  ValueVec4 = ir.CreateShuffleVector(Pos, ValueVec4, {0, 1, 2, 3});

  StoreOperand(sample.dst, MaskSwizzle(ValueVec4, GetMask(sample.dst), sample.read_swizzle));
}

void
Converter::operator()(const InstBufferInfo &bufinfo) {
  auto Buf = LoadBuffer(bufinfo.src);

  llvm::Value *Value = ir.getInt32(0);

  if (Buf && Buf->Metadata) {
    Value = DecodeRawBufferByteLength(Buf->Metadata);
    if (Buf->StructureStride) {
      Value = ir.CreateUDiv(Value, ir.getInt32(Buf->StructureStride));
    }
  } else if (auto Tex = LoadTexture(bufinfo.src); Tex) {
    Value = DecodeTextureBufferElement(Tex->Metadata);
  }

  StoreOperand(bufinfo.dst, Value);
}

void
Converter::operator()(const InstLoadRaw &load) {
  using namespace llvm::air;

  auto Buf = LoadBuffer(load.src);
  if (!Buf)
    return;

  mask_t Mask = GetMask(load.dst);
  bool Volatile = cast<llvm::PointerType>(Buf->Pointer->getType())->getAddressSpace() == 3;

  auto Index = ir.CreateLShr(LoadOperand(load.src_byte_offset, kMaskComponentX), 2);

  if (auto Comp = ComponentFromScalarMask(Mask, Buf->Swizzle); Comp >= 0) {
    auto Ptr = CreateGEPInt32WithBoundCheck(Buf.value(), ir.CreateAdd(Index, ir.getInt32(Comp)));
    auto ValueInt = Buf->GlobalCoherent ? (llvm::Value *)air.CreateDeviceCoherentLoad(ir.getInt32Ty(), Ptr)
                                        : (llvm::Value *)ir.CreateLoad(ir.getInt32Ty(), Ptr, Volatile);
    return StoreOperand(load.dst, MaskSwizzle(ValueInt, Mask));
  }

  llvm::Value *ValueVec = llvm::PoisonValue::get(air.getIntTy(4));
  for (auto [DstComp, _] : EnumerateComponents(MemoryAccessMask(Mask, Buf->Swizzle))) {
    auto Ptr = CreateGEPInt32WithBoundCheck(Buf.value(), ir.CreateAdd(Index, ir.getInt32(DstComp)));
    auto ValueInt = Buf->GlobalCoherent ? (llvm::Value *)air.CreateDeviceCoherentLoad(ir.getInt32Ty(), Ptr)
                                        : (llvm::Value *)ir.CreateLoad(ir.getInt32Ty(), Ptr, Volatile);
    ValueVec = ir.CreateInsertElement(ValueVec, ValueInt, DstComp);
  }
  StoreOperand(load.dst, MaskSwizzle(ValueVec, Mask, Buf->Swizzle));
}

void
Converter::operator()(const InstLoadStructured &load) {
  using namespace llvm::air;

  auto Buf = LoadBuffer(load.src);
  if (!Buf)
    return;

  mask_t Mask = GetMask(load.dst);
  bool Volatile = cast<llvm::PointerType>(Buf->Pointer->getType())->getAddressSpace() == 3;

  auto IndexStruct =
      ir.CreateMul(ir.getInt32(Buf->StructureStride >> 2), LoadOperand(load.src_address, kMaskComponentX));
  auto Index = ir.CreateAdd(IndexStruct, ir.CreateLShr(LoadOperand(load.src_byte_offset, kMaskComponentX), 2));

  if (auto Comp = ComponentFromScalarMask(Mask, Buf->Swizzle); Comp >= 0) {
    auto Ptr = CreateGEPInt32WithBoundCheck(Buf.value(), ir.CreateAdd(Index, ir.getInt32(Comp)));
    auto ValueInt = Buf->GlobalCoherent ? (llvm::Value *)air.CreateDeviceCoherentLoad(ir.getInt32Ty(), Ptr)
                                        : (llvm::Value *)ir.CreateLoad(ir.getInt32Ty(), Ptr, Volatile);
    return StoreOperand(load.dst, MaskSwizzle(ValueInt, Mask));
  }

  llvm::Value *ValueVec = llvm::PoisonValue::get(air.getIntTy(4));
  for (auto [DstComp, _] : EnumerateComponents(MemoryAccessMask(Mask, Buf->Swizzle))) {
    auto Ptr = CreateGEPInt32WithBoundCheck(Buf.value(), ir.CreateAdd(Index, ir.getInt32(DstComp)));
    auto ValueInt = Buf->GlobalCoherent ? (llvm::Value *)air.CreateDeviceCoherentLoad(ir.getInt32Ty(), Ptr)
                                        : (llvm::Value *)ir.CreateLoad(ir.getInt32Ty(), Ptr, Volatile);
    ValueVec = ir.CreateInsertElement(ValueVec, ValueInt, DstComp);
  }
  StoreOperand(load.dst, MaskSwizzle(ValueVec, Mask, Buf->Swizzle));
}

void
Converter::operator()(const InstStoreRaw &store) {
  using namespace llvm::air;

  auto Buf = LoadBuffer(store.dst);
  if (!Buf)
    return;
  switch (Buf->Mask) {
  case 0b1:
  case 0b11:
  case 0b111:
  case 0b1111:
    break;
  default:
    return;
  }

  bool Volatile = cast<llvm::PointerType>(Buf->Pointer->getType())->getAddressSpace() == 3;

  auto Index = ir.CreateLShr(LoadOperand(store.dst_byte_offset, kMaskComponentX), 2);

  auto Value = LoadOperand(store.src, Buf->Mask);

  for (auto [DstComp, _] : EnumerateComponents(Buf->Mask)) {
    auto Ptr = CreateGEPInt32WithBoundCheck(Buf.value(), ir.CreateAdd(Index, ir.getInt32(DstComp)));
    if (Buf->GlobalCoherent)
      air.CreateDeviceCoherentStore(ExtractElement(Value, DstComp), Ptr);
    else
      ir.CreateStore(ExtractElement(Value, DstComp), Ptr, Volatile);
  }
}

void
Converter::operator()(const InstStoreStructured &store) {
  using namespace llvm::air;

  auto Buf = LoadBuffer(store.dst);
  if (!Buf)
    return;
  switch (Buf->Mask) {
  case 0b1:
  case 0b11:
  case 0b111:
  case 0b1111:
    break;
  default:
    return;
  }

  bool Volatile = cast<llvm::PointerType>(Buf->Pointer->getType())->getAddressSpace() == 3;

  auto IndexStruct =
      ir.CreateMul(ir.getInt32(Buf->StructureStride >> 2), LoadOperand(store.dst_address, kMaskComponentX));
  auto Index = ir.CreateAdd(IndexStruct, ir.CreateLShr(LoadOperand(store.dst_byte_offset, kMaskComponentX), 2));

  auto Value = LoadOperand(store.src, Buf->Mask);

  for (auto [DstComp, _] : EnumerateComponents(Buf->Mask)) {
    auto Ptr = CreateGEPInt32WithBoundCheck(Buf.value(), ir.CreateAdd(Index, ir.getInt32(DstComp)));
    if (Buf->GlobalCoherent)
      air.CreateDeviceCoherentStore(ExtractElement(Value, DstComp), Ptr);
    else
      ir.CreateStore(ExtractElement(Value, DstComp), Ptr, Volatile);
  }
}

llvm::Value *
Converter::LoadAtomicOpAddress(const AtomicBufferResourceHandle &Handle, const SrcOperand &Address) {
  if (Handle.StructureStride > 0) {
    auto Address2D = LoadOperand(Address, kMaskVecXY);
    return ir.CreateLShr(
        ir.CreateAdd(
            ir.CreateMul(ir.getInt32(Handle.StructureStride), ExtractElement(Address2D, 0)),
            ExtractElement(Address2D, 1)
        ),
        2
    );
  }
  auto Address1D = LoadOperand(Address, kMaskComponentX);
  return ir.CreateLShr(Address1D, 2);
}

void
Converter::operator()(const InstAtomicBinOp &atomic) {
  using namespace llvm;
  using namespace llvm::air;

  AtomicRMWInst::BinOp Op;

  switch (atomic.op) {
  case AtomicBinaryOp::And:
    Op = AtomicRMWInst::And;
    break;
  case AtomicBinaryOp::Or:
    Op = AtomicRMWInst::Or;
    break;
  case AtomicBinaryOp::Xor:
    Op = AtomicRMWInst::Xor;
    break;
  case AtomicBinaryOp::Add:
    Op = AtomicRMWInst::Add;
    break;
  case AtomicBinaryOp::IMax:
    Op = AtomicRMWInst::Max;
    break;
  case AtomicBinaryOp::IMin:
    Op = AtomicRMWInst::Min;
    break;
  case AtomicBinaryOp::UMax:
    Op = AtomicRMWInst::UMax;
    break;
  case AtomicBinaryOp::UMin:
    Op = AtomicRMWInst::UMin;
    break;
  case AtomicBinaryOp::Xchg:
    Op = AtomicRMWInst::Xchg;
    break;
  }

  auto Buf = LoadBuffer(atomic.dst);

  if (Buf) {
    auto IntPtrOffset = LoadAtomicOpAddress(Buf.getValue(), atomic.dst_address);
    auto Ptr = ir.CreateGEP(ir.getInt32Ty(), Buf->Pointer, {IntPtrOffset});
    auto Value = air.CreateAtomicRMW(Op, Ptr, LoadOperand(atomic.src, kMaskComponentX));
    StoreOperand(atomic.dst_original, Value);
    return;
  }

  auto Tex = LoadTexture(atomic.dst);

  if (Tex) {
    llvm::Value *Address = nullptr;
    llvm::Value *ArrayIndex = nullptr;

    switch (Tex->Logical) {
    case Texture::texture_buffer:
      Address = LoadOperand(atomic.dst_address, kMaskComponentX);
      Address = ir.CreateAdd(Address, DecodeTextureBufferOffset(Tex->Metadata));
      break;
    case Texture::texture1d:
      Address = LoadOperand(atomic.dst_address, kMaskComponentX);
      Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
      break;
    case Texture::texture1d_array:
      Address = LoadOperand(atomic.dst_address, kMaskComponentX);
      Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
      ArrayIndex = LoadOperand(atomic.dst_address, kMaskComponentY);
      break;
    case Texture::texture2d:
      Address = LoadOperand(atomic.dst_address, kMaskVecXY);
      break;
    case Texture::texture2d_array:
      Address = LoadOperand(atomic.dst_address, kMaskVecXY);
      ArrayIndex = LoadOperand(atomic.dst_address, kMaskComponentZ);
      break;
    case Texture::texture3d:
      Address = LoadOperand(atomic.dst_address, kMaskVecXYZ);
      break;
    default:
      return;
    }
    auto Value =
        air.CreateAtomicRMW(Tex->Texture, Tex->Handle, Op, Address, LoadOperand(atomic.src, kMaskAll), ArrayIndex);
    StoreOperand(atomic.dst_original, Value);
    return;
  }
}

void
Converter::operator()(const InstAtomicImmCmpExchange &atomic) {
  using namespace llvm;
  using namespace llvm::air;

  auto Buf = LoadBuffer(atomic.dst_resource);

  if (Buf) {
    auto IntPtrOffset = LoadAtomicOpAddress(Buf.getValue(), atomic.dst_address);
    auto Ptr = ir.CreateGEP(ir.getInt32Ty(), Buf->Pointer, {IntPtrOffset});
    auto Value = ir.CreateAtomicCmpXchg(
        Ptr, LoadOperand(atomic.src0, kMaskComponentX), LoadOperand(atomic.src1, kMaskComponentX), {},
        AtomicOrdering::Monotonic, AtomicOrdering::Monotonic
    );
    StoreOperand(atomic.dst, ir.CreateExtractValue(Value, 0));
    return;
  }

  auto Tex = LoadTexture(atomic.dst_resource);

  if (Tex) {
    llvm::Value *Address = nullptr;
    llvm::Value *ArrayIndex = nullptr;

    switch (Tex->Logical) {
    case Texture::texture_buffer:
      Address = LoadOperand(atomic.dst_address, kMaskComponentX);
      Address = ir.CreateAdd(Address, DecodeTextureBufferOffset(Tex->Metadata));
      break;
    case Texture::texture1d:
      Address = LoadOperand(atomic.dst_address, kMaskComponentX);
      Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
      break;
    case Texture::texture1d_array:
      Address = LoadOperand(atomic.dst_address, kMaskComponentX);
      Address = ir.CreateInsertElement(llvm::ConstantInt::getNullValue(air.getIntTy(2)), Address, 0ull);
      ArrayIndex = LoadOperand(atomic.dst_address, kMaskComponentY);
      break;
    case Texture::texture2d:
      Address = LoadOperand(atomic.dst_address, kMaskVecXY);
      break;
    case Texture::texture2d_array:
      Address = LoadOperand(atomic.dst_address, kMaskVecXY);
      ArrayIndex = LoadOperand(atomic.dst_address, kMaskComponentZ);
      break;
    case Texture::texture3d:
      Address = LoadOperand(atomic.dst_address, kMaskVecXYZ);
      break;
    default:
      return;
    }
    auto [Value, Flag_DISCARDED] = air.CreateAtomicCmpXchg(
        Tex->Texture, Tex->Handle, Address, LoadOperand(atomic.src0, kMaskAll), LoadOperand(atomic.src1, kMaskAll),
        ArrayIndex
    );
    StoreOperand(atomic.dst, Value);
    return;
  }
}

void
Converter::operator()(const InstAtomicImmIncrement &atomic) {
  using namespace llvm;
  using namespace llvm::air;

  auto Ctr = LoadCounter(atomic.uav);
  if (!Ctr)
    return;

  auto Value = air.CreateAtomicRMW(AtomicRMWInst::Add, Ctr->Pointer, ir.getInt32(1));
  StoreOperand(atomic.dst, Value);
}
void
Converter::operator()(const InstAtomicImmDecrement &atomic) {
  using namespace llvm;
  using namespace llvm::air;

  auto Ctr = LoadCounter(atomic.uav);
  if (!Ctr)
    return;

  auto Value = air.CreateAtomicRMW(AtomicRMWInst::Sub, Ctr->Pointer, ir.getInt32(1));
  // imm_atomic_consume returns new value
  StoreOperand(atomic.dst, ir.CreateSub(Value, ir.getInt32(1)));
}

llvm::Optional<InterpolantHandle>
Converter::LoadInterpolant(uint32_t Index) {
  if (!res.interpolant_map.contains(Index))
    return {};

  auto interpolant = res.interpolant_map[Index];
  auto h = interpolant.interpolant(nullptr).build(ctx);
  if (h.takeError())
    return {};

  return llvm::Optional<InterpolantHandle>({h.get(), interpolant.perspective});
}

void
Converter::operator()(const InstInterpolateCentroid &eval) {
  auto Itp = LoadInterpolant(eval.regid);
  if (!Itp)
    return;

  auto Value = air.CreateInterpolateAtCentroid(Itp->Handle, Itp->Perspective);
  StoreOperand(eval.dst, MaskSwizzle(Value, GetMask(eval.dst), eval.read_swizzle));
}

void
Converter::operator()(const InstInterpolateSample &eval) {
  auto Itp = LoadInterpolant(eval.regid);
  if (!Itp)
    return;

  auto Value =
      air.CreateInterpolateAtSample(Itp->Handle, LoadOperand(eval.sample_index, kMaskComponentX), Itp->Perspective);
  StoreOperand(eval.dst, MaskSwizzle(Value, GetMask(eval.dst), eval.read_swizzle));
}

void
Converter::operator()(const InstInterpolateOffset &eval) {
  auto Itp = LoadInterpolant(eval.regid);
  if (!Itp)
    return;

  auto Offset = LoadOperand(eval.offset, kMaskVecXY);
  // truncated = (offset.xy + 8) & 0b1111
  auto Truncated = ir.CreateAnd(ir.CreateAdd(Offset, air.getInt2(8, 8)), air.getInt2(0b1111, 0b1111));
  auto OffsetFloat = ir.CreateFMul(air.CreateConvertToFloat(Truncated), air.getFloat2(1.0f / 16.0f, 1.0f / 16.0f));
  auto Value = air.CreateInterpolateAtOffset(Itp->Handle, OffsetFloat, Itp->Perspective);
  StoreOperand(eval.dst, MaskSwizzle(Value, GetMask(eval.dst), eval.read_swizzle));
}

void
Converter::operator()(const InstMaskedSumOfAbsDiff &msad) {
  using namespace llvm;

  mask_t Mask = GetMask(msad.dst);
  auto Ref = LoadOperand(msad.src0, Mask);
  auto Src = LoadOperand(msad.src1, Mask);
  auto Accum = LoadOperand(msad.src2, Mask);

  auto &Context = air.getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(Ref->getType());
  Ops.push_back(Ref);
  Tys.push_back(Src->getType());
  Ops.push_back(Src);
  Tys.push_back(Accum->getType());
  Ops.push_back(Accum);

  std::string FnName = "dxmt.msad";
  FnName += air.getTypeOverloadSuffix(Ref->getType());

  auto Fn = air.getModule()->getOrInsertFunction(FnName, FunctionType::get(Accum->getType(), Tys, false), Attrs);
  auto Result = ir.CreateCall(Fn, Ops);

  StoreOperand(msad.dst, Result);
}

void
Converter::operator()(const InstEmit &) {
  if (res.call_emit().build(ctx).takeError()) {
    // TODO
  }
}
void
Converter::operator()(const InstCut &) {
  if (res.call_cut().build(ctx).takeError()) {
    // TODO
  }
}

llvm::Value *
Converter::GetSamplePos(llvm::Value *SampleCount, llvm::Value *Index) {
  using namespace llvm;

  auto &Context = air.getContext();
  auto Attrs = AttributeList::get(
      Context, {{~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadNone)}}
  );

  std::string FnName = "dxmt.sample_pos.i32";
  auto Fn = air.getModule()->getOrInsertFunction(
      FnName, llvm::FunctionType::get(air.getFloatTy(2), {air.getIntTy(), air.getIntTy()}, false), Attrs
  );
  return ir.CreateCall(Fn, {SampleCount, Index});
}
void
Converter::HullGenerateWorkloadForTriangle(
    llvm::Value *PatchIndex, llvm::Value *CountPtr, llvm::Value *DataPtr, TessellatorPartitioning Partitioning,
    llvm::Value *TessFactorIn, llvm::Value *TessFactorOut0, llvm::Value *TessFactorOut1, llvm::Value *TessFactorOut2
) {
  using namespace llvm;

  auto &Context = air.getContext();
  auto Attrs = AttributeList::get(
      Context, {{3U, Attribute::get(Context, Attribute::AttrKind::WriteOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(PatchIndex->getType());
  Ops.push_back(PatchIndex);
  Tys.push_back(CountPtr->getType());
  Ops.push_back(CountPtr);
  Tys.push_back(DataPtr->getType());
  Ops.push_back(DataPtr);
  Tys.push_back(TessFactorIn->getType());
  Ops.push_back(TessFactorIn);
  Tys.push_back(TessFactorOut0->getType());
  Ops.push_back(TessFactorOut0);
  Tys.push_back(TessFactorOut1->getType());
  Ops.push_back(TessFactorOut1);
  Tys.push_back(TessFactorOut2->getType());
  Ops.push_back(TessFactorOut2);

  std::string FnName = "dxmt.generate_workload.triangle";
  switch (Partitioning) {
  case TessellatorPartitioning::integer:
    FnName += ".integer";
    break;
  case TessellatorPartitioning::pow2:
    FnName += ".pow2";
    break;
  case TessellatorPartitioning::fractional_odd:
    FnName += ".odd";
    break;
  case TessellatorPartitioning::fractional_even:
    FnName += ".even";
    break;
  }
  auto Fn = air.getModule()->getOrInsertFunction(FnName, llvm::FunctionType::get(air.getVoidTy(), Tys, false), Attrs);
  ir.CreateCall(Fn, Ops);
}

void
Converter::HullGenerateWorkloadForQuad(
    llvm::Value *PatchIndex, llvm::Value *CountPtr, llvm::Value *DataPtr, TessellatorPartitioning Partitioning,
    llvm::Value *TessFactorIn0, llvm::Value *TessFactorIn1, llvm::Value *TessFactorOut0, llvm::Value *TessFactorOut1,
    llvm::Value *TessFactorOut2, llvm::Value *TessFactorOut3
) {
  using namespace llvm;

  auto &Context = air.getContext();
  auto Attrs = AttributeList::get(
      Context, {{3U, Attribute::get(Context, Attribute::AttrKind::WriteOnly)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(PatchIndex->getType());
  Ops.push_back(PatchIndex);
  Tys.push_back(CountPtr->getType());
  Ops.push_back(CountPtr);
  Tys.push_back(DataPtr->getType());
  Ops.push_back(DataPtr);
  Tys.push_back(TessFactorIn0->getType());
  Ops.push_back(TessFactorIn0);
  Tys.push_back(TessFactorIn1->getType());
  Ops.push_back(TessFactorIn1);
  Tys.push_back(TessFactorOut0->getType());
  Ops.push_back(TessFactorOut0);
  Tys.push_back(TessFactorOut1->getType());
  Ops.push_back(TessFactorOut1);
  Tys.push_back(TessFactorOut2->getType());
  Ops.push_back(TessFactorOut2);
  Tys.push_back(TessFactorOut3->getType());
  Ops.push_back(TessFactorOut3);

  std::string FnName = "dxmt.generate_workload.quad";
  switch (Partitioning) {
  case TessellatorPartitioning::integer:
    FnName += ".integer";
    break;
  case TessellatorPartitioning::pow2:
    FnName += ".pow2";
    break;
  case TessellatorPartitioning::fractional_odd:
    FnName += ".odd";
    break;
  case TessellatorPartitioning::fractional_even:
    FnName += ".even";
    break;
  }
  auto Fn = air.getModule()->getOrInsertFunction(FnName, llvm::FunctionType::get(air.getVoidTy(), Tys, false), Attrs);
  ir.CreateCall(Fn, Ops);
}

llvm::Value *
Converter::DomainGetPatchIndex(llvm::Value *WorkloadIndex, llvm::Value *DataPtr) {
  using namespace llvm;

  auto &Context = air.getContext();
  auto Attrs = AttributeList::get(
      Context, {{2U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(WorkloadIndex->getType());
  Ops.push_back(WorkloadIndex);
  Tys.push_back(DataPtr->getType());
  Ops.push_back(DataPtr);

  std::string FnName = "dxmt.get_domain_patch_index";
  auto Fn = air.getModule()->getOrInsertFunction(FnName, llvm::FunctionType::get(air.getIntTy(), Tys, false), Attrs);
  return ir.CreateCall(Fn, Ops);
}

std::tuple<llvm::Value *, llvm::Value *, llvm::Value *>
Converter::DomainGetLocation(
    llvm::Value *WorkloadIndex, llvm::Value *ThreadIndex, llvm::Value *DataPtr, TessellatorPartitioning Partitioning
) {
  using namespace llvm;

  auto &Context = air.getContext();
  auto Attrs = AttributeList::get(
      Context, {{3U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {3U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(WorkloadIndex->getType());
  Ops.push_back(WorkloadIndex);
  Tys.push_back(ThreadIndex->getType());
  Ops.push_back(ThreadIndex);
  Tys.push_back(DataPtr->getType());
  Ops.push_back(DataPtr);

  std::string FnName = "dxmt.get_domain_location";
  switch (Partitioning) {
  case TessellatorPartitioning::integer:
    FnName += ".integer";
    break;
  case TessellatorPartitioning::pow2:
    FnName += ".pow2";
    break;
  case TessellatorPartitioning::fractional_odd:
    FnName += ".odd";
    break;
  case TessellatorPartitioning::fractional_even:
    FnName += ".even";
    break;
  }
  auto Fn = air.getModule()->getOrInsertFunction(
      FnName,
      llvm::FunctionType::get(
          llvm::StructType::create(Context, {air.getFloatTy(2), air.getByteTy(), air.getByteTy()}, ""), Tys, false
      ),
      Attrs
  );
  auto Return = ir.CreateCall(Fn, Ops);
  return {
      ir.CreateExtractValue(Return, 0ull), ir.CreateIsNotNull(ir.CreateExtractValue(Return, 1)),
      ir.CreateIsNotNull(ir.CreateExtractValue(Return, 2))
  };
}

void
Converter::DomainGeneratePrimitives(
    llvm::Value *WorkloadIndex, llvm::Value *DataPtr, TessellatorOutputPrimitive Primitive
) {
  using namespace llvm;

  auto &Context = air.getContext();
  auto Attrs = AttributeList::get(
      Context, {{2U, Attribute::get(Context, Attribute::AttrKind::ReadOnly)},
                {2U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {3U, Attribute::get(Context, Attribute::AttrKind::NoCapture)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(Context, Attribute::AttrKind::WillReturn)}}
  );

  SmallVector<Value *> Ops;
  SmallVector<Type *> Tys;

  Tys.push_back(WorkloadIndex->getType());
  Ops.push_back(WorkloadIndex);
  Tys.push_back(DataPtr->getType());
  Ops.push_back(DataPtr);
  Tys.push_back(air.getMeshHandleType());
  Ops.push_back(air.getMeshHandle());

  std::string FnName = "dxmt.domain_generate_primitives";
  switch (Primitive) {
  case TessellatorOutputPrimitive::point:
    FnName += ".point";
    break;
  case TessellatorOutputPrimitive::line:
    FnName += ".line";
    break;
  case TessellatorOutputPrimitive::triangle:
    FnName += ".triangle";
    break;
  case TessellatorOutputPrimitive::triangle_ccw:
    FnName += ".triangle_ccw";
    break;
  }
  auto Fn = air.getModule()->getOrInsertFunction(FnName, llvm::FunctionType::get(air.getVoidTy(), Tys, false), Attrs);
  ir.CreateCall(Fn, Ops);
}

llvm::Value *
Converter::CreateGEPInt32WithBoundCheck(BufferResourceHandle &Buffer, llvm::Value *Index) {
  auto Addr = ir.CreateGEP(ir.getInt32Ty(), Buffer.Pointer, {Index});
  if (!Buffer.Metadata) {
    return Addr;
  }
  auto ByteLength = DecodeRawBufferByteLength(Buffer.Metadata);
  return ir.CreateSelect(
      ir.CreateICmpULT(Index, ir.CreateLShr(ByteLength, 2)), Addr, llvm::Constant::getNullValue(Addr->getType())
  );
}

llvm::Value *
Converter::CreateGEPInt32WithBoundCheck(AtomicBufferResourceHandle &Buffer, llvm::Value *Index) {
  auto Addr = ir.CreateGEP(ir.getInt32Ty(), Buffer.Pointer, {Index});
  if (!Buffer.Metadata) {
    return Addr;
  }
  auto ByteLength = DecodeRawBufferByteLength(Buffer.Metadata);

  return ir.CreateSelect(
      ir.CreateICmpULT(Index, ir.CreateLShr(ByteLength, 2)), Addr, llvm::Constant::getNullValue(Addr->getType())
  );
}

std::unique_ptr<llvm::IRBuilder<>::FastMathFlagGuard>
Converter::UseFastMath(bool OptOut) {
  if (OptOut)
    return nullptr;
  auto Guard = std::make_unique<llvm::IRBuilder<>::FastMathFlagGuard>(ir);
  llvm::FastMathFlags FMF;

  /**
  TODO(airconv): provides option to allow NaN/INF
  TODO(invariance-analysis): for now fp contraction&refactoring is simply disabled for pre-raster stage to reduce the
  chance of depth prepass z-fighting. However if the game (e.g. SotTR) uses two versions of expression (mul+add vs. fma)
  this is not handled yet (need further investigation, probably need a pass for un-fuse/re-fuse)
  */
  if (ctx.shader_type == microsoft::D3D11_SB_COMPUTE_SHADER || ctx.shader_type == microsoft::D3D10_SB_PIXEL_SHADER) {
    FMF.setAllowContract();
    FMF.setAllowReassoc();
    FMF.setAllowReciprocal();
  }

  FMF.setApproxFunc();
  FMF.setNoSignedZeros();
  ir.setFastMathFlags(FMF);
  return Guard;
}

bool
Converter::SupportsMemoryCoherency() const {
  return ctx.metal_version >= SM50_SHADER_METAL_320;
}

bool
Converter::SupportsNonExecutionBarrier() const {
  return ctx.metal_version >= SM50_SHADER_METAL_320;
}

} // namespace dxmt::dxbc