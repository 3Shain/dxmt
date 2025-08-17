#include "air_operations.hpp"
#include "air_signature.hpp"
#include "air_type.hpp"
#include "airconv_error.hpp"
#include "airconv_public.h"
#include "dxbc_converter.hpp"
#include "ftl.hpp"
#include "nt/air_builder.hpp"
#include "nt/dxbc_converter_base.hpp"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include <stack>

char dxmt::UnsupportedFeature::ID;

namespace dxmt::dxbc {

template <typename T = std::monostate>
ReaderIO<context, T> throwUnsupported(llvm::StringRef ref) {
  return ReaderIO<context, T>(
    [msg = std::string(ref)](struct context ctx) mutable -> llvm::Expected<T> {
      // CAUTION: create llvm::Error as lazy as possible
      // DON'T create it and store somewhere like plain data
      // ^ which make fking perfect sense but NO DONT DO THAT!
      // BECAUSE IT IS STATEFUL! IT PRESEVED A 'CHECKED' STATE
      // to enforce programmer handle any created error
      // ^ imaging you move-capture the error in a lambda
      //   but it's never called and eventually deconstructed
      //   then BOOM
      // what the fuck why should I use it
      // `Monadic error handling w/ a gun shoot yourself in the foot`
      // ^ never claimed to be a monad actually 👆🤓
      // DO I REALLY DESERVE THIS?
      return llvm::Expected<T>(llvm::make_error<UnsupportedFeature>(msg));
    }
  );
};

ReaderIO<context, context> get_context() {
  return ReaderIO<context, context>([](struct context ctx) { return ctx; });
};

auto bitcast_int4(pvalue vec4) {
  return make_irvalue([=](context s) {
    if (vec4->getType() == s.types._int4)
      return vec4;
    return s.builder.CreateBitCast(vec4, s.types._int4);
  });
}

auto truncate_vec(uint8_t elements) {
  assert(elements > 1 && "use extract_element to extract single element");
  return [=](pvalue vec) {
    assert((vec->getType()->getTypeID() == llvm::Type::FixedVectorTyID));
    auto origin_element_count =
      cast<llvm::FixedVectorType>(vec->getType())->getNumElements();
    assert(origin_element_count >= elements);
    return make_irvalue([=](struct context ctx) {
      switch (elements) {
      case 2:
        return ctx.builder.CreateShuffleVector(vec, {0, 1});
      case 3:
        return ctx.builder.CreateShuffleVector(vec, {0, 1, 2});
      case 4:
        return ctx.builder.CreateShuffleVector(vec, {0, 1, 2, 3});
      default:
        assert(0 && "unexpected element count");
      }
    });
  };
};

auto read_int(pvalue vec4, Swizzle swizzle) {
  return bitcast_int4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateExtractElement(bitcasted, swizzle.r);
    });
  };
};

auto read_int2(pvalue vec4, Swizzle swizzle) {
  return bitcast_int4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(bitcasted, {swizzle.r, swizzle.g});
    });
  };
};

auto read_int3(pvalue vec4, Swizzle swizzle) {
  return bitcast_int4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(
        bitcasted, {swizzle.r, swizzle.g, swizzle.b}
      );
    });
  };
};

auto read_int4(pvalue vec4, Swizzle swizzle) {
  if (swizzle == swizzle_identity) {
    return bitcast_int4(vec4);
  }
  return bitcast_int4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(
        bitcasted, {swizzle.r, swizzle.g, swizzle.b, swizzle.a}
      );
    });
  };
};

auto get_int2(uint32_t value0, uint32_t value1) -> IRValue {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantVector::get(
      {llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value0, true}),
       llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value1, true})}
    );
  });
};

auto get_float2(float value0, float value1) -> IRValue {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantVector::get(
      {llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{value0}),
       llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{value1})}
    );
  });
};

auto get_int3(uint32_t value0, uint32_t value1, uint32_t value2) -> IRValue {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantVector::get(
      {llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value0, true}),
       llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value1, true}),
       llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value2, true})}
    );
  });
};

auto get_int(uint32_t value) -> IRValue {
  return make_irvalue([=](context ctx) { return ctx.builder.getInt32(value); });
};

auto get_float4_splat(float value) -> IRValue {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantVector::get(
      {llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{value}),
       llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{value}),
       llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{value}),
       llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{value})}
    );
  });
};

auto extend_to_vec4(pvalue value) {
  return make_irvalue([=](context ctx) {
    auto ty = value->getType();
    if (ty->isVectorTy()) {
      auto vecTy = llvm::cast<llvm::FixedVectorType>(ty);
      if (vecTy->getNumElements() == 4)
        return ctx.builder.CreateShuffleVector(value, {0, 1, 2, 3});
      if (vecTy->getNumElements() == 3)
        return ctx.builder.CreateShuffleVector(value, {0, 1, 2, 2});
      if (vecTy->getNumElements() == 2)
        return ctx.builder.CreateShuffleVector(value, {0, 1, 1, 1});
      if (vecTy->getNumElements() == 1)
        return ctx.builder.CreateShuffleVector(value, {0, 0, 0, 0});
      assert(0 && "?");
    } else {
      return ctx.builder.CreateVectorSplat(4, value);
    }
  });
}

auto extract_element(uint32_t index) {
  return [=](pvalue vec) {
    return make_irvalue([=](context ctx) -> llvm::Value * {
      if (auto constant_vec = llvm::dyn_cast<llvm::ConstantVector>(vec)) {
        return constant_vec->getAggregateElement(index);
      };
      if (auto data_vec = llvm::dyn_cast<llvm::ConstantDataVector>(vec)) {
        return data_vec->getAggregateElement(index);
      };
      return ctx.builder.CreateExtractElement(vec, index);
    });
  };
}

auto swizzle(Swizzle swizzle) {
  return [=](pvalue vec) {
    return make_irvalue([=](context ctx) -> llvm::Value * {
      if (auto constant_vec = llvm::dyn_cast<llvm::ConstantVector>(vec)) {
        return llvm::ConstantVector::get(
          {constant_vec->getAggregateElement(swizzle.x),
           constant_vec->getAggregateElement(swizzle.y),
           constant_vec->getAggregateElement(swizzle.z),
           constant_vec->getAggregateElement(swizzle.w)}
        );
      };
      if (auto data_vec = llvm::dyn_cast<llvm::ConstantDataVector>(vec)) {
        if (data_vec->getElementType() == ctx.types._int) {
          return llvm::ConstantDataVector::get(
            ctx.llvm,
            llvm::ArrayRef<uint32_t>{
              (uint32_t)data_vec->getElementAsInteger(swizzle.x),
              (uint32_t)data_vec->getElementAsInteger(swizzle.y),
              (uint32_t)data_vec->getElementAsInteger(swizzle.z),
              (uint32_t)data_vec->getElementAsInteger(swizzle.w)
            }
          );
        }
        if (data_vec->getElementType() == ctx.types._float) {
          return llvm::ConstantDataVector::get(
            ctx.llvm,
            llvm::ArrayRef<float>{
              data_vec->getElementAsFloat(swizzle.x),
              data_vec->getElementAsFloat(swizzle.y),
              data_vec->getElementAsFloat(swizzle.z),
              data_vec->getElementAsFloat(swizzle.w)
            }
          );
        }
      };
      // FIXME: should guarantee vec is of 4 elements
      if (auto is_vec = llvm::dyn_cast<llvm::VectorType>(vec->getType())) {
        return ctx.builder.CreateShuffleVector(
          vec, (std::array<int, 4>)swizzle
        );
      }
      return ctx.builder.CreateVectorSplat(4, vec); // effective
    });
  };
}

auto to_desired_type_from_int_vec4(pvalue vec4, llvm::Type *desired, uint32_t mask) {
  return make_irvalue([=](context ctx) {
    assert(vec4->getType() == ctx.types._int4);
    std::function<pvalue(pvalue, llvm::Type *)> convert =
      [&ctx, &convert, mask](pvalue vec4, llvm::Type *desired) {
        auto masked = [mask](int i) {
          return (mask & (1 << i)) ? i : llvm::UndefMaskElem;
        };
        if (desired == ctx.types._int4)
          return ctx.builder.CreateShuffleVector(
            vec4, {masked(0), masked(1), masked(2), masked(3)}
          );
        if (desired == ctx.types._float4)
          return ctx.builder.CreateShuffleVector(
            ctx.builder.CreateBitCast(vec4, ctx.types._float4),
            {masked(0), masked(1), masked(2), masked(3)}
          );
        // FIXME: 3d/2d vector implementations are unused and probably wrong!
        if (desired == ctx.types._int3)
          return ctx.builder.CreateShuffleVector(
            vec4, {masked(0), masked(1), masked(2)}
          );
        if (desired == ctx.types._float3)
          return ctx.builder.CreateShuffleVector(
            convert(vec4, ctx.types._float4), {masked(0), masked(1), masked(2)}
          );
        if (desired == ctx.types._int2)
          return ctx.builder.CreateShuffleVector(vec4, {masked(0), masked(1)});
        if (desired == ctx.types._float2)
          return ctx.builder.CreateShuffleVector(
            convert(vec4, ctx.types._float4), {masked(0), masked(1)}
          );
        if (desired == ctx.types._int)
          return ctx.builder.CreateExtractElement(vec4, (uint64_t)__builtin_ctz(mask));
        if (desired == ctx.types._float)
          return ctx.builder.CreateExtractElement(
            ctx.builder.CreateBitCast(vec4, ctx.types._float4), (uint64_t)__builtin_ctz(mask)
          );
        assert(0 && "unhandled vec4");
      };
    return convert(vec4, desired);
  });
};

bool is_constant_zero(pvalue v) {
  if (llvm::isa<llvm::Constant>(v)) {
    return llvm::cast<llvm::Constant>(v)->isZeroValue();
  }
  return false;
}

auto load_from_array_at(llvm::Value *array, pvalue index) -> IRValue {
  return make_irvalue([=](context ctx) {
    auto array_ty = llvm::cast<llvm::ArrayType>( // force line break
      llvm::cast<llvm::PointerType>(array->getType())
        ->getNonOpaquePointerElementType()
    );
    auto ptr = ctx.builder.CreateGEP(
      array_ty, array, {ctx.builder.getInt32(0), index}, "",
      llvm::isa<llvm::ConstantInt>(index)
    );
    return ctx.builder.CreateLoad(array_ty->getElementType(), ptr);
  });
};

IRValue
load_from_vec4_array_masked(llvm::Value *array, pvalue index, uint32_t mask) {
  if (mask == 0b1111) {
    return load_from_array_at(array, index);
  }
  return make_irvalue([=](context ctx) {
    auto array_type = llvm::cast<llvm::PointerType>(array->getType())->getNonOpaquePointerElementType();
    auto vec4_type = llvm::cast<llvm::ArrayType>(array_type)->getArrayElementType();
    auto ele_type = llvm::cast<llvm::VectorType>(vec4_type)->getElementType();
    pvalue value = llvm::ConstantAggregateZero::get(vec4_type);
    for (unsigned i = 0; i < 4; i++) {
      if ((mask & (1 << i)) == 0)
        continue;
      auto component_ptr =
          ctx.builder.CreateGEP(array_type, array, {ctx.builder.getInt32(0), index, ctx.builder.getInt32(i)});
      value = ctx.builder.CreateInsertElement(value, ctx.builder.CreateLoad(ele_type, component_ptr), uint64_t(i));
    }
    return value;
  });
};

auto store_to_array_at(
  llvm::Value *array, pvalue index, pvalue vec4_type_matched
) -> IREffect {
  return make_effect([=](context ctx) {
    auto ptr = ctx.builder.CreateInBoundsGEP(
      llvm::cast<llvm::PointerType>(array->getType())
        ->getNonOpaquePointerElementType(),
      array, {ctx.builder.getInt32(0), index}
    );
    ctx.builder.CreateStore(vec4_type_matched, ptr);
    return std::monostate();
  });
};

IREffect store_at_vec4_array_masked(
  llvm::Value *array, pvalue index, pvalue maybe_vec4, uint32_t mask
) {
  return extend_to_vec4(maybe_vec4) >>= [=](pvalue vec4) {
    if (mask == 0b1111) {
      return store_to_array_at(array, index, vec4);
    }
    return make_effect([=](context ctx) {
      for (unsigned i = 0; i < 4; i++) {
        if ((mask & (1 << i)) == 0)
          continue;
        auto component_ptr =  ctx.builder.CreateGEP(
          llvm::cast<llvm::PointerType>(array->getType())
            ->getNonOpaquePointerElementType(),
          array, {ctx.builder.getInt32(0), index, ctx.builder.getInt32(i)}
        );
        ctx.builder.CreateStore(
          ctx.builder.CreateExtractElement(vec4, i), component_ptr
        );
      }
      return std::monostate();
    });
  };
};

auto store_at_vec_array_masked(
  llvm::Value *array, pvalue index, pvalue maybe_vec4, uint32_t mask
) -> IREffect {
  auto array_ty = llvm::cast<llvm::ArrayType>( // force line break
    llvm::cast<llvm::PointerType>(array->getType())
      ->getNonOpaquePointerElementType()
  );
  auto components =
    cast<llvm::FixedVectorType>(array_ty->getElementType())->getNumElements();
  return make_effect([=](context ctx) {
    for (unsigned i = 0; i < components; i++) {
      if ((mask & (1 << i)) == 0)
        continue;
      auto component_ptr = ctx.builder.CreateGEP(
        array_ty,
        array, {ctx.builder.getInt32(0), index, ctx.builder.getInt32(i)}
      );
      ctx.builder.CreateStore(
        ctx.builder.CreateExtractElement(maybe_vec4, i), component_ptr
      );
    }
    return std::monostate();
  });
};

IREffect init_input_reg(
  uint32_t with_fnarg_at, uint32_t to_reg, uint32_t mask,
  bool fix_w_component
) {
  // no it doesn't work like this
  // regular input can be masked like .zw
  // assert((mask & 1) && "todo: handle input register sharing correctly");
  // FIXME: it's buggy as hell
  return make_effect_bind([=](context ctx) {
    pvalue arg = ctx.function->getArg(with_fnarg_at);
    auto const_index =
      llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, to_reg, false});
    if (arg->getType()->getScalarType()->isIntegerTy()) {
      if (arg->getType()->getScalarType() == ctx.types._bool) {
        // front_facing
        return store_at_vec4_array_masked(
          ctx.resource.input.ptr_int4, const_index,
          ctx.builder.CreateSExt(arg, ctx.types._int), mask
        );
      }
      if (arg->getType()->getScalarType() != ctx.types._int) {
        return throwUnsupported("TODO: handle non 32 bit integer");
      }
      return store_at_vec4_array_masked(
        ctx.resource.input.ptr_int4, const_index, arg, mask
      );
    } else if (arg->getType()->getScalarType()->isFloatTy()) {
      if (fix_w_component && isa<llvm::FixedVectorType>(arg->getType()) &&
          cast<llvm::FixedVectorType>(arg->getType())->getNumElements() == 4) {
        auto w_component = ctx.builder.CreateExtractElement(arg, 3);
        auto rcp_w = ctx.builder.CreateFDiv(
          llvm::ConstantFP::get(ctx.types._float, 1), w_component
        );
        arg = ctx.builder.CreateInsertElement(arg, rcp_w, 3);
      }
      return store_at_vec4_array_masked(
        ctx.resource.input.ptr_float4, const_index, arg, mask
      );
    } else {
      return throwUnsupported(
        "TODO: unhandled input type? might be interpolant..."
      );
    }
  });
};

IREffect init_input_reg_with_interpolation(
  uint32_t with_fnarg_at, uint32_t to_reg, uint32_t mask,
  air::Interpolation interpolation, uint32_t sampleidx_at
) {
  return make_effect_bind([=](context ctx) -> IREffect {
    pvalue interpolant = ctx.function->getArg(with_fnarg_at);
    auto const_index = llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, to_reg, false});
    pvalue interpolated_val = nullptr;
    switch(interpolation) {
    case air::Interpolation::center_perspective:
      interpolated_val = ctx.air.CreateInterpolateAtCenter(interpolant, true);
      break;
    case air::Interpolation::center_no_perspective:
      interpolated_val = ctx.air.CreateInterpolateAtCenter(interpolant, false);
      break;
    case air::Interpolation::centroid_perspective:
      interpolated_val = ctx.air.CreateInterpolateAtCentroid(interpolant, true);
      break;
    case air::Interpolation::centroid_no_perspective:
      interpolated_val = ctx.air.CreateInterpolateAtCentroid(interpolant, false);
      break;
    case air::Interpolation::sample_perspective:
      assert(~sampleidx_at);
      interpolated_val = ctx.air.CreateInterpolateAtSample(interpolant, ctx.function->getArg(sampleidx_at), true);
      break;
    case air::Interpolation::sample_no_perspective:
      assert(~sampleidx_at);
      interpolated_val = ctx.air.CreateInterpolateAtSample(interpolant, ctx.function->getArg(sampleidx_at), false);
      break;
    case air::Interpolation::flat:
      assert(0 && "unhandled interpolant mode");
      break;
    }
    co_return co_yield store_at_vec4_array_masked(
        ctx.resource.input.ptr_float4, const_index, interpolated_val, mask
    );
  });
}

std::function<IRValue(pvalue)>
pop_output_reg(uint32_t from_reg, uint32_t mask, uint32_t to_element) {
  return [=](pvalue ret) {
    return make_irvalue_bind([=](context ctx) {
      auto const_index =
        llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, from_reg, false});
      return load_from_array_at(
               ctx.resource.output.ptr_int4, const_index
             ) >>= [=, &ctx](auto ivec4) {
        auto desired_type =
          ctx.function->getReturnType()->getStructElementType(to_element);
        return to_desired_type_from_int_vec4(ivec4, desired_type, mask) |
               [=, &ctx](auto value) {
                 return ctx.builder.CreateInsertValue(ret, value, {to_element});
               };
      };
    });
  };
}

std::function<IRValue(pvalue)> pop_output_reg_fix_unorm(uint32_t from_reg, uint32_t mask, uint32_t to_element) {
  return [=](pvalue ret) {
    return make_irvalue_bind([=](context ctx) -> IRValue {
      auto const_index = llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, from_reg, false});
      auto fvec4 = co_yield load_from_array_at(ctx.resource.output.ptr_float4, const_index);
      auto fixed = ctx.builder.CreateFSub(fvec4, co_yield get_float4_splat(1.0f / 127500.0f) /* magic delta ?! */);
      co_return ctx.builder.CreateInsertValue(ret, fixed, {to_element});
    });
  };
}

IREffect init_tess_factor_patch_constant(uint32_t to_reg, uint32_t mask, uint32_t factor_index, uint32_t factor_count) {
  return make_effect_bind([=](context ctx) -> IREffect {
    auto src_ptr = ctx.builder.CreateGEP(
      ctx.types._half, ctx.resource.tess_factor_buffer,
      {ctx.builder.CreateAdd(ctx.builder.CreateMul(
         ctx.resource.instanced_patch_id, ctx.builder.getInt32(factor_count)
       ),
       ctx.builder.getInt32(factor_index))}
    );
    auto src_val = ctx.builder.CreateLoad(ctx.types._half, src_ptr);
    auto to_float = ctx.air.CreateConvertToFloat(src_val);
    auto array = ctx.resource.patch_constant_output.ptr_float4;
    auto array_ty = llvm::cast<llvm::ArrayType>( // force line break
      llvm::cast<llvm::PointerType>(array->getType())
        ->getNonOpaquePointerElementType()
    );
    auto component_ptr = ctx.builder.CreateGEP(
      array_ty, array,
      {ctx.builder.getInt32(0), ctx.builder.getInt32(to_reg),
       ctx.builder.getInt32(__builtin_ctz(mask))}
    );
    ctx.builder.CreateStore(to_float, component_ptr);
    co_return {};
  });
}

std::function<IRValue(pvalue)> pop_output_tess_factor(
  uint32_t from_reg, uint32_t mask, uint32_t factor_index, uint32_t factor_count
) {
  return [=](pvalue ret) -> IRValue {
    auto ctx = co_yield get_context();
    auto array = ctx.resource.patch_constant_output.ptr_float4;
    auto array_ty = llvm::cast<llvm::ArrayType>( // force line break
      llvm::cast<llvm::PointerType>(array->getType())
        ->getNonOpaquePointerElementType()
    );
    auto component_ptr = ctx.builder.CreateGEP(
      array_ty, array,
      {ctx.builder.getInt32(0), ctx.builder.getInt32(from_reg),
       ctx.builder.getInt32(__builtin_ctz(mask))}
    );
    auto to_half = ctx.air.CreateConvertToHalf(ctx.builder.CreateLoad(ctx.types._float, component_ptr));
    auto dst_ptr = ctx.builder.CreateGEP(
      ctx.types._half, ctx.resource.tess_factor_buffer,
      {ctx.builder.CreateAdd(ctx.builder.CreateMul(
         ctx.resource.instanced_patch_id, ctx.builder.getInt32(factor_count)
       ),
       ctx.builder.getInt32(factor_index))}
    );
    ctx.builder.CreateStore(to_half, dst_ptr);

    co_return nullptr;
  };
}

IREffect
pop_mesh_output_render_taget_array_index(uint32_t from_reg, uint32_t mask, pvalue primitive_id) {
  auto ctx = co_yield get_context();
  auto result = co_yield to_desired_type_from_int_vec4(
      co_yield load_from_array_at(ctx.resource.output.ptr_int4, ctx.builder.getInt32(from_reg)), ctx.types._int, mask
  );
  ctx.air.CreateSetMeshRenderTargetArrayIndex(primitive_id, result);
  co_return {};
}

IREffect
pop_mesh_output_viewport_array_index(uint32_t from_reg, uint32_t mask, pvalue primitive_id) {
  auto ctx = co_yield get_context();
  auto result = co_yield to_desired_type_from_int_vec4(
      co_yield load_from_array_at(ctx.resource.output.ptr_int4, ctx.builder.getInt32(from_reg)), ctx.types._int, mask
  );
  ctx.air.CreateSetMeshViewportArrayIndex(primitive_id, result);
  co_return {};
}

IREffect
pop_mesh_output_position(uint32_t from_reg, uint32_t mask, pvalue vertex_id) {
  auto ctx = co_yield get_context();
  auto result = co_yield load_from_array_at(ctx.resource.output.ptr_float4, ctx.builder.getInt32(from_reg));
  ctx.air.CreateSetMeshPosition(vertex_id, result);
  co_return {};
}

IREffect
pop_mesh_output_vertex_data(
    uint32_t from_reg, uint32_t mask, uint32_t idx, pvalue vertex_id, air::MSLScalerOrVectorType desired_type
) {
  auto ctx = co_yield get_context();
  auto result = co_yield to_desired_type_from_int_vec4(
      co_yield load_from_vec4_array_masked(ctx.resource.output.ptr_int4, ctx.builder.getInt32(from_reg), mask),
      air::get_llvm_type(desired_type, ctx.llvm), mask
  );
  ctx.air.CreateSetMeshVertexData(vertex_id, idx, result);
  co_return {};
}

IREffect pull_vertex_input(
  air::FunctionSignatureBuilder &func_signature, uint32_t to_reg, uint32_t mask,
  SM50_IA_INPUT_ELEMENT element_info, uint32_t slot_mask
) {
  auto vbuf_table = func_signature.DefineInput(air::ArgumentBindingBuffer{
    .buffer_size = {},
    .location_index = 16,
    .array_size = 0,
    .memory_access = air::MemoryAccess::read,
    .address_space = air::AddressSpace::constant,
    .type = air::msl_uint,
    .arg_name = "vertex_buffers",
    .raster_order_group = {},
  });
  return make_effect_bind([=](context ctx) -> IREffect {
    auto &types = ctx.types;
    auto &builder = ctx.builder;
    pvalue index;                   // uint
    if (element_info.step_function) // per_instance
    {
      if (element_info.step_rate) {
        index = builder.CreateAdd(
          ctx.resource.base_instance_id,
          builder.CreateUDiv(
            ctx.resource.instance_id,
            builder.getInt32(element_info.step_rate)
          )
        );
      } else {
        // 0 takes special meaning, that the instance data should never be
        // stepped at all
        index = ctx.resource.base_instance_id;
      }
    } else {
      index = ctx.resource.vertex_id_with_base;
    }
    if (!ctx.resource.vertex_buffer_table) {
      ctx.resource.vertex_buffer_table = ctx.builder.CreateBitCast(
        ctx.function->getArg(vbuf_table),
        ctx.types._dxmt_vertex_buffer_entry->getPointerTo((uint32_t
        )air::AddressSpace::constant)
      );
    }
    unsigned int shift = 32u - element_info.slot;
    unsigned int vertex_buffer_entry_index =
      element_info.slot ? __builtin_popcount((slot_mask << shift) >> shift) : 0;
    auto vertex_buffer_table = ctx.resource.vertex_buffer_table;
    auto vertex_buffer_entry = builder.CreateLoad(
      types._dxmt_vertex_buffer_entry,
      builder.CreateConstGEP1_32(
        types._dxmt_vertex_buffer_entry, vertex_buffer_table,
        vertex_buffer_entry_index
      )
    );
    auto base_addr = builder.CreateExtractValue(vertex_buffer_entry, {0});
    auto stride = builder.CreateExtractValue(vertex_buffer_entry, {1});
    auto byte_offset = builder.CreateAdd(
      builder.CreateMul(stride, index),
      builder.getInt32(element_info.aligned_byte_offset)
    );
    auto vec4 = co_yield air::pull_vec4_from_addr(
      (air::MTLAttributeFormat)element_info.format, base_addr, byte_offset
    );
    if (vec4->getType() == types._float4) {
      co_yield store_at_vec4_array_masked(
        ctx.resource.input.ptr_float4, builder.getInt32(element_info.reg), vec4,
        mask
      );
    } else {
      assert(vec4->getType() == types._int4);
      co_yield store_at_vec4_array_masked(
        ctx.resource.input.ptr_int4, builder.getInt32(element_info.reg), vec4,
        mask
      );
    }
    co_return {};
  });
};

auto extend_to_int2(pvalue value) -> IRValue {
  return make_irvalue([=](context ctx) {
    return ctx.builder.CreateInsertElement(llvm::ConstantAggregateZero::get(ctx.types._int2), value, uint64_t(0));
  });
};

template <bool ReadFloat>
IRValue load_src_op(SrcOperand src, uint32_t mask = 0b1111) {
  auto ctx = co_yield get_context();
  dxbc::Converter dxbc(ctx.air, ctx, ctx.resource);
  co_return dxbc.LoadOperand(src, mask);
};

template <bool StoreFloat>
IREffect store_dst_op(DstOperand dst, IRValue &&value) {
  return make_effect_bind(
    [=, value = std::move(value)](auto ctx) mutable -> IREffect {
      dxbc::Converter dxbc(ctx.air, ctx, ctx.resource);
      if (dxbc.IsNull(dst))
        co_return {};
      dxbc.StoreOperandVec4(dst, co_yield std::move(value));
      co_return {};
    }
  );
};

template <bool StoreFloat>
IREffect store_dst_op_masked(DstOperand dst, IRValue &&value) {
  return make_effect_bind(
    [=, value = std::move(value)](auto ctx) mutable -> IREffect {
      dxbc::Converter dxbc(ctx.air, ctx, ctx.resource);
      if (dxbc.IsNull(dst))
        co_return {};
      dxbc.StoreOperand(dst, co_yield std::move(value));
      co_return {};
    }
  );
};

auto load_condition(SrcOperand src, bool non_zero_test) {
  return load_src_op<false>(src) >>= [=](pvalue condition_src) {
    return make_irvalue([=](context ctx) {
      auto element =
        ctx.builder.CreateExtractElement(condition_src, (uint64_t)0);
      if (non_zero_test) {
        return ctx.builder.CreateICmpNE(element, ctx.builder.getInt32(0));
      } else {
        return ctx.builder.CreateICmpEQ(element, ctx.builder.getInt32(0));
      }
    });
  };
};

auto metadata_get_texture_buffer_offset(pvalue metadata) -> IRValue {
  auto ctx = co_yield get_context();
  co_return ctx.builder.CreateTrunc(metadata, ctx.types._int);
};

auto metadata_get_texture_buffer_length(pvalue metadata) -> IRValue {
  auto ctx = co_yield get_context();
  co_return ctx.builder.CreateTrunc(
    ctx.builder.CreateLShr(metadata, 32uLL), ctx.types._int
  );
};

auto metadata_get_raw_buffer_length(pvalue metadata) -> IRValue {
  // unintentional but they have the same representation
  return metadata_get_texture_buffer_offset(metadata);
};

auto metadata_get_texture_array_length(pvalue metadata) -> IRValue {
  // unintentional but they have the same representation
  return metadata_get_texture_buffer_length(metadata);
};

auto convert_array_index(pvalue float_num, pvalue array_length) -> IRValue {
  using namespace llvm::air;
  auto ctx = co_yield get_context();
  auto rounded = ctx.air.CreateFPUnOp(AIRBuilder::rint, float_num);
  auto integer = ctx.air.CreateConvertToSigned(rounded);
  auto positive_integer = ctx.air.CreateIntBinOp(AIRBuilder::max, integer, ctx.builder.getInt32(0), true);
  auto max_index = ctx.builder.CreateSub(array_length, ctx.builder.getInt32(1));
  co_return ctx.air.CreateIntBinOp(AIRBuilder::min, positive_integer, max_index, true);
};

/**
it might be a constant, device or threadgroup buffer of `uint*`
*/
auto load_from_uint_bufptr(
  pvalue uint_buf_ptr, uint32_t read_components, pvalue oob = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  pvalue vec = llvm::UndefValue::get(ctx.types._int4);
  for (uint32_t i = 0; i < read_components; i++) {
    auto ptr = ctx.builder.CreateGEP(
      ctx.types._int, uint_buf_ptr, {ctx.builder.getInt32(i)}
    );
    bool is_volatile =
      cast<llvm::PointerType>(ptr->getType())->getAddressSpace() != 2;
    vec = ctx.builder.CreateInsertElement(
      vec,
      oob ? ctx.builder.CreateSelect(
              oob, ctx.builder.getInt32(0),
              ctx.builder.CreateLoad(ctx.types._int, ptr, is_volatile)
            )
          : ctx.builder.CreateLoad(ctx.types._int, ptr, is_volatile),
      i
    );
  }
  co_return vec;
};

auto store_to_uint_bufptr(
  pvalue uint_buf_ptr, uint32_t written_components, pvalue ivec4_to_write
) -> IREffect {
  auto ctx = co_yield get_context();
  for (uint32_t i = 0; i < written_components; i++) {
    auto ptr = ctx.builder.CreateGEP(
      ctx.types._int, uint_buf_ptr, {ctx.builder.getInt32(i)}
    );
    ctx.builder.CreateStore(
      ctx.builder.CreateExtractElement(ivec4_to_write, i), ptr,
      cast<llvm::PointerType>(ptr->getType())->getAddressSpace() != 2
    );
  }
  co_return {};
};

auto mask_to_linear_components_num(uint32_t mask) {
  switch (mask) {
  case 0b1:
    return 1;
  case 0b11:
    return 2;
  case 0b111:
    return 3;
  case 0b1111:
    return 4;
  default:
    llvm::outs() << mask << '\n';
    assert(0 && "invalid write mask");
    return 0;
  }
}

auto store_tgsm(
  llvm::GlobalValue *g, pvalue offset_in_4bytes, uint32_t written_components,
  pvalue ivec4_to_write
) -> IREffect {
  auto ctx = co_yield get_context();
  for (uint32_t i = 0; i < written_components; i++) {
    auto ptr = ctx.builder.CreateInBoundsGEP(
      llvm::cast<llvm::PointerType>(g->getType())
        ->getNonOpaquePointerElementType(),
      g,
      {ctx.builder.getInt32(0),
       ctx.builder.CreateAdd(offset_in_4bytes, ctx.builder.getInt32(i))}
    );
    ctx.builder.CreateStore(co_yield extract_element(i)(ivec4_to_write), ptr);
  }
  co_return {};
};

auto read_uint_buf_addr(AtomicOperandTGSM tgsm, pvalue index = 0) -> IRValue {
  auto ctx = co_yield get_context();
  assert(ctx.resource.tgsm_map.contains(tgsm.id));
  auto gv = ctx.resource.tgsm_map[tgsm.id].second;
  llvm::Constant *Zero =
    llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm), 0);
  if (index) {
    co_return ctx.builder.CreateGEP(gv->getValueType(), gv, {Zero, index});
  }
  co_return llvm::ConstantExpr::getInBoundsGetElementPtr(
    gv->getValueType(), gv, (llvm::ArrayRef<llvm::Constant *>){Zero, Zero}
  );
}

// the same as above
auto read_uint_buf_addr(SrcOperandTGSM tgsm, pvalue index = 0) -> IRValue {
  auto ctx = co_yield get_context();
  assert(ctx.resource.tgsm_map.contains(tgsm.id));
  auto gv = ctx.resource.tgsm_map[tgsm.id].second;
  llvm::Constant *Zero =
    llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx.llvm), 0);
  if (index) {
    co_return ctx.builder.CreateGEP(gv->getValueType(), gv, {Zero, index});
  }
  co_return llvm::ConstantExpr::getInBoundsGetElementPtr(
    gv->getValueType(), gv, (llvm::ArrayRef<llvm::Constant *>){Zero, Zero}
  );
}

auto read_uint_buf_addr(
  AtomicDstOperandUAV uav, pvalue index = 0, pvalue *oob = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  assert(ctx.resource.uav_buf_range_map.contains(uav.range_id));
  auto &[_, handle, metadata] = ctx.resource.uav_buf_range_map[uav.range_id];
  if (index) {
    auto bound =
      co_yield metadata_get_raw_buffer_length(co_yield metadata(nullptr));
    auto _bound = ctx.builder.CreateLShr(bound, ctx.builder.getInt32(2));

    auto is_oob = ctx.builder.CreateICmpUGE(index, _bound);
    if (oob) {
      *oob = is_oob;
    };
    auto final_index = ctx.builder.CreateSelect(is_oob, _bound, index);
    co_return ctx.builder.CreateGEP(
      ctx.types._int, co_yield handle(nullptr), {final_index}
    );
  }
  // can't be even oob at 0?
  co_return co_yield handle(nullptr);
}

// the same as above
auto read_uint_buf_addr(
  SrcOperandUAV uav, pvalue index = 0, pvalue *oob = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  assert(ctx.resource.uav_buf_range_map.contains(uav.range_id));
  auto &[_, handle, metadata] = ctx.resource.uav_buf_range_map[uav.range_id];
  if (index) {
    auto bound =
      co_yield metadata_get_raw_buffer_length(co_yield metadata(nullptr));
    auto _bound = ctx.builder.CreateLShr(bound, ctx.builder.getInt32(2));

    auto is_oob = ctx.builder.CreateICmpUGE(index, _bound);
    if (oob) {
      *oob = is_oob;
    };
    auto final_index =
      ctx.builder.CreateSelect(is_oob, ctx.builder.getInt32(0), index);
    co_return ctx.builder.CreateGEP(
      ctx.types._int, co_yield handle(nullptr), {final_index}
    );
  }
  // can't be even oob at 0?
  co_return co_yield handle(nullptr);
}

auto read_uint_buf_addr(
  SrcOperandResource srv, pvalue index = 0, pvalue *oob = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  assert(ctx.resource.srv_buf_range_map.contains(srv.range_id));
  auto &[_, handle, metadata] = ctx.resource.srv_buf_range_map[srv.range_id];
  if (index) {
    auto bound =
      co_yield metadata_get_raw_buffer_length(co_yield metadata(nullptr));
    auto _bound = ctx.builder.CreateLShr(bound, ctx.builder.getInt32(2));

    auto is_oob = ctx.builder.CreateICmpUGE(index, _bound);
    if (oob) {
      *oob = is_oob;
    };
    auto final_index =
      ctx.builder.CreateSelect(is_oob, ctx.builder.getInt32(0), index);
    co_return ctx.builder.CreateGEP(
      ctx.types._int, co_yield handle(nullptr), {final_index}
    );
  }
  co_return co_yield handle(nullptr);
}

auto read_atomic_index(AtomicOperandTGSM tgsm, pvalue vec4) -> IRValue {
  auto ctx = co_yield get_context();
  assert(ctx.resource.tgsm_map.contains(tgsm.id));
  auto stride = ctx.resource.tgsm_map[tgsm.id].first;
  auto &builder = ctx.builder;
  co_return builder.CreateLShr(
    stride > 0 ? builder.CreateAdd(
                   builder.CreateMul(
                     builder.getInt32(stride), co_yield extract_element(0)(vec4)
                   ),
                   co_yield extract_element(1)(vec4)
                 )
               : co_yield extract_element(0)(vec4),
    2
  );
};

auto read_atomic_index(AtomicDstOperandUAV uav, pvalue addr_vec4) -> IRValue {
  auto ctx = co_yield get_context();
  assert(ctx.resource.uav_buf_range_map.contains(uav.range_id));
  auto &[stride, _, __] = ctx.resource.uav_buf_range_map[uav.range_id];
  auto &builder = ctx.builder;
  co_return builder.CreateLShr(
    stride > 0
      ? builder.CreateAdd(
          builder.CreateMul(
            builder.getInt32(stride), co_yield extract_element(0)(addr_vec4)
          ),
          co_yield extract_element(1)(addr_vec4)
        )
      : co_yield extract_element(0)(addr_vec4),
    2
  );
};

auto calc_uint_ptr_index(
  uint32_t stride, SrcOperand structure_idx, SrcOperand byte_offset
) -> IRValue {
  auto ctx = co_yield get_context();
  auto &builder = ctx.builder;
  co_return builder.CreateLShr(
    builder.CreateAdd(
      builder.CreateMul(
        builder.getInt32(stride),
        co_yield load_src_op<false>(structure_idx) >>= extract_element(0)
      ),
      co_yield load_src_op<false>(byte_offset) >>= extract_element(0)
    ),
    2
  );
};

auto calc_uint_ptr_index(uint32_t zero, uint32_t unused, SrcOperand byte_offset)
  -> IRValue {
  assert(zero == 0);
  auto ctx = co_yield get_context();
  auto &builder = ctx.builder;
  co_return builder.CreateLShr(
    co_yield load_src_op<false>(byte_offset) >>= extract_element(0), 2
  );
};

llvm::Expected<llvm::BasicBlock *> convert_basicblocks(
  std::shared_ptr<BasicBlock> entry, context &ctx, llvm::BasicBlock *return_bb
) {
  auto &context = ctx.llvm;
  auto &builder = ctx.builder;
  auto function = ctx.function;
  std::unordered_map<BasicBlock *, llvm::BasicBlock *> visited;
  std::vector<std::pair<BasicBlock *, llvm::BasicBlock *>> visit_order;

  std::stack<BasicBlock *> block_to_visit;
  block_to_visit.push(entry.get());

  while (!block_to_visit.empty()) {
    auto current = block_to_visit.top();
    block_to_visit.pop();
    if (visited.contains(current)) continue;
    auto inserted = visited.insert({current, llvm::BasicBlock::Create(context, current->debug_name, function)});
    visit_order.push_back(*inserted.first);
    std::visit(
      patterns{
        [](BasicBlockUndefined) {
          return;
        },
        [&](BasicBlockReturn ret) {
          return;
        },
        [&](BasicBlockUnconditionalBranch uncond) {
          block_to_visit.push(uncond.target.get());
        },
        [&](BasicBlockConditionalBranch cond) {
          block_to_visit.push(cond.true_branch.get());
          block_to_visit.push(cond.false_branch.get());
        },
        [&](BasicBlockSwitch swc)  {
          block_to_visit.push(swc.case_default.get());
          for (auto &[val, case_bb] : swc.cases) {
            block_to_visit.push(case_bb.get());
          }
        },
        [&](BasicBlockInstanceBarrier instance)  {
          block_to_visit.push(instance.active.get());
          block_to_visit.push(instance.sync.get());
        },
        [&](BasicBlockHullShaderWriteOutput hull_end)  {
          block_to_visit.push(hull_end.epilogue.get());
        },
      },
      current->target
    );
  }

  auto bb_pop = builder.GetInsertBlock();
  for (auto &[current, bb] : visit_order) {
    IREffect effect([](auto) { return std::monostate(); });
    {
      current->instructions.for_each(
        patterns{
          [](InstMaskedSumOfAbsDiff) { assert(0 && "unhandled msad"); },

          [&effect](InstBufferInfo bufinfo) {
            auto count = std::visit(
              patterns{
                [](SrcOperandResource srv) -> IRValue {
                  auto ctx = co_yield get_context();
                  if (ctx.resource.srv_buf_range_map.contains(srv.range_id)) {
                    auto &[stride, _, metadata] =
                      ctx.resource.srv_buf_range_map[srv.range_id];
                    auto byte_width = co_yield metadata_get_raw_buffer_length(
                      co_yield metadata(nullptr)
                    );
                    if (stride) {
                      co_return ctx.builder.CreateUDiv(
                        byte_width, ctx.builder.getInt32(stride)
                      );
                    }
                    co_return byte_width;
                  } else {
                    auto &[tex, srv_h, metadata] =
                      ctx.resource.srv_range_map[srv.range_id];
                    co_return co_yield metadata_get_texture_buffer_length(
                      co_yield metadata(nullptr)
                    );
                  }
                },
                [](SrcOperandUAV uav) -> IRValue {
                  auto ctx = co_yield get_context();
                  if (ctx.resource.uav_buf_range_map.contains(uav.range_id)) {
                    auto &[stride, _, metadata] =
                      ctx.resource.uav_buf_range_map[uav.range_id];
                    auto byte_width = co_yield metadata_get_raw_buffer_length(
                      co_yield metadata(nullptr)
                    );
                    if (stride) {
                      co_return ctx.builder.CreateUDiv(
                        byte_width, ctx.builder.getInt32(stride)
                      );
                    }
                    co_return byte_width;
                  } else {
                    auto &[tex, uav_h, metadata] =
                      ctx.resource.uav_range_map[uav.range_id];
                    co_return co_yield metadata_get_texture_buffer_length(
                      co_yield metadata(nullptr)
                    );
                  }
                }
              },
              bufinfo.src
            );
            effect << store_dst_op<false>(bufinfo.dst, std::move(count));
          },
          [&](InstSampleInfo sample_info) {
            effect << make_effect_bind([=](struct context ctx) -> IREffect {
              using namespace dxmt::air;
              pvalue sample_count = nullptr;
              dxbc::Swizzle swiz = swizzle_identity;
              if (sample_info.src.has_value()) {
                auto &[tex, res_h, _] = ctx.resource.srv_range_map[sample_info.src->range_id];
                sample_count = co_yield call_get_texture_info(
                  tex, co_yield res_h(nullptr), TextureInfoType::num_samples, ctx.builder.getInt32(0)
                );
                swiz = sample_info.src->read_swizzle;
              } else {
                sample_count = ctx.air.CreateGetNumSamples();
              }

              if (sample_info.uint_result) {
                pvalue vec_ret = ctx.builder.CreateInsertElement(
                  llvm::ConstantAggregateZero::get(ctx.types._int4), sample_count, (uint64_t)0
                );
                co_return co_yield store_dst_op<false>(sample_info.dst, swizzle(swiz)(vec_ret));
              } else {
                pvalue vec_ret = ctx.builder.CreateInsertElement(
                  llvm::ConstantAggregateZero::get(ctx.types._float4),
                  ctx.air.CreateConvertToFloat(sample_count),
                  (uint64_t)0
                );
                co_return co_yield store_dst_op<true>(sample_info.dst, swizzle(swiz)(vec_ret));
              }
            });
          },
          [&](InstSamplePos sample_pos) {
            // FIXME: stub
            effect << store_dst_op<true>(sample_pos.dst, make_irvalue([](struct context s) {
              return llvm::ConstantAggregateZero::get(s.types._float4);
            }));
          },
          [&](InstInterpolateCentroid eval_centroid) {
            assert(0 && "unhandled eval_centroid"); // remove this line if it's verified to work
            effect << store_dst_op<true>(
              eval_centroid.dst,
              make_irvalue_bind([=](struct context ctx) -> IRValue {
                auto desc = ctx.resource.interpolant_map.at(eval_centroid.regid);
                co_return ctx.air.CreateInterpolateAtCentroid(
                  co_yield desc.interpolant(nullptr), desc.perspective
                );
              }) >>= swizzle(eval_centroid.read_swizzle)
            );
          },
          [&](InstInterpolateSample eval_sample_index) {
            effect << store_dst_op<true>(
              eval_sample_index.dst,
              make_irvalue_bind([=](struct context ctx) -> IRValue {
                auto desc = ctx.resource.interpolant_map.at(eval_sample_index.regid);
                co_return ctx.air.CreateInterpolateAtSample(
                  co_yield desc.interpolant(nullptr),
                  co_yield load_src_op<false>(eval_sample_index.sample_index) >>= extract_element(0),
                  desc.perspective
                );
              }) >>= swizzle(eval_sample_index.read_swizzle)
            );
          },
          [&](InstInterpolateOffset eval_snapped) {
            assert(0 && "unhandled eval_snapped"); // remove this line if it's verified to work
            effect << store_dst_op<true>(
              eval_snapped.dst,
              make_irvalue_bind([=](struct context ctx) -> IRValue {
                auto desc = ctx.resource.interpolant_map.at(eval_snapped.regid);
                auto offset = co_yield load_src_op<false>(eval_snapped.offset);
                // truncated = (offset.xy + 8) & 0b1111
                auto truncated = ctx.builder.CreateAnd(
                  ctx.builder.CreateAdd(
                    ctx.builder.CreateShuffleVector(offset, {0, 1}),
                    co_yield get_int2(8, 8)
                  ),
                  co_yield get_int2(0b1111, 0b1111)
                );
                auto offset_f = ctx.builder.CreateFMul(
                  ctx.air.CreateConvertToFloat(truncated),
                  co_yield get_float2(1.0f / 16.0f, 1.0f / 16.0f)
                );
                co_return ctx.air.CreateInterpolateAtOffset(
                  co_yield desc.interpolant(nullptr), offset_f, desc.perspective
                );
              }) >>= swizzle(eval_snapped.read_swizzle)
            );
          },
          [&](InstEmit) { effect << ctx.resource.call_emit(); },
          [&](InstCut) { effect << ctx.resource.call_cut(); },
          [&](auto Op) {
            effect << make_effect([=](struct context ctx) { 
              dxbc::Converter dxbc(ctx.air, ctx, ctx.resource);
              dxbc(Op);
              return std::monostate{};
            });
          }
      });
    }
    builder.SetInsertPoint(bb);
    if (auto err = effect.build(ctx).takeError()) {
      return err;
    }
    if (auto err = std::visit(
          patterns{
            [](BasicBlockUndefined) -> llvm::Error {
              return llvm::make_error<UnsupportedFeature>(
                "unexpected undefined basicblock"
              );
            },
            [&](BasicBlockUnconditionalBranch uncond) -> llvm::Error {
              auto target_bb = visited[uncond.target.get()];
              builder.CreateBr(target_bb);
              return llvm::Error::success();
            },
            [&](BasicBlockConditionalBranch cond) -> llvm::Error {
              auto target_true_bb = visited[cond.true_branch.get()];
              auto target_false_bb = visited[cond.false_branch.get()];
              auto test =
                load_condition(cond.cond.operand, cond.cond.test_nonzero)
                  .build(ctx);
              if (auto err = test.takeError()) {
                return err;
              }
              builder.CreateCondBr(test.get(), target_true_bb, target_false_bb);
              return llvm::Error::success();
            },
            [&](BasicBlockSwitch swc) -> llvm::Error {
              auto value =
                (load_src_op<false>(swc.value) >>= extract_element(0))
                  .build(ctx);
              if (auto err = value.takeError()) {
                return err;
              }
              auto switch_inst = builder.CreateSwitch(
                value.get(), visited[swc.case_default.get()], swc.cases.size()
              );
              for (auto &[val, case_bb] : swc.cases) {
                switch_inst->addCase(
                  llvm::ConstantInt::get(context, llvm::APInt(32, val)),
                  visited[case_bb.get()]
                );
              }
              return llvm::Error::success();
            },
            [&](BasicBlockReturn ret) -> llvm::Error {
              // we have done here!
              // unconditional jump to return bb
              builder.CreateBr(return_bb);
              return llvm::Error::success();
            },
            [&](BasicBlockInstanceBarrier instance) -> llvm::Error {
              auto target_true_bb = visited[instance.active.get()];
              auto target_false_bb = visited[instance.sync.get()];
              builder.CreateCondBr(
                ctx.builder.CreateICmp(
                  llvm::CmpInst::ICMP_ULT,
                  ctx.resource.thread_id_in_patch,
                  ctx.builder.getInt32(instance.instance_count)
                ),
                target_true_bb, target_false_bb
              );
              return llvm::Error::success();
            },
            [&](BasicBlockHullShaderWriteOutput hull_end) -> llvm::Error {
              auto active = llvm::BasicBlock::Create(
                context, "write_control_point", function
              );
              auto sync = llvm::BasicBlock::Create(
                context, "write_control_point_end", function
              );

              builder.CreateCondBr(
                ctx.builder.CreateICmp(
                  llvm::CmpInst::ICMP_ULT,
                  ctx.resource.thread_id_in_patch,
                  ctx.builder.getInt32(hull_end.instance_count)
                ),
                active, sync
              );
              builder.SetInsertPoint(active);
              auto dst_ptr = builder.CreateGEP(
                ctx.types._int4, ctx.resource.control_point_buffer,
                {builder.CreateMul(
                  ctx.resource.instanced_patch_id, builder.getInt32(
                                           ctx.resource.output_element_count *
                                           hull_end.instance_count
                                         )
                )}
              );
              for (unsigned i = 0; i < ctx.resource.output_element_count; i++) {
                auto dst_ptr_offset = builder.CreateGEP(
                  ctx.types._int4, dst_ptr,
                  {builder.CreateAdd(
                    builder.CreateMul(
                      ctx.resource.thread_id_in_patch,
                      builder.getInt32(ctx.resource.output_element_count)
                    ),
                    builder.getInt32(i)
                  )}
                );
                auto src_ptr_offset = builder.CreateGEP(
                  llvm::ArrayType::get(ctx.types._int4, ctx.resource.output_element_count * hull_end.instance_count),
                  ctx.resource.output.ptr_int4,
                  {builder.getInt32(0),
                   builder.CreateAdd(
                     builder.CreateMul(
                       ctx.resource.thread_id_in_patch,
                       builder.getInt32(ctx.resource.output_element_count)
                     ),
                     builder.getInt32(i)
                   )}
                );
                builder.CreateStore(
                  builder.CreateLoad(ctx.types._int4, src_ptr_offset),
                  dst_ptr_offset
                );
              }
              builder.CreateBr(sync);
              builder.SetInsertPoint(sync);
              ctx.air.CreateBarrier(llvm::air::MemFlags::Threadgroup);

              auto target_bb = visited[hull_end.epilogue.get()];
              builder.CreateBr(target_bb);
              return llvm::Error::success();
            },
          },
          current->target
        )) {
      return err;
    };
  };
  assert(visited.count(entry.get()) == 1);
  builder.SetInsertPoint(bb_pop);
  return visited[entry.get()];
}

} // namespace dxmt::dxbc

template <>
struct environment_cast<::dxmt::dxbc::context, ::dxmt::air::AIRBuilderContext> {
  ::dxmt::air::AIRBuilderContext cast(const ::dxmt::dxbc::context &src) {
    return {src.llvm, src.module, src.builder, src.types, src.air};
  };
};