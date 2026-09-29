#include "dxbc_io_helper.hpp"
#include "../airconv_error.hpp"
#include "../dxbc_converter.hpp"
#include "air_vertex_puller.hpp"

#include "llvm/ADT/SmallVector.h"
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
      return value;
    if (vecTy->getNumElements() == 3)
      return ir.CreateShuffleVector(value, {0, 1, 2, 2});
    if (vecTy->getNumElements() == 2)
      return ir.CreateShuffleVector(value, {0, 1, 1, 1});
    if (vecTy->getNumElements() == 1)
      return ir.CreateShuffleVector(value, {0, 0, 0, 0});
    return ir.CreateShuffleVector(value, {0, 1, 2, 3});
  } else {
    return ir.CreateVectorSplat(4, value);
  }
}

llvm::Value *
IOHelper::ToDesiredTypeFromIntVec4(llvm::Value *vec4, llvm::Type *desired, uint32_t mask) {
  llvm::Type *scalar = desired->getScalarType();
  assert((scalar == types._int || scalar == types._float) && "unhandled desired element type");

  // reinterpret the integer register as float when a float type is requested
  llvm::Value *value = scalar == types._float ? ir.CreateBitCast(vec4, types._float4) : vec4;

  if (auto *vector = llvm::dyn_cast<llvm::FixedVectorType>(desired)) {
    unsigned num_elements = vector->getNumElements();
    llvm::SmallVector<int, 4> indices;
    for (unsigned i = 0; i < num_elements; i++)
      indices.push_back(mask & (1 << i) ? (int)i : llvm::UndefMaskElem);
    return ir.CreateShuffleVector(value, indices);
  }

  assert(mask);
  return ir.CreateExtractElement(value, mask ? (uint64_t)__builtin_ctz(mask) : 0);
};

llvm::Value *
IOHelper::LoadFromArrayAt(llvm::Value *array, llvm::Value *index) {
  auto array_ty = llvm::cast<llvm::ArrayType>( // force line break
      llvm::cast<llvm::PointerType>(array->getType())->getNonOpaquePointerElementType()
  );
  auto ptr = ir.CreateGEP(array_ty, array, {air.getInt(0), index}, "", llvm::isa<llvm::ConstantInt>(index));
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
    auto component_ptr = ir.CreateGEP(array_type, array, {air.getInt(0), index, air.getInt(i)});
    value = ir.CreateInsertElement(value, ir.CreateLoad(ele_type, component_ptr), uint64_t(i));
  }
  return value;
};

llvm::Error
IOHelper::StoreAtVec4ArrayMasked(llvm::Value *array, llvm::Value *index, llvm::Value *maybe_vec4, uint32_t mask) {
  auto vec4 = ExtendToVec4(maybe_vec4);
  if (mask == 0b1111) {
    auto ptr = ir.CreateInBoundsGEP(
        llvm::cast<llvm::PointerType>(array->getType())->getNonOpaquePointerElementType(), array, {air.getInt(0), index}
    );
    ir.CreateStore(vec4, ptr);
    return llvm::Error::success();
  }
  for (unsigned i = 0; i < 4; i++) {
    if ((mask & (1 << i)) == 0)
      continue;
    auto component_ptr = ir.CreateGEP(
        llvm::cast<llvm::PointerType>(array->getType())->getNonOpaquePointerElementType(), array,
        {air.getInt(0), index, air.getInt(i)}
    );
    ir.CreateStore(ir.CreateExtractElement(vec4, i), component_ptr);
  }
  return llvm::Error::success();
};

llvm::Error
IOHelper::InitializeInput(uint32_t with_fnarg_at, uint32_t to_reg, uint32_t mask, bool fix_w_component) {
  llvm::Value *arg = function->getArg(with_fnarg_at);
  if (arg->getType()->getScalarType()->isIntegerTy()) {
    if (arg->getType()->getScalarType() == types._bool) {
      // front_facing
      return StoreAtVec4ArrayMasked(io.input.ptr_int4, air.getInt(to_reg), ir.CreateSExt(arg, types._int), mask);
    } else if (arg->getType()->getScalarType() != types._int) {
      return llvm::make_error<UnsupportedFeature>("TODO: handle non 32 bit integer");
    } else {
      return StoreAtVec4ArrayMasked(io.input.ptr_int4, air.getInt(to_reg), arg, mask);
    }
  } else if (arg->getType()->getScalarType()->isFloatTy()) {
    if (fix_w_component && isa<llvm::FixedVectorType>(arg->getType()) &&
        cast<llvm::FixedVectorType>(arg->getType())->getNumElements() == 4) {
      auto w_component = ir.CreateExtractElement(arg, 3);
      auto rcp_w = ir.CreateFDiv(air.getFloat(1.0f), w_component);
      arg = ir.CreateInsertElement(arg, rcp_w, 3);
    }
    return StoreAtVec4ArrayMasked(io.input.ptr_float4, air.getInt(to_reg), arg, mask);
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
    return llvm::make_error<UnsupportedFeature>("unexpected interpolant mode: flat");
  }
  return StoreAtVec4ArrayMasked(io.input.ptr_float4, air.getInt(to_reg), interpolated_val, mask);
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
  } else if (vec4->getType() == types._int4) {
    return StoreAtVec4ArrayMasked(io.input.ptr_int4, builder.getInt32(element_info.reg), vec4, mask);
  } else {
    return llvm::make_error<UnsupportedFeature>("unexpected value type from vertex puller");
  }
};

llvm::Error
IOHelper::InitializeVertexIDInput(uint32_t to_reg, uint32_t mask) {
  return StoreAtVec4ArrayMasked(io.input.ptr_int4, air.getInt(to_reg), io.vertex_id, mask);
}

llvm::Error
IOHelper::InitializeInstanceIDInput(uint32_t to_reg, uint32_t mask) {
  return StoreAtVec4ArrayMasked(io.input.ptr_int4, air.getInt(to_reg), io.instance_id, mask);
}

llvm::Error
IOHelper::InitializeInputAttribute(shader::common::InputAttribute value, uint32_t from_fnarg_at) {
  auto arg = function->getArg(from_fnarg_at);
  switch (value) {
  case shader::common::InputAttribute::ThreadId:
    io.thread_id_arg = arg;
    break;
  case shader::common::InputAttribute::ThreadGroupId:
    io.thread_group_id_arg = arg;
    break;
  case shader::common::InputAttribute::ThreadIdInGroup:
    io.thread_id_in_group_arg = arg;
    break;
  case shader::common::InputAttribute::ThreadIdInGroupFlatten:
    io.thread_id_in_group_flat_arg = arg;
    break;
  case shader::common::InputAttribute::CoverageMask:
    io.coverage_mask_arg = arg;
    break;
  default:
    return llvm::make_error<UnsupportedFeature>("unhandled input system value");
  }
  return llvm::Error::success();
}

llvm::Error
IOHelper::CreateDepthOutputRegister() {
  if (io.depth_output_reg != nullptr)
    return llvm::make_error<UnsupportedFeature>("oDepth is defined twice");
  io.depth_output_reg = ir.CreateAlloca(types._float);
  return llvm::Error::success();
}

llvm::Error
IOHelper::CreateStencilRefOutputRegister() {
  if (io.stencil_ref_reg != nullptr)
    return llvm::make_error<UnsupportedFeature>("oStencil is defined twice");
  io.stencil_ref_reg = ir.CreateAlloca(types._int);
  return llvm::Error::success();
}

llvm::Error
IOHelper::CreateCoverageMaskOutputRegister() {
  if (io.coverage_mask_reg != nullptr)
    return llvm::make_error<UnsupportedFeature>("oMask is defined twice");
  io.coverage_mask_reg = ir.CreateAlloca(types._int);
  return llvm::Error::success();
}

llvm::Value *
IOHelper::PopulateOutput(uint32_t from_reg, uint32_t mask, llvm::Value *StructRet, uint32_t to_element) {
  auto desired_type = function->getReturnType()->getStructElementType(to_element);
  auto value = ToDesiredTypeFromIntVec4(LoadFromArrayAt(io.output.ptr_int4, air.getInt(from_reg)), desired_type, mask);
  return ir.CreateInsertValue(StructRet, value, {to_element});
}

llvm::Value *
IOHelper::PopulateOutputFixUnorm(uint32_t from_reg, uint32_t mask, llvm::Value *StructRet, uint32_t to_element) {
  auto fvec4 = LoadFromArrayAt(io.output.ptr_float4, air.getInt(from_reg));
  auto fixed = ir.CreateFSub(fvec4, GetFloat4Splat(1.0f / 127500.0f) /* magic delta ?! */);
  return ir.CreateInsertValue(StructRet, fixed, {to_element});
}

llvm::Value *
IOHelper::PopulateOutputSanitizePosition(
    uint32_t from_reg, uint32_t mask, llvm::Value *StructRet, uint32_t to_element
) {
  auto fvec4 = LoadFromArrayAt(io.output.ptr_float4, air.getInt(from_reg));
  auto fixed = air.SanitizePosition(fvec4);
  return ir.CreateInsertValue(StructRet, fixed, {to_element});
}

llvm::Value *
IOHelper::PopulateOutputDepth(llvm::Value *StructRet, uint32_t to_element) {
  auto value = ir.CreateLoad(types._float, ir.CreateConstInBoundsGEP1_32(types._float, io.depth_output_reg, 0));
  return ir.CreateInsertValue(StructRet, value, {to_element});
}

llvm::Value *
IOHelper::PopulateOutputStencilRef(llvm::Value *StructRet, uint32_t to_element) {
  auto value = ir.CreateLoad(types._int, ir.CreateConstInBoundsGEP1_32(types._int, io.stencil_ref_reg, 0));
  return ir.CreateInsertValue(StructRet, value, {to_element});
}

llvm::Value *
IOHelper::PopulateOutputCoverageMask(llvm::Value *StructRet, uint32_t to_element, uint32_t sample_mask) {
  llvm::Value *value = ir.CreateLoad(types._int, ir.CreateConstInBoundsGEP1_32(types._int, io.coverage_mask_reg, 0));
  if (sample_mask != 0xffffffff)
    value = ir.CreateAnd(value, sample_mask);
  return ir.CreateInsertValue(StructRet, value, {to_element});
}

llvm::Value *
IOHelper::PopulateDefaultCoverageMask(llvm::Value *StructRet, uint32_t to_element, uint32_t sample_mask) {
  if (io.coverage_mask_reg)
    return StructRet;
  return ir.CreateInsertValue(StructRet, air.getInt(sample_mask), {to_element});
}

llvm::Value *
IOHelper::PopulateOutputStreamOutput(
    llvm::Value *StructRet, const SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA *vertex_so,
    uint32_t base_vertex_arg_index, uint32_t vertex_id_arg_index, uint32_t stream_output_table_arg_index
) {
  auto base_vertex = function->getArg(base_vertex_arg_index);
  auto vertex_id = function->getArg(vertex_id_arg_index);
  auto so_entries_type = types._dxmt_stream_output_buffer_entry;
  auto so_entries = ir.CreateBitCast(
      function->getArg(stream_output_table_arg_index),
      so_entries_type->getPointerTo((uint32_t)air::AddressSpace::constant)
  );
  auto slot0_entry = ir.CreateLoad(so_entries_type, ir.CreateConstInBoundsGEP1_32(so_entries_type, so_entries, 0));
  auto slot0 = ir.CreateExtractValue(slot0_entry, {0});
  auto adjusted_vertex_id = ir.CreateSub(vertex_id, base_vertex);
  auto output_regs = ir.CreateBitOrPointerCast(io.output.ptr_int4, llvm::PointerType::get(types._int, 0));
  for (unsigned i = 0; i < vertex_so->num_elements; i++) {
    auto &element = vertex_so->elements[i];
    if (element.reg_id == 0xffffffff)
      continue;
    auto ptr = ir.CreateConstGEP1_32(types._int, output_regs, (unsigned)(element.reg_id * 4 + element.component));
    auto target_offset = ir.CreateAdd(
        ir.CreateMul(adjusted_vertex_id, air.getInt(vertex_so->strides[element.output_slot /* expected to be 0 */])),
        air.getInt(element.offset)
    );
    auto target_ptr = ir.CreateGEP(types._int, slot0, {ir.CreateLShr(target_offset, 2)});
    ir.CreateStore(ir.CreateLoad(types._int, ptr), target_ptr, true);
  }
  return StructRet;
}

void
IOHelper::PopulateMeshOutputRenderTargetArrayIndex(uint32_t from_reg, uint32_t mask, llvm::Value *primitive_id) {
  auto result = ToDesiredTypeFromIntVec4(LoadFromArrayAt(io.output.ptr_int4, air.getInt(from_reg)), types._int, mask);
  air.CreateSetMeshRenderTargetArrayIndex(primitive_id, result);
}

void
IOHelper::PopulateMeshOutputViewportArrayIndex(uint32_t from_reg, uint32_t mask, llvm::Value *primitive_id) {
  auto result = ToDesiredTypeFromIntVec4(LoadFromArrayAt(io.output.ptr_int4, air.getInt(from_reg)), types._int, mask);
  air.CreateSetMeshViewportArrayIndex(primitive_id, result);
}

void
IOHelper::PopulateMeshOutputPosition(uint32_t from_reg, uint32_t mask, llvm::Value *vertex_id) {
  auto result = LoadFromArrayAt(io.output.ptr_float4, air.getInt(from_reg));
  air.CreateSetMeshPosition(vertex_id, result);
}

void
IOHelper::PopulateMeshOutputVertexData(
    uint32_t from_reg, uint32_t mask, uint32_t idx, llvm::Value *vertex_id, air::MSLScalerOrVectorType desired_type
) {
  auto result = ToDesiredTypeFromIntVec4(
      LoadFromVec4ArrayMasked(io.output.ptr_int4, air.getInt(from_reg), mask),
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
