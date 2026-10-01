#include "dxbc_io_helper.hpp"
#include "../airconv_error.hpp"
#include "../dxbc_converter.hpp"
#include "air_vertex_puller.hpp"

#include "llvm/IR/Constants.h"

namespace dxmt::dxbc {

IOHelper::IOHelper(
    llvm::air::AIRBuilder &air, llvm::Function *function, air::AirType &types, io_binding_map &io_legacy
) :
    air(air),
    ir(air.builder),
    function(function),
    types(types),
    io(io_legacy) {}

llvm::Value *
IOHelper::GetFloat4Splat(float value) {
  return llvm::ConstantVector::get(
      {llvm::ConstantFP::get(air.getContext(), llvm::APFloat{value}),
       llvm::ConstantFP::get(air.getContext(), llvm::APFloat{value}),
       llvm::ConstantFP::get(air.getContext(), llvm::APFloat{value}),
       llvm::ConstantFP::get(air.getContext(), llvm::APFloat{value})}
  );
};

llvm::Value *
IOHelper::ExtendToVec4(llvm::Value *value) {
  auto ty = value->getType();
  if (ty->isVectorTy()) {
    auto vecTy = llvm::cast<llvm::FixedVectorType>(ty);
    if (vecTy->getNumElements() == 4)
      return ir.CreateShuffleVector(value, {0, 1, 2, 3});
    if (vecTy->getNumElements() == 3)
      return ir.CreateShuffleVector(value, {0, 1, 2, 2});
    if (vecTy->getNumElements() == 2)
      return ir.CreateShuffleVector(value, {0, 1, 1, 1});
    if (vecTy->getNumElements() == 1)
      return ir.CreateShuffleVector(value, {0, 0, 0, 0});
    assert(0 && "?");
  } else {
    return ir.CreateVectorSplat(4, value);
  }
}

llvm::Value *
IOHelper::ToDesiredTypeFromIntVec4(llvm::Value *vec4, llvm::Type *desired, uint32_t mask) {
  assert(vec4->getType() == types._int4);
  std::function<llvm::Value *(llvm::Value *, llvm::Type *)> convert = [this, &convert,
                                                                       mask](llvm::Value *vec4, llvm::Type *desired) {
    auto masked = [mask](int i) { return (mask & (1 << i)) ? i : llvm::UndefMaskElem; };
    if (desired == types._int4)
      return ir.CreateShuffleVector(vec4, {masked(0), masked(1), masked(2), masked(3)});
    if (desired == types._float4)
      return ir.CreateShuffleVector(
          ir.CreateBitCast(vec4, types._float4), {masked(0), masked(1), masked(2), masked(3)}
      );
    // FIXME: 3d/2d vector implementations are unused and probably wrong!
    if (desired == types._int3)
      return ir.CreateShuffleVector(vec4, {masked(0), masked(1), masked(2)});
    if (desired == types._float3)
      return ir.CreateShuffleVector(convert(vec4, types._float4), {masked(0), masked(1), masked(2)});
    if (desired == types._int2)
      return ir.CreateShuffleVector(vec4, {masked(0), masked(1)});
    if (desired == types._float2)
      return ir.CreateShuffleVector(convert(vec4, types._float4), {masked(0), masked(1)});
    if (desired == types._int)
      return ir.CreateExtractElement(vec4, (uint64_t)__builtin_ctz(mask));
    if (desired == types._float)
      return ir.CreateExtractElement(ir.CreateBitCast(vec4, types._float4), (uint64_t)__builtin_ctz(mask));
    assert(0 && "unhandled vec4");
  };
  return convert(vec4, desired);
};

llvm::Value *
IOHelper::LoadFromArrayAt(llvm::Value *array, llvm::Value *index) {
  auto array_ty = llvm::cast<llvm::ArrayType>( // force line break
      llvm::cast<llvm::PointerType>(array->getType())->getNonOpaquePointerElementType()
  );
  auto ptr = ir.CreateGEP(array_ty, array, {ir.getInt32(0), index}, "", llvm::isa<llvm::ConstantInt>(index));
  return ir.CreateLoad(array_ty->getElementType(), ptr);
};

llvm::Value *
IOHelper::LoadFromVec4ArrayMasked(llvm::Value *array, llvm::Value *index, uint32_t mask) {
  if (mask == 0b1111) {
    return LoadFromArrayAt(array, index);
  }
  auto array_type = llvm::cast<llvm::PointerType>(array->getType())->getNonOpaquePointerElementType();
  auto vec4_type = llvm::cast<llvm::ArrayType>(array_type)->getArrayElementType();
  auto ele_type = llvm::cast<llvm::VectorType>(vec4_type)->getElementType();
  llvm::Value *value = llvm::ConstantAggregateZero::get(vec4_type);
  for (unsigned i = 0; i < 4; i++) {
    if ((mask & (1 << i)) == 0)
      continue;
    auto component_ptr = ir.CreateGEP(array_type, array, {ir.getInt32(0), index, ir.getInt32(i)});
    value = ir.CreateInsertElement(value, ir.CreateLoad(ele_type, component_ptr), uint64_t(i));
  }
  return value;
};

llvm::Error
IOHelper::StoreAtVec4ArrayMasked(llvm::Value *array, llvm::Value *index, llvm::Value *maybe_vec4, uint32_t mask) {
  auto vec4 = ExtendToVec4(maybe_vec4);
  if (mask == 0b1111) {
    auto ptr = ir.CreateInBoundsGEP(
        llvm::cast<llvm::PointerType>(array->getType())->getNonOpaquePointerElementType(), array,
        {ir.getInt32(0), index}
    );
    ir.CreateStore(vec4, ptr);
    return llvm::Error::success();
  }
  for (unsigned i = 0; i < 4; i++) {
    if ((mask & (1 << i)) == 0)
      continue;
    auto component_ptr = ir.CreateGEP(
        llvm::cast<llvm::PointerType>(array->getType())->getNonOpaquePointerElementType(), array,
        {ir.getInt32(0), index, ir.getInt32(i)}
    );
    ir.CreateStore(ir.CreateExtractElement(vec4, i), component_ptr);
  }
  return llvm::Error::success();
};

llvm::Error
IOHelper::InitializeInput(uint32_t with_fnarg_at, uint32_t to_reg, uint32_t mask, bool fix_w_component) {
  // no it doesn't work like this
  // regular input can be masked like .zw
  // assert((mask & 1) && "todo: handle input register sharing correctly");
  // FIXME: it's buggy as hell
  llvm::Value *arg = function->getArg(with_fnarg_at);
  auto const_index = llvm::ConstantInt::get(air.getContext(), llvm::APInt{32, to_reg, false});
  if (arg->getType()->getScalarType()->isIntegerTy()) {
    if (arg->getType()->getScalarType() == types._bool) {
      // front_facing
      return  StoreAtVec4ArrayMasked(io.input.ptr_int4, const_index, ir.CreateSExt(arg, types._int), mask);
    } else if (arg->getType()->getScalarType() != types._int) {
      return llvm::make_error<UnsupportedFeature>("TODO: handle non 32 bit integer");
    } else {
      return StoreAtVec4ArrayMasked(io.input.ptr_int4, const_index, arg, mask);
    }
  } else if (arg->getType()->getScalarType()->isFloatTy()) {
    if (fix_w_component && isa<llvm::FixedVectorType>(arg->getType()) &&
        cast<llvm::FixedVectorType>(arg->getType())->getNumElements() == 4) {
      auto w_component = ir.CreateExtractElement(arg, 3);
      auto rcp_w = ir.CreateFDiv(llvm::ConstantFP::get(types._float, 1), w_component);
      arg = ir.CreateInsertElement(arg, rcp_w, 3);
    }
    return StoreAtVec4ArrayMasked(io.input.ptr_float4, const_index, arg, mask);
  } else {
    return llvm::make_error<UnsupportedFeature>("TODO: unhandled input type? might be interpolant...");
  }
  // unreachable
  return llvm::Error::success();
};

llvm::Error
IOHelper::InitializeInterpolatedInput(
    uint32_t with_fnarg_at, uint32_t to_reg, uint32_t mask, air::Interpolation interpolation, uint32_t sampleidx_at
) {
  llvm::Value *interpolant = function->getArg(with_fnarg_at);
  auto const_index = llvm::ConstantInt::get(air.getContext(), llvm::APInt{32, to_reg, false});
  llvm::Value *interpolated_val = nullptr;
  switch (interpolation) {
  case air::Interpolation::center_perspective:
    interpolated_val = air.CreateInterpolateAtCenter(interpolant, true);
    break;
  case air::Interpolation::center_no_perspective:
    interpolated_val = air.CreateInterpolateAtCenter(interpolant, false);
    break;
  case air::Interpolation::centroid_perspective:
    interpolated_val = air.CreateInterpolateAtCentroid(interpolant, true);
    break;
  case air::Interpolation::centroid_no_perspective:
    interpolated_val = air.CreateInterpolateAtCentroid(interpolant, false);
    break;
  case air::Interpolation::sample_perspective:
    assert(~sampleidx_at);
    interpolated_val = air.CreateInterpolateAtSample(interpolant, function->getArg(sampleidx_at), true);
    break;
  case air::Interpolation::sample_no_perspective:
    assert(~sampleidx_at);
    interpolated_val = air.CreateInterpolateAtSample(interpolant, function->getArg(sampleidx_at), false);
    break;
  case air::Interpolation::flat:
    assert(0 && "unhandled interpolant mode");
    break;
  }
  return StoreAtVec4ArrayMasked(io.input.ptr_float4, const_index, interpolated_val, mask);
}

llvm::Error
IOHelper::PullVertexInput(
    uint32_t vbuf_table_arg_index, uint32_t to_reg_unused, uint32_t mask, SM50_IA_INPUT_ELEMENT element_info,
    uint32_t slot_mask
) {
  auto &builder = ir;
  llvm::Value *index;             // uint
  if (element_info.step_function) // per_instance
  {
    if (element_info.step_rate) {
      index = builder.CreateAdd(
          io.base_instance_id, builder.CreateUDiv(io.instance_id, builder.getInt32(element_info.step_rate))
      );
    } else {
      // 0 takes special meaning, that the instance data should never be
      // stepped at all
      index = io.base_instance_id;
    }
  } else {
    index = io.vertex_id_with_base;
  }
  if (!io.vertex_buffer_table) {
    io.vertex_buffer_table = builder.CreateBitCast(
        function->getArg(vbuf_table_arg_index),
        types._dxmt_vertex_buffer_entry->getPointerTo((uint32_t)air::AddressSpace::constant)
    );
  }
  unsigned int shift = 32u - element_info.slot;
  unsigned int vertex_buffer_entry_index = element_info.slot ? __builtin_popcount((slot_mask << shift) >> shift) : 0;
  auto vertex_buffer_table = io.vertex_buffer_table;
  auto vertex_buffer_entry = builder.CreateLoad(
      types._dxmt_vertex_buffer_entry,
      builder.CreateConstGEP1_32(types._dxmt_vertex_buffer_entry, vertex_buffer_table, vertex_buffer_entry_index)
  );
  auto base_addr = builder.CreateExtractValue(vertex_buffer_entry, {0});
  auto stride = builder.CreateExtractValue(vertex_buffer_entry, {1});
  auto byte_offset =
      builder.CreateAdd(builder.CreateMul(stride, index), builder.getInt32(element_info.aligned_byte_offset));
  air::VertexPuller puller(air, *function->getParent());
  auto vec4 = puller.PullVec4((air::MTLVertexFormat)element_info.format, base_addr, byte_offset);
  if (vec4->getType() == types._float4) {
    return StoreAtVec4ArrayMasked(io.input.ptr_float4, builder.getInt32(element_info.reg), vec4, mask);
  } else {
    assert(vec4->getType() == types._int4);
    return StoreAtVec4ArrayMasked(io.input.ptr_int4, builder.getInt32(element_info.reg), vec4, mask);
  }
};

llvm::Value *
IOHelper::PopulateOutput(uint32_t from_reg, uint32_t mask, llvm::Value *StructRet, uint32_t to_element) {
  auto const_index = llvm::ConstantInt::get(air.getContext(), llvm::APInt{32, from_reg, false});
  auto ivec4 = LoadFromArrayAt(io.output.ptr_int4, const_index);
  auto desired_type = function->getReturnType()->getStructElementType(to_element);
  auto value = ToDesiredTypeFromIntVec4(ivec4, desired_type, mask);
  return ir.CreateInsertValue(StructRet, value, {to_element});
}

llvm::Value *
IOHelper::PopulateOutputFixUnorm(uint32_t from_reg, uint32_t mask, llvm::Value *StructRet, uint32_t to_element) {
  auto const_index = llvm::ConstantInt::get(air.getContext(), llvm::APInt{32, from_reg, false});
  auto fvec4 = LoadFromArrayAt(io.output.ptr_float4, const_index);
  auto fixed = ir.CreateFSub(fvec4, GetFloat4Splat(1.0f / 127500.0f) /* magic delta ?! */);
  return ir.CreateInsertValue(StructRet, fixed, {to_element});
}

llvm::Value *
IOHelper::PopulateOutputSanitizePosition(
    uint32_t from_reg, uint32_t mask, llvm::Value *StructRet, uint32_t to_element
) {
  auto const_index = llvm::ConstantInt::get(air.getContext(), llvm::APInt{32, from_reg, false});
  auto fvec4 = LoadFromArrayAt(io.output.ptr_float4, const_index);
  auto fixed = air.SanitizePosition(fvec4);
  return ir.CreateInsertValue(StructRet, fixed, {to_element});
}

void
IOHelper::PopulateMeshOutputRenderTargetArrayIndex(uint32_t from_reg, uint32_t mask, llvm::Value *primitive_id) {
  auto result = ToDesiredTypeFromIntVec4(LoadFromArrayAt(io.output.ptr_int4, ir.getInt32(from_reg)), types._int, mask);
  air.CreateSetMeshRenderTargetArrayIndex(primitive_id, result);
}

void
IOHelper::PopulateMeshOutputViewportArrayIndex(uint32_t from_reg, uint32_t mask, llvm::Value *primitive_id) {
  auto result = ToDesiredTypeFromIntVec4(LoadFromArrayAt(io.output.ptr_int4, ir.getInt32(from_reg)), types._int, mask);
  air.CreateSetMeshViewportArrayIndex(primitive_id, result);
}

void
IOHelper::PopulateMeshOutputPosition(uint32_t from_reg, uint32_t mask, llvm::Value *vertex_id) {
  auto result = LoadFromArrayAt(io.output.ptr_float4, ir.getInt32(from_reg));
  air.CreateSetMeshPosition(vertex_id, result);
}

void
IOHelper::PopulateMeshOutputVertexData(
    uint32_t from_reg, uint32_t mask, uint32_t idx, llvm::Value *vertex_id, air::MSLScalerOrVectorType desired_type
) {
  auto result = ToDesiredTypeFromIntVec4(
      LoadFromVec4ArrayMasked(io.output.ptr_int4, ir.getInt32(from_reg), mask),
      air::get_llvm_type(desired_type, air.getContext()), mask
  );
  air.CreateSetMeshVertexData(vertex_id, idx, result);
}

llvm::Error
Prologues::Run(IOHelper &IO) {
  for (auto &fn : functions) {
    if (auto err = fn(IO))
      return err;
  }
  return llvm::Error::success();
}

llvm::Expected<llvm::Value *>
Epilogues::Run(llvm::Type *TyRet, IOHelper &IO) {
  llvm::Value *ret = TyRet->isVoidTy() ? nullptr : llvm::ConstantAggregateZero::get(TyRet);
  for (auto &fn : functions) {
    auto next = fn(ret, IO);
    if (auto err = next.takeError())
      return err;
    ret = next.get();
  }
  return ret;
}

} // namespace dxmt::dxbc
