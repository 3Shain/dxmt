#pragma once
#include "air_signature.hpp"
#include "air_type.hpp"
#include "dxbc_converter.hpp"
#include "ftl.hpp"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/ArrayRef.h"
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
#include <cstdint>
#include <functional>
#include <string>
#include <variant>

// it's suposed to be include by specific file
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
// NOLINTBEGIN(misc-definitions-in-headers)

namespace dxmt::dxbc {

using pvalue = llvm::Value *;
using dxbc::Swizzle;
using dxbc::swizzle_identity;

struct context;
using IRValue = ReaderIO<context, pvalue>;
using IREffect = ReaderIO<context, std::monostate>;
using IndexedIRValue = std::function<IRValue(pvalue)>;

struct register_file {
  llvm::Value *ptr_int4 = nullptr;
  llvm::Value *ptr_float4 = nullptr;
};

struct indexable_register_file {
  llvm::Value *ptr_int_vec = nullptr;
  llvm::Value *ptr_float_vec = nullptr;
  uint32_t vec_size = 0;
};

struct io_binding_map {
  llvm::GlobalVariable *icb = nullptr;
  llvm::Value *icb_float = nullptr;
  std::unordered_map<uint32_t, IndexedIRValue> cb_range_map{};
  std::unordered_map<uint32_t, IndexedIRValue> sampler_range_map{};
  std::unordered_map<
    uint32_t, std::tuple<air::MSLTexture, IndexedIRValue, int32_t>>
    srv_range_map{};
  std::unordered_map<
    uint32_t, std::tuple<air::MSLTexture, IndexedIRValue, int32_t>>
    uav_range_map{};
  std::unordered_map<uint32_t, std::pair<uint32_t, llvm::GlobalVariable *>>
    tgsm_map{};

  register_file input{};
  register_file output{};
  register_file temp{};
  std::unordered_map<uint32_t, indexable_register_file> indexable_temp_map{};

  // special registers (input)
  llvm::Value *thread_id_arg = nullptr;
  llvm::Value *thread_group_id_arg = nullptr;
  llvm::Value *thread_id_in_group_arg = nullptr;
  llvm::Value *thread_id_in_group_flat_arg = nullptr;

  // special registers (output)
  llvm::AllocaInst *depth_output_reg = nullptr;
  llvm::AllocaInst *stencil_ref_reg = nullptr;
  llvm::AllocaInst *coverage_mask_reg = nullptr;
};

struct context {
  llvm::IRBuilder<> &builder;
  llvm::LLVMContext &llvm;
  llvm::Module &module;
  llvm::Function *function;
  io_binding_map &resource;
  air::AirType &types; // hmmm
};

ReaderIO<context, context> get_context() {
  return ReaderIO<context, context>([](struct context ctx) { return ctx; });
};

template <typename S> IRValue make_irvalue(S &&fs) {
  return IRValue(std::forward<S>(fs));
}

template <typename S> IRValue make_irvalue_bind(S &&fs) {
  return IRValue([fs = std::forward<S>(fs)](auto ctx) {
    return fs(ctx).build(ctx);
  });
}

template <typename S> IREffect make_effect(S &&fs) {
  return IREffect(std::forward<S>(fs));
}

template <typename S> IREffect make_effect_bind(S &&fs) {
  return IREffect([fs = std::forward<S>(fs)](auto ctx) mutable {
    return fs(ctx).build(ctx);
  });
}

uint32_t get_value_dimension(pvalue maybe_vec) {
  if (maybe_vec->getType()->getTypeID() == llvm::Type::FixedVectorTyID) {
    return llvm::cast<llvm::FixedVectorType>(maybe_vec->getType())
      ->getNumElements();
  } else {
    return 1;
  }
};

std::string type_overload_suffix(
  llvm::Type *type, air::Sign sign = air::Sign::inapplicable
) {
  if (type->isFloatTy()) {
    return ".f32";
  }
  if (type->isIntegerTy()) {
    assert(llvm::cast<llvm::IntegerType>(type)->getBitWidth() == 32);
    if (sign == air::Sign::inapplicable)
      return ".i32";
    return sign == air::Sign::with_sign ? ".s.i32" : ".u.i32";
  }
  if (type->getTypeID() == llvm::Type::FixedVectorTyID) {
    auto fixedVecTy = llvm::cast<llvm::FixedVectorType>(type);
    auto elementTy = fixedVecTy->getElementType();
    if (elementTy->isFloatTy()) {
      return ".v" + std::to_string(fixedVecTy->getNumElements()) + "f32";
    } else if (elementTy->isIntegerTy()) {
      assert(llvm::cast<llvm::IntegerType>(elementTy)->getBitWidth() == 32);
      if (sign == air::Sign::inapplicable)
        return ".v" + std::to_string(fixedVecTy->getNumElements()) + "i32";
      return sign == air::Sign::with_sign
               ? ".s.v" + std::to_string(fixedVecTy->getNumElements()) + "i32"
               : ".u.v" + std::to_string(fixedVecTy->getNumElements()) + "i32";
    }
  }
  type->dump();
  assert(0 && "unexpected or unhandled type");
};

auto bitcast_float4(pvalue vec4) {
  return make_irvalue([=](context s) {
    if (vec4->getType() == s.types._float4)
      return vec4;
    return s.builder.CreateBitCast(vec4, s.types._float4);
  });
}

auto bitcast_int4(pvalue vec4) {
  return make_irvalue([=](context s) {
    if (vec4->getType() == s.types._int4)
      return vec4;
    return s.builder.CreateBitCast(vec4, s.types._int4);
  });
}

auto truncate_vec(uint8_t &&elements) {
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

auto get_int3(uint32_t value0, uint32_t value1, uint32_t value2) -> IRValue {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantVector::get(
      {llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value0, true}),
       llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value1, true}),
       llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value2, true})}
    );
  });
};

auto get_int4_splat(uint32_t value) -> IRValue {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantVector::get(
      {llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value, true}),
       llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value, true}),
       llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value, true}),
       llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, value, true})}
    );
  });
};

auto get_int(uint32_t value) -> IRValue {
  return make_irvalue([=](context ctx) { return ctx.builder.getInt32(value); });
};

auto get_float(float value) -> IRValue {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{value});
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

auto extract_value(uint32_t index) {
  return [=](pvalue aggregate) {
    return make_irvalue([=](context ctx) {
      return ctx.builder.CreateExtractValue(aggregate, {index});
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
      return ctx.builder.CreateShuffleVector(vec, (std::array<int, 4>)swizzle);
    });
  };
}

auto to_desired_type_from_int_vec4(pvalue vec4, llvm::Type *desired) {
  return make_irvalue([=](context ctx) {
    assert(vec4->getType() == ctx.types._int4);
    std::function<pvalue(pvalue, llvm::Type *)> convert =
      [&ctx, &convert](pvalue vec4, llvm::Type *desired) {
        if (desired == ctx.types._int4)
          return vec4;
        if (desired == ctx.types._float4)
          return ctx.builder.CreateBitCast(vec4, ctx.types._float4);
        if (desired == ctx.types._int3)
          return ctx.builder.CreateShuffleVector(vec4, {0, 1, 2});
        if (desired == ctx.types._float3)
          return ctx.builder.CreateShuffleVector(
            convert(vec4, ctx.types._float4), {0, 1, 2}
          );
        if (desired == ctx.types._int2)
          return ctx.builder.CreateShuffleVector(vec4, {0, 1});
        if (desired == ctx.types._float2)
          return ctx.builder.CreateShuffleVector(
            convert(vec4, ctx.types._float4), {0, 1}
          );
        if (desired == ctx.types._int)
          return ctx.builder.CreateExtractElement(vec4, (uint64_t)0);
        if (desired == ctx.types._float)
          return ctx.builder.CreateExtractElement(
            ctx.builder.CreateBitCast(vec4, ctx.types._float4), (uint64_t)0
          );
        assert(0 && "unhandled vec4");
      };
    return convert(vec4, desired);
  });
};

auto get_function_arg(uint32_t arg_index) {
  return make_irvalue([=](context ctx) {
    return ctx.function->getArg(arg_index);
  });
};

auto get_item_in_argbuf_binding_table(uint32_t argbuf_index, uint32_t index) {
  return make_irvalue([=](context ctx) {
    auto argbuf = ctx.function->getArg(argbuf_index);
    auto argbuf_struct_type = llvm::cast<llvm::StructType>(
      llvm::cast<llvm::PointerType>(argbuf->getType())
        ->getNonOpaquePointerElementType()
    );
    return ctx.builder.CreateLoad(
      argbuf_struct_type->getElementType(index),
      ctx.builder.CreateStructGEP(
        llvm::cast<llvm::PointerType>(argbuf->getType())
          ->getNonOpaquePointerElementType(),
        argbuf, index
      )
    );
  });
};

auto get_shuffle_mask(uint32_t writeMask) {
  // original value: 0 1 2 3
  // new value: 4 5 6 7
  // if writeMask at specific component is not set, use original value
  std::array<int, 4> mask = {0, 1, 2, 3}; // identity selection
  if (writeMask & 1) {
    mask[0] = 4;
  }
  if (writeMask & 2) {
    mask[1] = 5;
  }
  if (writeMask & 4) {
    mask[2] = 6;
  }
  if (writeMask & 8) {
    mask[3] = 7;
  }
  return mask;
}

/* it should return sign extended uint4 (all bits 0 or all bits 1) */
auto cmp_integer(llvm::CmpInst::Predicate cmp, pvalue a, pvalue b) {
  return make_irvalue([=](context ctx) {
    return ctx.builder.CreateSExt(
      ctx.builder.CreateICmp(cmp, a, b), ctx.types._int4
    );
  });
};

/* it should return sign extended uint4 (all bits 0 or all bits 1) */
auto cmp_float(llvm::CmpInst::Predicate cmp, pvalue a, pvalue b) {
  return make_irvalue([=](context ctx) {
    return ctx.builder.CreateSExt(
      ctx.builder.CreateFCmp(cmp, a, b), ctx.types._int4
    );
  });
};

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

auto store_at_vec4_array_masked(
  llvm::Value *array, pvalue index, pvalue maybe_vec4, uint32_t mask
) -> IREffect {
  return extend_to_vec4(maybe_vec4) >>= [=](pvalue vec4) {
    return make_effect_bind([=](context ctx) {
      if (mask == 0b1111) {
        return store_to_array_at(array, index, vec4);
      }
      return load_from_array_at(array, index) >>= [=](auto current) {
        assert(current->getType() == vec4->getType());
        auto new_value = ctx.builder.CreateShuffleVector(
          current, vec4, get_shuffle_mask(mask)
        );
        return store_to_array_at(array, index, new_value);
      };
    });
  };
};

auto store_at_vec_array_masked(
  llvm::Value *array, pvalue index,  pvalue maybe_vec4,
  uint32_t mask
) -> IREffect {
  auto array_ty = llvm::cast<llvm::ArrayType>( // force line break
      llvm::cast<llvm::PointerType>(array->getType())
        ->getNonOpaquePointerElementType()
    );
    auto components = cast<llvm::FixedVectorType>(array_ty->getElementType())->getNumElements();
  return extend_to_vec4(maybe_vec4) >>= [=](pvalue vec4) {
    return make_effect_bind([=](context ctx) {
      return (load_from_array_at(array, index) >>= extend_to_vec4) >>=
             [=](auto current) {
               auto cx = mask & 1 ? 4 : 0;
               auto cy = mask & 2 ? 5 : 1;
               auto cz = mask & 4 ? 6 : 2;
               auto cw = mask & 8 ? 7 : 3;
               switch (components) {
               case 1: {
                 auto new_value = ctx.builder.CreateShuffleVector(
                   current, vec4, {cx}
                 );
                 return store_to_array_at(array, index, new_value);
               };
               case 2: {
                 auto new_value = ctx.builder.CreateShuffleVector(
                   current, vec4,  {cx, cy}
                 );
                 return store_to_array_at(array, index, new_value);
               };
               case 3: {
                 auto new_value = ctx.builder.CreateShuffleVector(
                   current, vec4,  {cx, cy, cz}
                 );
                 return store_to_array_at(array, index, new_value);
               };
               case 4: {
                 auto new_value = ctx.builder.CreateShuffleVector(
                   current, vec4,  {cx, cy, cz, cw}
                 );
                 return store_to_array_at(array, index, new_value);
               };
               default: {
                 assert(0 && "UNREACHABLE");
                 break;
               }
               }
             };
    });
  };
};

auto init_input_reg(uint32_t with_fnarg_at, uint32_t to_reg, uint32_t mask)
  -> IREffect {
  // no it doesn't work like this
  // regular input can be masked like .zw
  // assert((mask & 1) && "todo: handle input register sharing correctly");
  // FIXME: it's buggy as hell
  return make_effect_bind([=](context ctx) {
    auto arg = ctx.function->getArg(with_fnarg_at);
    auto const_index =
      llvm::ConstantInt::get(ctx.llvm, llvm::APInt{32, to_reg, false});
    if (arg->getType()->getScalarType()->isIntegerTy()) {
      assert(
        arg->getType()->getScalarType() == ctx.types._int &&
        "TODO: handle non 32 bit integer"
      );
      return store_at_vec4_array_masked(
        ctx.resource.input.ptr_int4, const_index, arg, mask
      );
    } else if (arg->getType()->getScalarType()->isFloatTy()) {
      return store_at_vec4_array_masked(
        ctx.resource.input.ptr_float4, const_index, arg, mask
      );
    } else {
      assert(0 && "TODO: unhandled input type? might be interpolant...");
    }
  });
};

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
        return to_desired_type_from_int_vec4(ivec4, desired_type) |
               [=, &ctx](auto value) {
                 return ctx.builder.CreateInsertValue(ret, value, {to_element});
               };
      };
    });
  };
}

auto call_dot_product(uint32_t dimension, pvalue a, pvalue b) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    auto &types = ctx.types;
    auto att = AttributeList::get(
      context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)}}
    );
    auto operand_type = dimension == 4   ? types._float4
                        : dimension == 3 ? types._float3
                                         : types._float2;
    auto fn = (module.getOrInsertFunction(
      "air.dot" + type_overload_suffix(operand_type),
      llvm::FunctionType::get(
        types._float, {operand_type, operand_type}, false
      ),
      att
    ));
    return ctx.builder.CreateCall(fn, {a, b});
  });
};

auto call_float_mad(pvalue a, pvalue b, pvalue c) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    assert(a->getType() == b->getType());
    assert(a->getType() == c->getType());
    auto att = AttributeList::get(
      context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)}}
    );
    auto operand_type = a->getType();
    auto fn = (module.getOrInsertFunction(
      "air.fma" + type_overload_suffix(operand_type),
      llvm::FunctionType::get(
        operand_type, {operand_type, operand_type, operand_type}, false
      ),
      att
    ));
    return ctx.builder.CreateCall(fn, {a, b, c});
  });
};

auto call_integer_unary_op(std::string op, pvalue a) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    assert(a->getType()->getScalarType()->isIntegerTy());
    auto att = AttributeList::get(
      context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)}}
    );
    auto operand_type = a->getType();
    auto fn = (module.getOrInsertFunction(
      "air." + op + type_overload_suffix(operand_type, air::Sign::inapplicable),
      llvm::FunctionType::get(operand_type, {operand_type}, false), att
    ));
    return ctx.builder.CreateCall(fn, {a});
  });
};

auto call_integer_binop(
  std::string op, pvalue a, pvalue b, bool is_signed = false
) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    assert(a->getType()->getScalarType()->isIntegerTy());
    assert(b->getType()->getScalarType()->isIntegerTy());
    assert(a->getType() == b->getType());
    auto att = AttributeList::get(
      context, {std::make_pair<unsigned int, Attribute>(
                  ~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)
                ),
                std::make_pair<unsigned int, Attribute>(
                  ~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)
                ),
                std::make_pair<unsigned int, Attribute>(
                  ~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)
                )}
    );
    auto operand_type = a->getType();
    auto fn = (module.getOrInsertFunction(
      "air." + op +
        type_overload_suffix(
          operand_type, is_signed ? air::Sign::with_sign : air::Sign::no_sign
        ),
      llvm::FunctionType::get(
        operand_type, {operand_type, operand_type}, false
      ),
      att
    ));
    return ctx.builder.CreateCall(fn, {a, b});
  });
};

auto call_float_unary_op(std::string op, pvalue a) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    assert(a->getType()->getScalarType()->isFloatTy());
    auto att = AttributeList::get(
      context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)}}
    );
    auto operand_type = a->getType();
    auto fn = (module.getOrInsertFunction(
      "air." + op + type_overload_suffix(operand_type),
      llvm::FunctionType::get(operand_type, {operand_type}, false), att
    ));
    return ctx.builder.CreateCall(fn, {a});
  });
};

auto call_float_binop(std::string op, pvalue a, pvalue b) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    assert(a->getType()->getScalarType()->isFloatTy());
    assert(b->getType()->getScalarType()->isFloatTy());
    assert(a->getType() == b->getType());
    auto att = AttributeList::get(
      context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)}}
    );
    auto operand_type = a->getType();
    auto fn = (module.getOrInsertFunction(
      "air." + op + type_overload_suffix(operand_type),
      llvm::FunctionType::get(
        operand_type, {operand_type, operand_type}, false
      ),
      att
    ));
    return ctx.builder.CreateCall(fn, {a, b});
  });
};

auto pure(pvalue value) {
  return make_irvalue([=](auto) { return value; });
}

auto saturate(bool sat) {
  return [sat](pvalue floaty) {
    if (sat) {
      return call_float_unary_op("saturate", floaty);
    } else {
      return pure(floaty);
    }
  };
}

struct TextureOperationInfo {
  // it seems to satisty: offset vector length > 1
  bool sample_unknown_bit = false;
  bool is_depth = false;
  bool is_cube = false;
  bool is_array = false;
  bool is_ms = false;
  bool is_1d_or_1d_array = false;
  bool is_texture_buffer = false;
  air::Sign sign = air::Sign::inapplicable;
  std::string air_symbol_suffix = {};
  llvm::Type *coord_type = nullptr;
  llvm::Type *address_type = nullptr;
  llvm::Type *offset_type = nullptr;
  llvm::Type *sample_type = nullptr;
  llvm::Type *gather_type = nullptr;
  llvm::Type *read_type = nullptr;
  llvm::Type *write_type = nullptr;
  llvm::Type *gradient_type = nullptr;
};

auto get_operation_info(air::MSLTexture texture)
  -> ReaderIO<context, TextureOperationInfo> {
  using namespace dxmt::air;
  auto ctx = co_yield get_context();
  auto &types = ctx.types;
  auto sign = std::visit(
    patterns{
      [](MSLUint) { return air::Sign::no_sign; },
      [](MSLInt) { return air::Sign::with_sign; },
      [](auto) { return air::Sign::inapplicable; }
    },
    texture.component_type
  );
  TextureOperationInfo ret;
  ret.sign = sign;
  ret.sample_type =
    types.to_vec4_type(air::get_llvm_type(texture.component_type, ctx.llvm));
  ret.gather_type = ret.sample_type; // handle exceptions separately
  ret.read_type = ret.sample_type;   // handle exceptions separately
  ret.write_type = ret.sample_type;  // handle exceptions separately
  switch (texture.resource_kind) {
  case TextureKind::texture_1d: {
    ret.coord_type = types._float;
    ret.air_symbol_suffix = "texture_1d";
    ret.offset_type = types._int;
    ret.is_1d_or_1d_array = true;
    ret.gather_type = nullptr;
    ret.address_type = types._int;
    break;
  };
  case TextureKind::texture_2d: {
    ret.coord_type = types._float2;
    ret.sample_unknown_bit = true;
    ret.air_symbol_suffix = "texture_2d";
    ret.offset_type = types._int2;
    ret.gradient_type = types._float2;
    ret.address_type = types._int2;
    break;
  }
  case TextureKind::texture_1d_array: {
    ret.coord_type = types._float;
    ret.air_symbol_suffix = "texture_1d_array";
    ret.is_array = true;
    ret.offset_type = types._int;
    ret.is_1d_or_1d_array = true;
    ret.gather_type = nullptr;
    ret.address_type = types._int;
    break;
  }
  case TextureKind::texture_2d_array: {
    ret.coord_type = types._float2;
    ret.sample_unknown_bit = true;
    ret.air_symbol_suffix = "texture_2d_array";
    ret.is_array = true;
    ret.offset_type = types._int2;
    ret.gradient_type = types._float2;
    ret.address_type = types._int2;
    break;
  }
  case TextureKind::texture_3d: {
    ret.coord_type = types._float3;
    ret.sample_unknown_bit = true;
    ret.air_symbol_suffix = "texture_3d";
    ret.offset_type = types._int3;
    ret.gradient_type = types._float3;
    ret.gather_type = nullptr;
    ret.address_type = types._int3;
    break;
  }
  case TextureKind::texture_cube: {
    ret.coord_type = types._float3;
    ret.air_symbol_suffix = "texture_cube";
    ret.gradient_type = types._float3;
    ret.is_cube = true;
    ret.address_type = types._int2;
    break;
  }
  case TextureKind::texture_cube_array: {
    ret.coord_type = types._float3;
    ret.air_symbol_suffix = "texture_cube_array";
    ret.is_array = true;
    ret.gradient_type = types._float3;
    ret.is_cube = true;
    ret.address_type = types._int2;
    break;
  }
  case TextureKind::texture_buffer: {
    ret.sample_type = nullptr;
    ret.air_symbol_suffix = "texture_buffer_1d";
    ret.gather_type = nullptr;
    ret.address_type = types._int;
    ret.is_texture_buffer = true;
    break;
  }
  case TextureKind::depth_2d: {
    ret.coord_type = types._float2;
    ret.air_symbol_suffix = "depth_2d";
    ret.is_depth = true;
    ret.offset_type = types._int2;
    ret.sample_type = types._float;
    ret.read_type = types._float;
    ret.write_type = nullptr;
    ret.gradient_type = types._float2;
    ret.gather_type = types._float4;
    break;
  }
  case TextureKind::depth_2d_array: {
    ret.coord_type = types._float2;
    ret.air_symbol_suffix = "depth_2d_array";
    ret.is_array = true;
    ret.is_depth = true;
    ret.sample_unknown_bit = true;
    ret.offset_type = types._int2;
    ret.sample_type = types._float;
    ret.read_type = types._float;
    ret.write_type = nullptr;
    ret.gradient_type = types._float2;
    ret.gather_type = types._float4;
    break;
  }
  case TextureKind::depth_cube: {
    ret.coord_type = types._float3;
    ret.air_symbol_suffix = "depth_cube";
    ret.is_depth = true;
    ret.sample_unknown_bit = true;
    ret.sample_type = types._float;
    ret.read_type = types._float;
    ret.write_type = nullptr;
    ret.gradient_type = types._float3;
    ret.gather_type = types._float4;
    ret.is_cube = true;
    break;
  }
  case TextureKind::depth_cube_array: {
    ret.coord_type = types._float3;
    ret.air_symbol_suffix = "depth_cube_array";
    ret.is_array = true;
    ret.is_depth = true;
    ret.sample_type = types._float;
    ret.read_type = types._float;
    ret.write_type = nullptr;
    ret.gradient_type = types._float3;
    ret.gather_type = types._float4;
    ret.is_cube = true;
    break;
  }
  case TextureKind::texture_2d_ms: {
    ret.sample_type = nullptr;
    ret.air_symbol_suffix = "texture_2d_ms";
    ret.is_ms = true;
    ret.gather_type = nullptr;
    break;
  }
  case TextureKind::texture_2d_ms_array: {
    ret.sample_type = nullptr;
    ret.air_symbol_suffix = "texture_2d_ms_array";
    ret.is_array = true;
    ret.is_ms = true;
    ret.gather_type = nullptr;
    break;
  }
  case TextureKind::depth_2d_ms: {
    ret.air_symbol_suffix = "depth_2d_ms";
    ret.is_depth = true;
    ret.sample_type = nullptr;
    ret.read_type = types._float;
    ret.write_type = nullptr;
    ret.is_ms = true;
    ret.gather_type = nullptr;
    break;
  }
  case TextureKind::depth_2d_ms_array: {
    ret.air_symbol_suffix = "depth_2d_ms_array";
    ret.is_array = true;
    ret.is_depth = true;
    ret.sample_type = nullptr;
    ret.read_type = types._float;
    ret.write_type = nullptr;
    ret.is_ms = true;
    ret.gather_type = nullptr;
    break;
  }
  }
  co_return ret;
}

IRValue call_sample(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler, pvalue coord,
  pvalue array_index, pvalue offset = nullptr, pvalue bias = nullptr,
  pvalue min_lod_clamp = nullptr, pvalue lod_level = nullptr
) {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {2U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {2U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::Convergent)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  assert(!op_info.is_ms);
  auto fn_name = "air.sample_" + op_info.air_symbol_suffix +
                 type_overload_suffix(op_info.sample_type, op_info.sign);
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  args_type.push_back(types._sampler->getPointerTo(2));
  args_value.push_back(sampler);

  if (op_info.is_depth) {
    args_type.push_back(types._int);
    args_value.push_back(ctx.builder.getInt32(1));
  }

  args_type.push_back(op_info.coord_type);
  args_value.push_back(coord);

  if (op_info.is_array) {
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }

  if (op_info.offset_type != nullptr) {
    args_type.push_back(types._bool);
    args_value.push_back(ctx.builder.getInt1(op_info.sample_unknown_bit));
    args_type.push_back(op_info.offset_type);
    args_value.push_back(
      offset != nullptr ? offset
                        : ConstantAggregateZero::get(op_info.offset_type)
    );
  }

  /* parameters */
  args_type.push_back(types._bool);
  args_type.push_back(types._float);
  args_type.push_back(types._float);

  if (lod_level != nullptr) {
    args_value.push_back(ctx.builder.getInt1(true));
    args_value.push_back(lod_level);
    args_value.push_back(ConstantFP::get(context, APFloat{0.0f}));
  } else {
    args_value.push_back(ctx.builder.getInt1(false));
    args_value.push_back(
      bias != nullptr ? bias : ConstantFP::get(context, APFloat{0.0f})
    );
    args_value.push_back(
      min_lod_clamp != nullptr ? min_lod_clamp
                               : ConstantFP::get(context, APFloat{0.0f})
    );
  }

  /* access: always 0 = sample */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(0));

  auto return_type =
    StructType::get(context, {op_info.sample_type, ctx.types._byte});
  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(return_type, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

auto call_sample_grad(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler_handle,
  pvalue coord, pvalue array_index, pvalue dpdx, pvalue dpdy,
  pvalue minlod = nullptr, pvalue offset = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {2U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {2U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  assert(!op_info.is_ms);
  assert(!op_info.is_1d_or_1d_array);
  auto fn_name = "air.sample_" + op_info.air_symbol_suffix + "_grad" +
                 type_overload_suffix(op_info.sample_type, op_info.sign);
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  args_type.push_back(types._sampler->getPointerTo(2));
  args_value.push_back(sampler_handle);

  if (op_info.is_depth) {
    args_type.push_back(types._int);
    args_value.push_back(ctx.builder.getInt32(1));
  }

  args_type.push_back(op_info.coord_type);
  args_value.push_back(coord);

  if (op_info.is_array) {
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }
  assert(op_info.gradient_type);
  args_type.push_back(op_info.gradient_type);
  args_value.push_back(dpdx);
  args_type.push_back(op_info.gradient_type);
  args_value.push_back(dpdy);

  args_type.push_back(types._float);
  args_value.push_back(minlod ? minlod : ConstantFP::get(types._float, 0));

  if (op_info.offset_type != nullptr) {
    args_type.push_back(types._bool);
    args_value.push_back(ctx.builder.getInt1(op_info.sample_unknown_bit)
    ); // = 1
    args_type.push_back(op_info.offset_type);
    args_value.push_back(
      offset != nullptr ? offset
                        : ConstantAggregateZero::get(op_info.offset_type)
    );
  }

  /* access: always 0 = sample */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(0));

  auto return_type =
    StructType::get(context, {op_info.sample_type, ctx.types._byte});
  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(return_type, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

auto call_sample_compare(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler_handle,
  pvalue coord, pvalue array_index, pvalue reference = nullptr,
  pvalue offset = nullptr, pvalue bias = nullptr,
  pvalue min_lod_clamp = nullptr, pvalue lod_level = nullptr
) -> IRValue {

  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {2U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {2U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::Convergent)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  assert(op_info.is_depth);
  auto fn_name =
    "air.sample_compare_" + op_info.air_symbol_suffix +
    type_overload_suffix(op_info.sample_type, op_info.sign) /* = .f32*/;
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  args_type.push_back(types._sampler->getPointerTo(2));
  args_value.push_back(sampler_handle);

  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(1));

  args_type.push_back(op_info.coord_type);
  args_value.push_back(coord);

  if (op_info.is_array) {
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }

  /* reference */
  args_type.push_back(types._float);
  args_value.push_back(
    reference ? reference : ConstantFP::get(types._float, 0)
  );

  if (op_info.offset_type != nullptr) {
    args_type.push_back(types._bool);
    args_value.push_back(ctx.builder.getInt1(op_info.sample_unknown_bit)
    ); // = 1
    args_type.push_back(op_info.offset_type);
    args_value.push_back(
      offset != nullptr ? offset
                        : ConstantAggregateZero::get(op_info.offset_type)
    );
  }

  /* parameters */
  args_type.push_back(types._bool);
  args_type.push_back(types._float);
  args_type.push_back(types._float);

  if (lod_level != nullptr) {
    args_value.push_back(ctx.builder.getInt1(true));
    args_value.push_back(lod_level);
    args_value.push_back(ConstantFP::get(context, APFloat{0.0f}));
  } else {
    args_value.push_back(ctx.builder.getInt1(false));
    args_value.push_back(
      bias != nullptr ? bias : ConstantFP::get(context, APFloat{0.0f})
    );
    args_value.push_back(
      min_lod_clamp != nullptr ? min_lod_clamp
                               : ConstantFP::get(context, APFloat{0.0f})
    );
  }

  /* access: always 0 = sample */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(0));

  auto return_type =
    StructType::get(context, {op_info.sample_type, ctx.types._byte});
  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(return_type, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

auto call_gather(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler_handle,
  pvalue coord, pvalue array_index, pvalue offset = nullptr,
  pvalue component = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {2U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {2U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  assert(op_info.gather_type);
  auto fn_name = "air.gather_" + op_info.air_symbol_suffix +
                 type_overload_suffix(op_info.sample_type, op_info.sign);
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  args_type.push_back(types._sampler->getPointerTo(2));
  args_value.push_back(sampler_handle);

  if (op_info.is_depth) {
    args_type.push_back(types._int);
    args_value.push_back(ctx.builder.getInt32(1));
  }

  args_type.push_back(op_info.coord_type);
  args_value.push_back(coord);

  if (op_info.is_array) {
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }

  if (op_info.offset_type != nullptr) {
    args_type.push_back(types._bool);
    args_value.push_back(ctx.builder.getInt1(op_info.sample_unknown_bit)
    ); // = 1
    args_type.push_back(op_info.offset_type);
    args_value.push_back(
      offset != nullptr ? offset
                        : ConstantAggregateZero::get(op_info.offset_type)
    );
  }

  /* component */
  if (!op_info.is_depth) {
    args_type.push_back(types._int);
    args_value.push_back(component ? component : ctx.builder.getInt32(0));
  }

  /* access: always 0 = sample */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(0));

  auto return_type =
    StructType::get(context, {op_info.gather_type, ctx.types._byte});
  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(return_type, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

auto call_gather_compare(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler_handle,
  pvalue coord, pvalue array_index, pvalue reference = nullptr,
  pvalue offset = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {2U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {2U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  assert(op_info.is_depth);
  auto fn_name = "air.gather_compare_" + op_info.air_symbol_suffix + ".f32";
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  args_type.push_back(types._sampler->getPointerTo(2));
  args_value.push_back(sampler_handle);

  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(1));

  args_type.push_back(op_info.coord_type);
  args_value.push_back(coord);

  if (op_info.is_array) {
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }

  // reference

  args_type.push_back(types._float);
  args_value.push_back(
    reference ? reference : ConstantFP::get(types._float, 0)
  );

  if (op_info.offset_type != nullptr) {
    args_type.push_back(types._bool);
    args_value.push_back(ctx.builder.getInt1(op_info.sample_unknown_bit)
    ); // = 1
    args_type.push_back(op_info.offset_type);
    args_value.push_back(
      offset != nullptr ? offset
                        : ConstantAggregateZero::get(op_info.offset_type)
    );
  }

  /* access: always 0 = sample */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(0));

  auto return_type =
    StructType::get(context, {op_info.gather_type, ctx.types._byte});
  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(return_type, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

auto call_read(
  air::MSLTexture texture_type, pvalue handle, pvalue address,
  pvalue offset = nullptr, pvalue cube_face = nullptr,
  pvalue array_index = nullptr, pvalue sample_index = nullptr,
  pvalue lod = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  auto fn_name = "air.read_" + op_info.air_symbol_suffix +
                 type_overload_suffix(op_info.read_type, op_info.sign);
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  if (op_info.is_depth) {
    args_type.push_back(types._int);
    args_value.push_back(ctx.builder.getInt32(1));
  }

  args_type.push_back(op_info.address_type);
  if (Constant *c = offset ? dyn_cast<Constant>(offset) : nullptr) {
    if (c->isZeroValue()) {
      args_value.push_back(address);
    } else {
      args_value.push_back(ctx.builder.CreateAdd(address, offset));
    }
  } else {
    args_value.push_back(address);
  }

  if (op_info.is_cube) {
    assert(cube_face);
    args_type.push_back(types._int);
    args_value.push_back(cube_face);
  }

  if (op_info.is_array) {
    assert(array_index);
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }

  if (op_info.is_ms) {
    assert(sample_index);
    args_type.push_back(types._int);
    args_value.push_back(sample_index);
  }

  if (!op_info.is_ms && texture_type.resource_kind != air::TextureKind::texture_buffer) {
    args_type.push_back(types._int);
    if (op_info.is_1d_or_1d_array) {
      args_value.push_back(ctx.builder.getInt32(0));
    } else {
      args_value.push_back(lod ? lod : ctx.builder.getInt32(0));
    }
  }

  /* access */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32((uint32_t)texture_type.memory_access
  ));

  auto return_type =
    StructType::get(context, {op_info.read_type, ctx.types._byte});
  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(return_type, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

auto call_write(
  air::MSLTexture texture_type, pvalue handle, pvalue address, pvalue cube_face,
  pvalue array_index, pvalue vec4, pvalue lod = nullptr
) -> IRValue {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  assert(!op_info.is_ms);
  assert(!op_info.is_depth);
  auto fn_name = "air.write_" + op_info.air_symbol_suffix +
                 type_overload_suffix(op_info.read_type, op_info.sign);
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  args_type.push_back(op_info.address_type);
  args_value.push_back(address);

  if (op_info.is_cube) {
    args_type.push_back(types._int);
    args_value.push_back(cube_face);
  }

  if (op_info.is_array) {
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }

  args_type.push_back(op_info.write_type);
  args_value.push_back(vec4);

  if (texture_type.resource_kind != air::TextureKind::texture_buffer) {
    args_type.push_back(types._int);
    if (op_info.is_1d_or_1d_array) {
      args_value.push_back(ctx.builder.getInt32(0));
    } else {
      args_value.push_back(lod ? lod : ctx.builder.getInt32(0));
    }
  }

  /* access */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32((uint32_t)texture_type.memory_access
  ));

  auto return_type = Type::getVoidTy(context);
  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(return_type, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

IRValue call_calc_lod(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler, pvalue coord,
  bool is_unclamped
) {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {2U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {2U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::Convergent)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  assert(!op_info.is_ms);
  assert(!op_info.is_1d_or_1d_array);
  auto fn_name = is_unclamped
                   ? "air.calculate_unclamped_lod_"
                   : "air.calculate_clamped_lod_" + op_info.air_symbol_suffix;
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  args_type.push_back(types._sampler->getPointerTo(2));
  args_value.push_back(sampler);

  args_type.push_back(op_info.coord_type);
  args_value.push_back(coord);

  /* access: always 0 = sample */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(0));

  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(types._float, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

enum class TextureInfoType {
  width,
  height,
  depth,
  array_length,
  num_mip_levels,
  num_samples
};

IRValue call_get_texture_info(
  air::MSLTexture texture_type, pvalue handle, TextureInfoType type, pvalue lod
) {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
      {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  if (type == TextureInfoType::array_length) {
    assert(op_info.is_array);
  }
  if (type == TextureInfoType::num_mip_levels) {
    assert(!op_info.is_ms);
  }
  if (type == TextureInfoType::num_samples) {
    assert(op_info.is_ms);
  }
  auto fn_name =
    (type == TextureInfoType::width          ? "air.get_width_"
     : type == TextureInfoType::height       ? "air.get_height_"
     : type == TextureInfoType::depth        ? "air.get_depth_"
     : type == TextureInfoType::array_length ? "air.get_array_size_"
     : type == TextureInfoType::num_samples  ? "air.get_num_samples_"
                                             : "air.get_num_mip_levels_") +
    op_info.air_symbol_suffix;
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  if (type == TextureInfoType::width || type == TextureInfoType::height || type == TextureInfoType::depth) {
    if (!op_info.is_ms && !op_info.is_texture_buffer) {
      args_type.push_back(types._int);
      args_value.push_back(lod);
    }
  }

  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(types._int, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
}

IREffect call_discard_fragment() {
  using namespace llvm;
  auto ctx = co_yield get_context();
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto att = AttributeList::get(
    context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
              {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)}}
  );
  auto fn = (module.getOrInsertFunction(
    "air.discard_fragment",
    llvm::FunctionType::get(Type::getVoidTy(context), {}, false), att
  ));
  ctx.builder.CreateCall(fn, {});
  co_return {};
}

IRValue call_derivative(pvalue fvec4, bool dfdy) {
  using namespace llvm;
  auto ctx = co_yield get_context();
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
              {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)}}
  );
  auto fn = (module.getOrInsertFunction(
    "air." + std::string(dfdy ? "dfdy" : "dfdx") + ".v4f32",
    llvm::FunctionType::get(types._float4, {types._float4}, false), att
  ));
  co_return ctx.builder.CreateCall(fn, {fvec4});
}

enum class mem_flags : uint8_t {
  device = 1,
  threadgroup = 2,
  texture = 4,
};

IREffect call_threadgroup_barrier(mem_flags mem_flag) {
  using namespace llvm;
  auto ctx = co_yield get_context();
  auto &context = ctx.llvm;
  auto &types = ctx.types;
  auto &module = ctx.module;
  auto att = AttributeList::get(
    context, {{~0U, Attribute::get(context, Attribute::AttrKind::Convergent)},
              {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
              {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)}}
  );

  auto fn = (module.getOrInsertFunction(
    "air.wg.barrier",
    llvm::FunctionType::get(
      Type::getVoidTy(context),
      {
        types._int, // mem_flag
        types._int, // scope, should be always 1?
      },
      false
    ),
    att
  ));
  ctx.builder.CreateCall(
    fn, {co_yield get_int((uint32_t)mem_flag), co_yield get_int(1)}
  );
  co_return {};
}

IRValue call_atomic_fetch_explicit(
  pvalue pointer, pvalue operand, std::string op, bool is_signed = false,
  bool device = false
) {
  using namespace llvm;
  auto ctx = co_yield get_context();
  auto &context = ctx.llvm;
  auto &types = ctx.types;
  auto &module = ctx.module;
  auto att = AttributeList::get(
    context, {{1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
              {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
              {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)}}
  );

  assert(operand->getType() == types._int);

  auto fn = (module.getOrInsertFunction(
    "air.atomic." + std::string(device ? "global." : "local.") + op +
      std::string(is_signed ? ".s.i32" : ".u.i32"),
    llvm::FunctionType::get(
      types._int,
      {
        types._int->getPointerTo(device ? 1 : 3),
        types._int, // operand
        types._int, // order 0 relaxed
        types._int, // scope 1 threadgroup 2 device (type: memflag?)
        types._bool // volatile? should be true all the time?
      },
      false
    ),
    att
  ));
  co_return ctx.builder.CreateCall(
    fn, {pointer, operand, co_yield get_int(0),
         co_yield get_int(device ? 2 : 1), ConstantInt::getBool(context, true)}
  );
};

/* are we assume that vec4 can only be integer? */
IRValue call_texture_atomic_fetch_explicit(
  air::MSLTexture texture_type, pvalue handle, std::string op, bool is_signed,
  pvalue address, pvalue array_index, pvalue vec4
) {
  auto ctx = co_yield get_context();
  using namespace llvm;
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto &types = ctx.types;
  auto att = AttributeList::get(
    context,
    {
      {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
      {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
      {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
    }
  );
  auto op_info = co_yield get_operation_info(texture_type);
  assert(!op_info.is_depth);
  assert(!op_info.is_cube);
  assert(!op_info.is_ms);
  auto fn_name =
    "air.atomic_fetch_" + op + "_explicit_" + op_info.air_symbol_suffix +
    type_overload_suffix(
      vec4->getType(), is_signed ? air::Sign::with_sign : air::Sign::no_sign
    );
  std::vector<llvm::Type *> args_type;
  std::vector<pvalue> args_value;

  args_type.push_back(texture_type.get_llvm_type(context));
  args_value.push_back(handle);

  args_type.push_back(op_info.address_type);
  args_value.push_back(address);

  if (op_info.is_array) {
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }

  args_type.push_back(op_info.write_type);
  args_value.push_back(vec4);

  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32(0)); // memory order relaxed

  /* access */
  args_type.push_back(types._int);
  args_value.push_back(ctx.builder.getInt32((uint32_t)texture_type.memory_access
  ));

  auto fn = module.getOrInsertFunction(
    fn_name, llvm::FunctionType::get(op_info.write_type, args_type, false), att
  );
  co_return ctx.builder.CreateCall(fn, args_value);
};

IRValue call_atomic_exchange_explicit(
  pvalue pointer, pvalue operand, bool device = false
) {
  using namespace llvm;
  auto ctx = co_yield get_context();
  auto &context = ctx.llvm;
  auto &types = ctx.types;
  auto &module = ctx.module;
  auto att = AttributeList::get(
    context, {{1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
              {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
              {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)}}
  );

  assert(operand->getType() == types._int);

  auto fn = (module.getOrInsertFunction(
    "air.atomic." + std::string(device ? "global." : "local.") + "xchg.i32",
    llvm::FunctionType::get(
      types._int,
      {
        types._int->getPointerTo(device ? 1 : 3),
        types._int, // desired
        types._int, // order 0 relaxed
        types._int, // scope 1 threadgroup 2 device (type: memflag?)
        types._bool // volatile? should be true all the time?
      },
      false
    ),
    att
  ));
  co_return ctx.builder.CreateCall(
    fn, {pointer, operand, co_yield get_int(0),
         co_yield get_int(device ? 2 : 1), ConstantInt::getBool(context, true)}
  );
}

ReaderIO<context, std::pair<pvalue, pvalue>> call_atomic_cmp_exchange(
  pvalue pointer, pvalue compared, pvalue operand, std::string op,
  bool is_signed = false, bool device = false
) {
  using namespace llvm;
  auto ctx = co_yield get_context();
  auto &context = ctx.llvm;
  auto &types = ctx.types;
  auto &module = ctx.module;
  auto att = AttributeList::get(
    context, {{1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
              {2U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
              {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
              {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)}}
  );

  assert(operand->getType() == types._int);

  auto fn = module.getOrInsertFunction(
    "air.atomic." + std::string(device ? "global." : "local.") +
      "cmpxchg.weak.i32",
    llvm::FunctionType::get(
      types._int,
      {
        types._int->getPointerTo(device ? 1 : 3),
        types._int->getPointerTo(), // expected
        types._int,                 // desired
        types._int,                 // order 0 relaxed
        types._int,                 // order 0 relaxed
        types._int, // scope 1 threadgroup 2 device (type: memflag?)
        types._bool // volatile? should be true all the time?
      },
      false
    ),
    att
  );
  auto alloca = ctx.builder.CreateAlloca(types._int);
  auto ptr = ctx.builder.CreateConstGEP1_64(types._int, alloca, 0);
  ctx.builder.CreateLifetimeStart(alloca);
  ctx.builder.CreateStore(compared, ptr);
  auto call_ret = ctx.builder.CreateCall(
    fn, {pointer, alloca, operand, co_yield get_int(0), co_yield get_int(0),
         co_yield get_int(device ? 2 : 1), ConstantInt::getBool(context, true)}
  );
  auto expected = ctx.builder.CreateLoad(types._int, ptr);
  ctx.builder.CreateLifetimeEnd(alloca);
  co_return {call_ret, expected};
}

// TODO: not good, expose too much detail
IRValue call_convert(
  pvalue src, llvm::Type *src_type, llvm::Type *dst_type,
  std::string src_suffix, std::string dst_suffix
) {
  using namespace llvm;
  auto ctx = co_yield get_context();
  auto &context = ctx.llvm;
  auto &module = ctx.module;
  auto att = AttributeList::get(
    context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
              {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)},
              {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)}}
  );

  auto fn = (module.getOrInsertFunction(
    "air.convert" + dst_suffix + src_suffix,
    llvm::FunctionType::get(dst_type, {src_type}, false), att
  ));
  co_return ctx.builder.CreateCall(fn, {src});
};

auto implicit_float_to_int(pvalue num) -> IRValue {
  auto &types = (co_yield get_context()).types;
  auto rounded = co_yield call_float_unary_op("rint", num);
  co_return co_yield call_convert(
    rounded, types._float, types._int, ".f.f32", ".u.i32"
  );
};

SrcOperandModifier get_modifier(SrcOperand src) {
  return std::visit(
    patterns{
      [](SrcOperandImmediate32) {
        return SrcOperandModifier{.swizzle = swizzle_identity};
      },
      [](SrcOperandAttribute src) { return src._; },
      [](SrcOperandConstantBuffer src) { return src._; },
      [](SrcOperandImmediateConstantBuffer src) { return src._; },
      [](SrcOperandInput src) { return src._; },
      [](SrcOperandTemp src) { return src._; },
      [](SrcOperandIndexableTemp src) { return src._; },
      [](auto s) {
        llvm::outs() << "get_modifier: unhandled src operand type "
                     << decltype(s)::debug_name << "\n";
        assert(0 && "get_modifier: unhandled src operand type");
        return SrcOperandModifier{};
      }
    },
    src
  );
};

IRValue apply_float_src_operand_modifier(
  SrcOperandModifier c, llvm::Value *fvec4, uint32_t mask
) {
  auto ctx = co_yield get_context();
  assert(fvec4->getType() == ctx.types._float4);

  auto ret = fvec4;
  if (c.swizzle != swizzle_identity) {
    ret = ctx.builder.CreateShuffleVector(
      ret, {c.swizzle.x, c.swizzle.y, c.swizzle.z, c.swizzle.w}
    );
  }
  /* optimization for scaler */
  switch (mask) {
  case 0b1:
    ret = co_yield extract_element(0)(ret);
    break;
  case 0b10:
    ret = co_yield extract_element(1)(ret);
    break;
  case 0b100:
    ret = co_yield extract_element(2)(ret);
    break;
  case 0b1000:
    ret = co_yield extract_element(3)(ret);
    break;
  default:
    break;
  }
  if (c.abs) {
    ret = co_yield call_float_unary_op("fabs", ret);
  }
  if (c.neg) {
    ret = ctx.builder.CreateFNeg(ret);
  }
  co_return ret;
};

IRValue apply_integer_src_operand_modifier(
  SrcOperandModifier c, llvm::Value *ivec4, uint32_t mask
) {
  auto ctx = co_yield get_context();
  assert(ivec4->getType() == ctx.types._int4);
  auto ret = ivec4;
  if (c.swizzle != swizzle_identity) {
    ret = ctx.builder.CreateShuffleVector(
      ret, {c.swizzle.x, c.swizzle.y, c.swizzle.z, c.swizzle.w}
    );
  }
  /* optimization for scaler */
  switch (mask) {
  case 0b1:
    ret = co_yield extract_element(0)(ret);
    break;
  case 0b10:
    ret = co_yield extract_element(1)(ret);
    break;
  case 0b100:
    ret = co_yield extract_element(2)(ret);
    break;
  case 0b1000:
    ret = co_yield extract_element(3)(ret);
    break;
  default:
    break;
  }
  if (c.abs) {
    assert(0 && "otherwise dxbc spec is wrong");
  }
  if (c.neg) {
    ret = ctx.builder.CreateNeg(ret);
  }
  co_return ret;
};

uint32_t get_dst_mask(DstOperand dst) {
  return std::visit(
    patterns{
      [](DstOperandNull) { return (uint32_t)0b1111; },
      [](DstOperandOutput dst) { return dst._.mask; },
      [](DstOperandTemp dst) { return dst._.mask; },
      [](DstOperandIndexableTemp dst) { return dst._.mask; },
      [](DstOperandOutputDepth) { return (uint32_t)1; },
      [](auto s) {
        llvm::outs() << "get_dst_mask: unhandled dst operand type "
                     << decltype(s)::debug_name << "\n";
        assert(0 && "get_dst_mask: unhandled dst operand type");
        return (uint32_t)0;
      }
    },
    dst
  );
};

/* should provide single i32 value */
auto load_operand_index(OperandIndex idx) {
  return std::visit(
    patterns{
      [&](uint32_t v) {
        return make_irvalue([=](context ctx) { return ctx.builder.getInt32(v); }
        );
      },
      [&](IndexByTempComponent ot) {
        return make_irvalue([=](context ctx) {
          auto temp =
            load_from_array_at(
              ctx.resource.temp.ptr_int4, ctx.builder.getInt32(ot.regid)
            )
              .build(ctx);
          return ctx.builder.CreateAdd(
            ctx.builder.CreateExtractElement(temp, ot.component),
            ctx.builder.getInt32(ot.offset)
          );
        });
      },
      [&](IndexByIndexableTempComponent it) {
        return make_irvalue([=](context ctx) {
          assert(0 && "not implemented");
          return nullptr;
        });
      }
    },
    idx
  );
}

template <typename Operand, bool ReadFloat> IRValue load_src(Operand) {
  llvm::outs() << "register load of " << Operand::debug_name << "<"
               << (ReadFloat ? "float" : "int") << "> is not implemented\n";
  assert(0 && "operation not implemented");
};

template <bool ReadFloat>
IRValue load_src_op(SrcOperand src, uint32_t mask = 0b1111) {
  return std::visit(
           [](auto src) { return load_src<decltype(src), ReadFloat>(src); }, src
         ) >>= [mask, src](auto vec) {
    auto modifier = get_modifier(src);
    if (ReadFloat) {
      return apply_float_src_operand_modifier(modifier, vec, mask);
    } else {
      return apply_integer_src_operand_modifier(modifier, vec, mask);
    }
  };
};

template <>
IRValue load_src<SrcOperandConstantBuffer, false>(SrcOperandConstantBuffer cb) {
  auto ctx = co_yield get_context();
  auto cb_handle = co_yield ctx.resource.cb_range_map[cb.rangeid](nullptr);
  assert(cb_handle);
  auto ptr = ctx.builder.CreateGEP(
    ctx.types._int4, cb_handle, {co_yield load_operand_index(cb.regindex)}
  );
  co_return ctx.builder.CreateLoad(ctx.types._int4, ptr);
};

template <>
IRValue load_src<SrcOperandConstantBuffer, true>(SrcOperandConstantBuffer cb) {
  auto ctx = co_yield get_context();
  auto cb_handle = co_yield ctx.resource.cb_range_map[cb.rangeid](nullptr);
  assert(cb_handle);
  auto ptr = ctx.builder.CreateGEP(
    ctx.types._int4, cb_handle, {co_yield load_operand_index(cb.regindex)}
  );
  auto vec = ctx.builder.CreateBitCast(
    ctx.builder.CreateLoad(ctx.types._int4, ptr), ctx.types._float4
  );
  co_return vec;
};

template <>
IRValue load_src<SrcOperandImmediateConstantBuffer, false>(
  SrcOperandImmediateConstantBuffer cb
) {
  auto ctx = co_yield get_context();
  auto vec = co_yield load_from_array_at(
    ctx.resource.icb, co_yield load_operand_index(cb.regindex)
  );
  co_return vec;
};

template <>
IRValue load_src<SrcOperandImmediateConstantBuffer, true>(
  SrcOperandImmediateConstantBuffer cb
) {
  auto ctx = co_yield get_context();
  auto vec = co_yield load_from_array_at(
    ctx.resource.icb_float, co_yield load_operand_index(cb.regindex)
  );
  co_return vec;
};

template <>
IRValue load_src<SrcOperandImmediate32, false>(SrcOperandImmediate32 imm) {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantDataVector::get(ctx.llvm, imm.uvalue);
  });
};

template <>
IRValue load_src<SrcOperandImmediate32, true>(SrcOperandImmediate32 imm) {
  return make_irvalue([=](context ctx) {
    return llvm::ConstantDataVector::get(ctx.llvm, imm.fvalue);
  });
};

template <> IRValue load_src<SrcOperandTemp, true>(SrcOperandTemp temp) {
  auto ctx = co_yield get_context();
  auto s = co_yield load_from_array_at(
    ctx.resource.temp.ptr_float4, ctx.builder.getInt32(temp.regid)
  );
  co_return s;
};

template <> IRValue load_src<SrcOperandTemp, false>(SrcOperandTemp temp) {
  auto ctx = co_yield get_context();
  auto s = co_yield load_from_array_at(
    ctx.resource.temp.ptr_int4, ctx.builder.getInt32(temp.regid)
  );
  co_return s;
};

template <>
IRValue load_src<SrcOperandIndexableTemp, true>(SrcOperandIndexableTemp itemp) {
  auto ctx = co_yield get_context();
  auto regfile = ctx.resource.indexable_temp_map[itemp.regfile];
  auto s = co_yield load_from_array_at(
    regfile.ptr_float_vec, co_yield load_operand_index(itemp.regindex)
  );
  co_return co_yield extend_to_vec4(s);
};

template <>
IRValue load_src<SrcOperandIndexableTemp, false>(SrcOperandIndexableTemp itemp
) {
  auto ctx = co_yield get_context();
  auto regfile = ctx.resource.indexable_temp_map[itemp.regfile];
  auto s = co_yield load_from_array_at(
    regfile.ptr_int_vec, co_yield load_operand_index(itemp.regindex)
  );
  co_return co_yield extend_to_vec4(s);
};

template <> IRValue load_src<SrcOperandInput, true>(SrcOperandInput input) {
  auto ctx = co_yield get_context();
  auto s = co_yield load_from_array_at(
    ctx.resource.input.ptr_float4, ctx.builder.getInt32(input.regid)
  );
  co_return s;
};

template <> IRValue load_src<SrcOperandInput, false>(SrcOperandInput input) {
  auto ctx = co_yield get_context();
  auto s = co_yield load_from_array_at(
    ctx.resource.input.ptr_int4, ctx.builder.getInt32(input.regid)
  );
  co_return s;
};

template <>
IRValue load_src<SrcOperandAttribute, false>(SrcOperandAttribute attr) {
  auto ctx = co_yield get_context();
  pvalue vec;
  switch (attr.attribute) {
  case shader::common::InputAttribute::VertexId:
  case shader::common::InputAttribute::PrimitiveId:
  case shader::common::InputAttribute::InstanceId:
    assert(0 && "never reached: should be handled separately");
    break;
  case shader::common::InputAttribute::ThreadId: {
    assert(ctx.resource.thread_id_arg);
    vec = co_yield extend_to_vec4(ctx.resource.thread_id_arg);
    break;
  }
  case shader::common::InputAttribute::ThreadIdInGroup: {
    assert(ctx.resource.thread_id_in_group_arg);
    vec = co_yield extend_to_vec4(ctx.resource.thread_id_in_group_arg);
    break;
  }
  case shader::common::InputAttribute::ThreadGroupId: {
    assert(ctx.resource.thread_group_id_arg);
    vec = co_yield extend_to_vec4(ctx.resource.thread_group_id_arg);
    break;
  }
  case shader::common::InputAttribute::ThreadIdInGroupFlatten: {
    assert(ctx.resource.thread_id_in_group_flat_arg);
    vec = co_yield extend_to_vec4(ctx.resource.thread_id_in_group_flat_arg);
    break;
  }
  }
  co_return vec;
};

template <>
IRValue load_src<SrcOperandAttribute, true>(SrcOperandAttribute attr) {
  auto ctx = co_yield get_context();
  pvalue vec = nullptr;
  switch (attr.attribute) {
  case shader::common::InputAttribute::VertexId:
  case shader::common::InputAttribute::PrimitiveId:
  case shader::common::InputAttribute::InstanceId:
    assert(0 && "never reached: should be handled separately");
    break;
  case shader::common::InputAttribute::ThreadId: {
    vec = co_yield extend_to_vec4(ctx.resource.thread_id_arg) >>=
      bitcast_float4;
    break;
  }
  case shader::common::InputAttribute::ThreadIdInGroup: {
    vec = co_yield extend_to_vec4(ctx.resource.thread_id_in_group_arg) >>=
      bitcast_float4;
    break;
  }
  case shader::common::InputAttribute::ThreadGroupId: {
    vec = co_yield extend_to_vec4(ctx.resource.thread_group_id_arg) >>=
      bitcast_float4;
    break;
  }
  case shader::common::InputAttribute::ThreadIdInGroupFlatten: {
    vec = co_yield extend_to_vec4(ctx.resource.thread_id_in_group_flat_arg) >>=
      bitcast_float4;
    break;
  }
  }
  co_return vec;
};

template <typename Operand, bool StoreFloat>
IREffect store_dst(Operand, IRValue &&value) {
  llvm::outs() << "register store of " << Operand::debug_name << "<"
               << (StoreFloat ? "float" : "int") << "> is not implemented\n";
  assert(0 && "operation not implemented");
};

template <bool StoreFloat>
IREffect store_dst_op(DstOperand dst, IRValue &&value) {
  return std::visit(
    [value = std::move(value)](auto dst) mutable {
      return store_dst<decltype(dst), StoreFloat>(dst, std::move(value));
    },
    dst
  );
};

template <>
IREffect store_dst<DstOperandNull, false>(DstOperandNull, IRValue &&) {
  co_return {};
};

template <>
IREffect store_dst<DstOperandNull, true>(DstOperandNull, IRValue &&) {
  co_return {};
};

template <>
IREffect
store_dst<DstOperandTemp, false>(DstOperandTemp temp, IRValue &&value) {
  // coroutine + rvalue reference = SHOOT YOURSELF IN THE FOOT
  return make_effect_bind(
    [value = std::move(value), temp](auto ctx) mutable -> IREffect {
      co_return co_yield store_at_vec4_array_masked(
        ctx.resource.temp.ptr_int4, co_yield get_int(temp.regid),
        co_yield std::move(value), temp._.mask
      );
    }
  );
};

template <>
IREffect store_dst<DstOperandTemp, true>(DstOperandTemp temp, IRValue &&value) {
  // coroutine + rvalue reference = SHOOT YOURSELF IN THE FOOT
  return make_effect_bind(
    [value = std::move(value), temp](auto ctx) mutable -> IREffect {
      co_return co_yield store_at_vec4_array_masked(
        ctx.resource.temp.ptr_float4, co_yield get_int(temp.regid),
        co_yield std::move(value), temp._.mask
      );
    }
  );
};

template <>
IREffect store_dst<DstOperandIndexableTemp, true>(
  DstOperandIndexableTemp itemp, IRValue &&value
) {
  // coroutine + rvalue reference = SHOOT YOURSELF IN THE FOOT
  return make_effect_bind(
    [value = std::move(value), itemp](auto ctx) mutable -> IREffect {
      auto regfile = ctx.resource.indexable_temp_map[itemp.regfile];
      co_return co_yield store_at_vec_array_masked(
        regfile.ptr_float_vec, co_yield load_operand_index(itemp.regindex),
        co_yield std::move(value), itemp._.mask
      );
    }
  );
};

template <>
IREffect
store_dst<DstOperandOutput, false>(DstOperandOutput output, IRValue &&value) {
  // coroutine + rvalue reference = SHOOT YOURSELF IN THE FOOT
  return make_effect_bind(
    [value = std::move(value), output](auto ctx) mutable -> IREffect {
      co_return co_yield store_at_vec4_array_masked(
        ctx.resource.output.ptr_int4, co_yield get_int(output.regid),
        co_yield std::move(value), output._.mask
      );
    }
  );
};

template <>
IREffect
store_dst<DstOperandOutput, true>(DstOperandOutput output, IRValue &&value) {
  // coroutine + rvalue reference = SHOOT YOURSELF IN THE FOOT
  return make_effect_bind(
    [value = std::move(value), output](auto ctx) mutable -> IREffect {
      co_return co_yield store_at_vec4_array_masked(
        ctx.resource.output.ptr_float4, co_yield get_int(output.regid),
        co_yield std::move(value), output._.mask
      );
    }
  );
};

template <>
IREffect
store_dst<DstOperandOutputDepth, true>(DstOperandOutputDepth, IRValue &&value) {
  // coroutine + rvalue reference = SHOOT YOURSELF IN THE FOOT
  return make_effect_bind(
    [value = std::move(value)](context ctx) mutable -> IREffect {
      pvalue depth = co_yield std::move(value) >>=
        extract_element(0); // FIXME: value might be a scalar!
      auto ptr = ctx.builder.CreateConstGEP1_32(
        ctx.types._float, ctx.resource.depth_output_reg, 0
      );
      ctx.builder.CreateStore(depth, ptr);
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

auto load_tgsm(
  llvm::GlobalValue *g, pvalue offset_in_4bytes, uint32_t read_components
) -> IRValue {
  auto ctx = co_yield get_context();
  pvalue vec = llvm::UndefValue::get(ctx.types._int4);
  for (uint32_t i = 0; i < read_components; i++) {
    auto ptr = ctx.builder.CreateInBoundsGEP(
      llvm::cast<llvm::PointerType>(g->getType())
        ->getNonOpaquePointerElementType(),
      g,
      {ctx.builder.getInt32(0),
       ctx.builder.CreateAdd(offset_in_4bytes, ctx.builder.getInt32(i))}
    );
    vec = ctx.builder.CreateInsertElement(
      vec, ctx.builder.CreateLoad(ctx.types._int, ptr), i
    );
  }
  co_return vec;
};

auto load_texture_raw(
  air::MSLTexture texture, pvalue h, pvalue offset_in_4bytes,
  uint32_t read_components
) -> IRValue {
  assert(texture.resource_kind == air::TextureKind::texture_buffer);
  auto ctx = co_yield get_context();
  pvalue vec = llvm::UndefValue::get(ctx.types._int4);
  for (uint32_t i = 0; i < read_components; i++) {
    auto read_ret = call_read(
      texture, h, ctx.builder.CreateAdd(offset_in_4bytes, co_yield get_int(i))
    );
    vec = ctx.builder.CreateInsertElement(
      vec,
      co_yield (std::move(read_ret) >>= extract_value(0)) >>=
      extract_element(0),
      i
    );
  }
  co_return vec;
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

auto store_texture_raw(
  air::MSLTexture texture, pvalue h, pvalue offset_in_4bytes,
  uint32_t written_components, pvalue ivec4_to_write
) -> IREffect {
  assert(texture.resource_kind == air::TextureKind::texture_buffer);
  auto ctx = co_yield get_context();
  for (int i = 0; (uint32_t)i < written_components; i++) {
    co_yield call_write(
      texture, h,
      ctx.builder.CreateAdd(offset_in_4bytes, ctx.builder.getInt32(i)), nullptr,
      nullptr,
      ctx.builder.CreateShuffleVector(
        // by deisgn, only .r channel is written, but we need to provide a
        // vector anyway
        ivec4_to_write, llvm::ArrayRef<int>{i, i, i, i}
        // why shufflevector accept negative mask????
      ),
      nullptr
    );
  }
  co_return {};
};

auto convert_basicblocks(
  std::shared_ptr<BasicBlock> entry, context &ctx, llvm::BasicBlock *return_bb
) {
  auto &context = ctx.llvm;
  auto &builder = ctx.builder;
  auto function = ctx.function;
  auto &types = ctx.types;
  std::unordered_map<BasicBlock *, llvm::BasicBlock *> visited;
  std::function<void(std::shared_ptr<BasicBlock>)> readBasicBlock =
    [&](std::shared_ptr<BasicBlock> current) {
      auto bb =
        llvm::BasicBlock::Create(context, current->debug_name, function);
      assert(visited.insert({current.get(), bb}).second);
      IREffect effect([](auto) { return std::monostate(); });
      for (auto &inst : current->instructions) {
        std::visit(
          patterns{
            [&](InstMov mov) {
              effect << store_dst_op<true>(
                mov.dst, load_src_op<true>(mov.src) >>= saturate(mov._.saturate)
              );
            },
            [&](InstMovConditional movc) {
              effect << store_dst_op<true>(
                movc.dst,
                make_irvalue_bind([=](struct context ctx) -> IRValue {
                  auto src0 = co_yield load_src_op<true>(movc.src0);
                  auto src1 = co_yield load_src_op<true>(movc.src1);
                  auto cond = co_yield load_src_op<false>(movc.src_cond);
                  co_return ctx.builder.CreateSelect(
                    ctx.builder.CreateICmpNE(
                      cond, llvm::ConstantAggregateZero::get(ctx.types._int4)
                    ),
                    src0, src1
                  );
                }) >>= saturate(movc._.saturate)
              );
            },
            [&](InstSwapConditional swapc) {
              effect << make_effect_bind([=](struct context ctx) -> IREffect {
                auto src0 = co_yield load_src_op<true>(swapc.src0);
                auto src1 = co_yield load_src_op<true>(swapc.src1);
                auto cond = co_yield load_src_op<false>(swapc.src_cond);
                co_yield store_dst_op<true>(
                  swapc.dst0, make_irvalue([=](auto ctx) {
                    return ctx.builder.CreateSelect(
                      ctx.builder.CreateICmpNE(
                        cond, llvm::ConstantAggregateZero::get(ctx.types._int4)
                      ),
                      src1, src0
                    );
                  })
                );
                co_yield store_dst_op<true>(
                  swapc.dst1, make_irvalue([=](auto ctx) {
                    return ctx.builder.CreateSelect(
                      ctx.builder.CreateICmpNE(
                        cond, llvm::ConstantAggregateZero::get(ctx.types._int4)
                      ),
                      src0, src1
                    );
                  })
                );
                co_return {};
              });
            },
            [&](InstSample sample) {
              auto offset_const = llvm::ConstantVector::get(
                {llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[0], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[1], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[2], true}
                 )}
              );
              effect << store_dst_op<true>(
                sample.dst,
                (make_irvalue_bind([=](struct context ctx) -> IRValue {
                   auto [res, res_handle_fn, _] =
                     ctx.resource.srv_range_map[sample.src_resource.range_id];
                   auto sampler_handle_fn =
                     ctx.resource
                       .sampler_range_map[sample.src_sampler.range_id];
                   auto res_h = co_yield res_handle_fn(nullptr);
                   auto sampler_h = co_yield sampler_handle_fn(nullptr);
                   auto coord = co_yield load_src_op<true>(sample.src_address);
                   switch (res.resource_kind) {
                   case air::TextureKind::texture_1d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield extract_element(0)(coord), nullptr,
                       co_yield truncate_vec(1)(offset_const)
                     );
                   }
                   case air::TextureKind::texture_1d_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield extract_element(0)(coord), // .r
                       co_yield extract_element(1)(coord) >>=
                       implicit_float_to_int,                 // .g
                       co_yield truncate_vec(1)(offset_const) // 1d offset
                     );
                   }
                   case air::TextureKind::depth_2d:
                   case air::TextureKind::texture_2d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       nullptr,
                       co_yield truncate_vec(2)(offset_const) // 2d offset
                     );
                   }
                   case air::TextureKind::depth_2d_array:
                   case air::TextureKind::texture_2d_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       co_yield extract_element(2)(coord) >>=
                       implicit_float_to_int,                 // .b
                       co_yield truncate_vec(2)(offset_const) // 2d offset
                     );
                   }
                   case air::TextureKind::texture_3d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       nullptr,
                       co_yield truncate_vec(3)(offset_const) // 3d offset
                     );
                   }
                   case air::TextureKind::depth_cube:
                   case air::TextureKind::texture_cube: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       nullptr
                     );
                   }
                   case air::TextureKind::depth_cube_array:
                   case air::TextureKind::texture_cube_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       co_yield extract_element(3)(coord) >>=
                       implicit_float_to_int // .a
                     );
                   } break;
                   default: {
                     assert(0 && "invalid sample resource type");
                   }
                   }
                 }) >>= extract_value(0)) >>=
                swizzle(sample.src_resource.read_swizzle)
              );
            },
            [&](InstSampleLOD sample) {
              auto offset_const = llvm::ConstantVector::get(
                {llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[0], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[1], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[2], true}
                 )}
              );
              effect << store_dst_op<true>(
                sample.dst,
                (make_irvalue_bind([=](struct context ctx) -> IRValue {
                   auto [res, res_handle_fn, _] =
                     ctx.resource.srv_range_map[sample.src_resource.range_id];
                   auto sampler_handle_fn =
                     ctx.resource
                       .sampler_range_map[sample.src_sampler.range_id];
                   auto res_h = co_yield res_handle_fn(nullptr);
                   auto sampler_h = co_yield sampler_handle_fn(nullptr);
                   auto coord = co_yield load_src_op<true>(sample.src_address);
                   auto LOD = co_yield load_src_op<true>(sample.src_lod);
                   switch (res.resource_kind) {
                   case air::TextureKind::texture_1d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield extract_element(0)(coord), nullptr,
                       co_yield truncate_vec(1)(offset_const), nullptr, nullptr,
                       co_yield extract_element(0)(LOD)
                     );
                   }
                   case air::TextureKind::texture_1d_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield extract_element(0)(coord), // .r
                       co_yield extract_element(1)(coord) >>=
                       implicit_float_to_int,                  // .g
                       co_yield truncate_vec(1)(offset_const), // 1d offset
                       nullptr, nullptr, co_yield extract_element(0)(LOD)
                     );
                   }
                   case air::TextureKind::depth_2d:
                   case air::TextureKind::texture_2d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       nullptr,
                       co_yield truncate_vec(2)(offset_const), // 2d offset
                       nullptr, nullptr, co_yield extract_element(0)(LOD)
                     );
                   }
                   case air::TextureKind::depth_2d_array:
                   case air::TextureKind::texture_2d_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       co_yield extract_element(2)(coord) >>=
                       implicit_float_to_int,                  // .b
                       co_yield truncate_vec(2)(offset_const), // 2d offset
                       nullptr, nullptr, co_yield extract_element(0)(LOD)
                     );
                   }
                   case air::TextureKind::texture_3d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       nullptr,
                       co_yield truncate_vec(3)(offset_const), // 3d offset
                       nullptr, nullptr, co_yield extract_element(0)(LOD)
                     );
                   }
                   case air::TextureKind::depth_cube:
                   case air::TextureKind::texture_cube: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       nullptr, nullptr, nullptr, nullptr,
                       co_yield extract_element(0)(LOD)
                     );
                   }
                   case air::TextureKind::depth_cube_array:
                   case air::TextureKind::texture_cube_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       co_yield extract_element(3)(coord) >>=
                       implicit_float_to_int, // .a
                       nullptr, nullptr, nullptr,
                       co_yield extract_element(0)(LOD)
                     );
                   } break;
                   default: {
                     assert(0 && "invalid sample resource type");
                   }
                   }
                 }) >>= extract_value(0)) >>=
                swizzle(sample.src_resource.read_swizzle)
              );
            },
            [&](InstSampleBias sample) {
              auto offset_const = llvm::ConstantVector::get(
                {llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[0], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[1], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[2], true}
                 )}
              );
              effect << store_dst_op<true>(
                sample.dst,
                (make_irvalue_bind([=](struct context ctx) -> IRValue {
                   auto [res, res_handle_fn, _] =
                     ctx.resource.srv_range_map[sample.src_resource.range_id];
                   auto sampler_handle_fn =
                     ctx.resource
                       .sampler_range_map[sample.src_sampler.range_id];
                   auto res_h = co_yield res_handle_fn(nullptr);
                   auto sampler_h = co_yield sampler_handle_fn(nullptr);
                   auto coord = co_yield load_src_op<true>(sample.src_address);
                   auto bias = co_yield load_src_op<true>(sample.src_bias);
                   switch (res.resource_kind) {
                   case air::TextureKind::texture_1d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield extract_element(0)(coord), nullptr,
                       co_yield truncate_vec(1)(offset_const),
                       co_yield extract_element(0)(bias)
                     );
                   }
                   case air::TextureKind::texture_1d_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield extract_element(0)(coord), // .r
                       co_yield extract_element(1)(coord) >>=
                       implicit_float_to_int,                  // .g
                       co_yield truncate_vec(1)(offset_const), // 1d offset
                       co_yield extract_element(0)(bias)
                     );
                   }
                   case air::TextureKind::depth_2d:
                   case air::TextureKind::texture_2d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       nullptr,
                       co_yield truncate_vec(2)(offset_const), // 2d offset
                       co_yield extract_element(0)(bias)
                     );
                   }
                   case air::TextureKind::depth_2d_array:
                   case air::TextureKind::texture_2d_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       co_yield extract_element(2)(coord) >>=
                       implicit_float_to_int,                  // .b
                       co_yield truncate_vec(2)(offset_const), // 2d offset
                       co_yield extract_element(0)(bias)
                     );
                   }
                   case air::TextureKind::texture_3d: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       nullptr,
                       co_yield truncate_vec(3)(offset_const), // 3d offset
                       co_yield extract_element(0)(bias)
                     );
                   }
                   case air::TextureKind::depth_cube:
                   case air::TextureKind::texture_cube: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       nullptr, nullptr, co_yield extract_element(0)(bias)
                     );
                   }
                   case air::TextureKind::depth_cube_array:
                   case air::TextureKind::texture_cube_array: {
                     co_return co_yield call_sample(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       co_yield extract_element(3)(coord) >>=
                       implicit_float_to_int, // .a
                       nullptr, co_yield extract_element(0)(bias)
                     );
                   } break;
                   default: {
                     assert(0 && "invalid sample resource type");
                   }
                   }
                 }) >>= extract_value(0)) >>=
                swizzle(sample.src_resource.read_swizzle)
              );
            },
            [&](InstSampleDerivative sample) {
              auto offset_const = llvm::ConstantVector::get(
                {llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[0], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[1], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[2], true}
                 )}
              );
              effect << store_dst_op<true>(
                sample.dst,
                (make_irvalue_bind([=](struct context ctx) -> IRValue {
                   auto [res, res_handle_fn, _] =
                     ctx.resource.srv_range_map[sample.src_resource.range_id];
                   auto sampler_handle_fn =
                     ctx.resource
                       .sampler_range_map[sample.src_sampler.range_id];
                   auto res_h = co_yield res_handle_fn(nullptr);
                   auto sampler_h = co_yield sampler_handle_fn(nullptr);
                   auto coord = co_yield load_src_op<true>(sample.src_address);
                   auto dpdx =
                     co_yield load_src_op<true>(sample.src_x_derivative);
                   auto dpdy =
                     co_yield load_src_op<true>(sample.src_x_derivative);
                   switch (res.resource_kind) {
                   case air::TextureKind::depth_2d:
                   case air::TextureKind::texture_2d: {
                     co_return co_yield call_sample_grad(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       nullptr,                         // array_index
                       co_yield truncate_vec(2)(dpdx),
                       co_yield truncate_vec(2)(dpdy), nullptr,
                       co_yield truncate_vec(2)(offset_const) // 2d offset
                     );
                   }
                   case air::TextureKind::depth_2d_array:
                   case air::TextureKind::texture_2d_array: {
                     co_return co_yield call_sample_grad(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       co_yield extract_element(2)(coord) >>=
                       implicit_float_to_int, // array_index
                       co_yield truncate_vec(2)(dpdx),
                       co_yield truncate_vec(2)(dpdy), nullptr,
                       co_yield truncate_vec(2)(offset_const) // 2d offset
                     );
                   }
                   case air::TextureKind::texture_3d: {
                     co_return co_yield call_sample_grad(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       nullptr,                         // array_index
                       co_yield truncate_vec(3)(dpdx),
                       co_yield truncate_vec(3)(dpdy), nullptr,
                       co_yield truncate_vec(3)(offset_const) // 3d offset
                     );
                   }
                   case air::TextureKind::depth_cube:
                   case air::TextureKind::texture_cube: {
                     co_return co_yield call_sample_grad(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       nullptr,                         // array_index
                       co_yield truncate_vec(3)(dpdx),
                       co_yield truncate_vec(3)(dpdy), nullptr, nullptr
                     );
                   }
                   case air::TextureKind::depth_cube_array:
                   case air::TextureKind::texture_cube_array: {
                     co_return co_yield call_sample_grad(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rgb
                       co_yield extract_element(3)(coord) >>=
                       implicit_float_to_int, // array_index
                       co_yield truncate_vec(3)(dpdx),
                       co_yield truncate_vec(3)(dpdy), nullptr, nullptr
                     );
                   } break;
                   default: {
                     assert(0 && "invalid sample resource type");
                   }
                   }
                 }) >>= extract_value(0)) >>=
                swizzle(sample.src_resource.read_swizzle)
              );
            },
            [&](InstSampleCompare sample) {
              auto offset_const = llvm::ConstantVector::get(
                {llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[0], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[1], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[2], true}
                 )}
              );
              effect << store_dst_op<true>(
                sample.dst,
                (make_irvalue_bind([=](struct context ctx) -> IRValue {
                   auto [res, res_handle_fn, _] =
                     ctx.resource.srv_range_map[sample.src_resource.range_id];
                   auto sampler_handle_fn =
                     ctx.resource
                       .sampler_range_map[sample.src_sampler.range_id];
                   auto res_h = co_yield res_handle_fn(nullptr);
                   auto sampler_h = co_yield sampler_handle_fn(nullptr);
                   auto coord = co_yield load_src_op<true>(sample.src_address);
                   auto reference =
                     co_yield load_src_op<true>(sample.src_reference);
                   switch (res.resource_kind) {
                   case air::TextureKind::depth_2d: {
                     co_return co_yield call_sample_compare(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord),        // .rg
                       nullptr,                                // array_index
                       co_yield extract_element(0)(reference), // reference
                       co_yield truncate_vec(2)(offset_const), // 2d offset
                       nullptr, nullptr,
                       sample.level_zero
                         ? llvm::ConstantFP::getZero(ctx.types._float)
                         : nullptr
                     );
                   }
                   case air::TextureKind::depth_2d_array: {
                     co_return co_yield call_sample_compare(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(2)(coord), // .rg
                       co_yield extract_element(2)(coord) >>=
                       implicit_float_to_int,                  // array_index
                       co_yield extract_element(0)(reference), // reference
                       co_yield truncate_vec(2)(offset_const), // 2d offset
                       nullptr, nullptr,
                       sample.level_zero
                         ? llvm::ConstantFP::getZero(ctx.types._float)
                         : nullptr
                     );
                   }
                   case air::TextureKind::depth_cube: {
                     co_return co_yield call_sample_compare(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord),        // .rgb
                       nullptr,                                // array_index
                       co_yield extract_element(0)(reference), // reference
                       nullptr, nullptr, nullptr,
                       sample.level_zero
                         ? llvm::ConstantFP::getZero(ctx.types._float)
                         : nullptr
                     );
                   }
                   case air::TextureKind::depth_cube_array: {
                     co_return co_yield call_sample_compare(
                       res, res_h, sampler_h,
                       co_yield truncate_vec(3)(coord), // .rgb
                       co_yield extract_element(3)(coord) >>=
                       implicit_float_to_int,                  // array_index
                       co_yield extract_element(0)(reference), // reference
                       nullptr, nullptr, nullptr,
                       sample.level_zero
                         ? llvm::ConstantFP::getZero(ctx.types._float)
                         : nullptr
                     );
                   } break;
                   default: {
                     assert(0 && "invalid sample_compare resource type");
                   }
                   }
                 }) >>= extract_value(0))
              );
            },
            [&](InstGather sample) {
              effect << store_dst_op<true>(
                sample.dst,
                (make_irvalue_bind([=](struct context ctx) -> IRValue {
                   auto [res, res_handle_fn, _] =
                     ctx.resource.srv_range_map[sample.src_resource.range_id];
                   auto sampler_handle_fn =
                     ctx.resource
                       .sampler_range_map[sample.src_sampler.range_id];
                   auto res_h = co_yield res_handle_fn(nullptr);
                   auto sampler_h = co_yield sampler_handle_fn(nullptr);
                   auto coord = co_yield load_src_op<true>(sample.src_address);
                   auto offset = co_yield load_src_op<false>(sample.offset);
                   auto component =
                     co_yield get_int(sample.src_sampler.gather_channel);
                   switch (res.resource_kind) {
                   case air::TextureKind::depth_2d:
                   case air::TextureKind::texture_2d: {
                     co_return co_yield call_gather(
                       res, res_h, sampler_h, co_yield truncate_vec(2)(coord),
                       nullptr, co_yield truncate_vec(2)(offset), component
                     );
                   }
                   case air::TextureKind::depth_2d_array:
                   case air::TextureKind::texture_2d_array: {
                     co_return co_yield call_gather(
                       res, res_h, sampler_h, co_yield truncate_vec(2)(coord),
                       co_yield extract_element(2)(coord) >>=
                       implicit_float_to_int, // .b
                       co_yield truncate_vec(2)(offset), component
                     );
                   }
                   case air::TextureKind::depth_cube:
                   case air::TextureKind::texture_cube: {
                     co_return co_yield call_gather(
                       res, res_h, sampler_h, co_yield truncate_vec(3)(coord),
                       nullptr, nullptr, component
                     );
                   }
                   case air::TextureKind::depth_cube_array:
                   case air::TextureKind::texture_cube_array: {
                     co_return co_yield call_gather(
                       res, res_h, sampler_h, co_yield truncate_vec(3)(coord),
                       co_yield extract_element(3)(coord) >>=
                       implicit_float_to_int, // .b
                       nullptr, component
                     );
                   } break;
                   default: {
                     assert(0 && "invalid sample resource type");
                   }
                   }
                 }) >>= extract_value(0)) >>=
                swizzle(sample.src_resource.read_swizzle)
              );
            },
            [&](InstGatherCompare sample) {
              effect << store_dst_op<true>(
                sample.dst,
                (make_irvalue_bind([=](struct context ctx) -> IRValue {
                   auto [res, res_handle_fn, _] =
                     ctx.resource.srv_range_map[sample.src_resource.range_id];
                   auto sampler_handle_fn =
                     ctx.resource
                       .sampler_range_map[sample.src_sampler.range_id];
                   auto res_h = co_yield res_handle_fn(nullptr);
                   auto sampler_h = co_yield sampler_handle_fn(nullptr);
                   auto coord = co_yield load_src_op<true>(sample.src_address);
                   auto offset = co_yield load_src_op<false>(sample.offset);
                   auto reference =
                     co_yield load_src_op<true>(sample.src_reference);
                   switch (res.resource_kind) {
                   case air::TextureKind::depth_2d: {
                     co_return co_yield call_gather_compare(
                       res, res_h, sampler_h, co_yield truncate_vec(2)(coord),
                       nullptr, co_yield extract_element(0)(reference),
                       co_yield truncate_vec(2)(offset)
                     );
                   }
                   case air::TextureKind::depth_2d_array: {
                     co_return co_yield call_gather_compare(
                       res, res_h, sampler_h, co_yield truncate_vec(2)(coord),
                       co_yield extract_element(2)(coord) >>=
                       implicit_float_to_int, // .b
                       co_yield extract_element(0)(reference),
                       co_yield truncate_vec(2)(offset)
                     );
                   }
                   case air::TextureKind::depth_cube: {
                     co_return co_yield call_gather_compare(
                       res, res_h, sampler_h, co_yield truncate_vec(3)(coord),
                       nullptr, co_yield extract_element(0)(reference), nullptr
                     );
                   }
                   case air::TextureKind::depth_cube_array: {
                     co_return co_yield call_gather_compare(
                       res, res_h, sampler_h, co_yield truncate_vec(3)(coord),
                       co_yield extract_element(3)(coord) >>=
                       implicit_float_to_int, // .a
                       co_yield extract_element(0)(reference), nullptr
                     );
                   } break;
                   default: {
                     assert(0 && "invalid sample resource type");
                   }
                   }
                 }) >>= extract_value(0)) >>=
                swizzle(sample.src_resource.read_swizzle)
              );
            },

            [&](InstLoad load) {
              effect << make_effect_bind([=](struct context ctx) -> IREffect {
                auto [res, res_handle_fn, _] =
                  ctx.resource.srv_range_map[load.src_resource.range_id];
                auto res_h = co_yield res_handle_fn(nullptr);
                auto coord = co_yield load_src_op<false>(load.src_address);
                pvalue ret;
                switch (res.resource_kind) {
                case air::TextureKind::texture_buffer:
                case air::TextureKind::texture_1d: {
                  ret = co_yield call_read(
                    res, res_h, co_yield extract_element(0)(coord),
                    co_yield get_int(load.offsets[0])
                  );
                  break;
                }
                case air::TextureKind::texture_1d_array: {
                  ret = co_yield call_read(
                    res, res_h, co_yield extract_element(0)(coord),
                    co_yield get_int(load.offsets[0]), nullptr,
                    co_yield extract_element(1)(coord)
                  );
                  break;
                }
                case air::TextureKind::depth_2d:
                case air::TextureKind::texture_2d: {
                  ret = co_yield call_read(
                    res, res_h, co_yield truncate_vec(2)(coord),
                    co_yield get_int2(load.offsets[0], load.offsets[1])
                  );
                  break;
                }
                case air::TextureKind::depth_2d_array:
                case air::TextureKind::texture_2d_array: {
                  ret = co_yield call_read(
                    res, res_h, co_yield truncate_vec(2)(coord),
                    co_yield get_int2(load.offsets[0], load.offsets[1]),
                    nullptr, co_yield extract_element(2)(coord)
                  );
                  break;
                }
                case air::TextureKind::texture_3d: {
                  ret = co_yield call_read(
                    res, res_h, co_yield truncate_vec(3)(coord),
                    co_yield get_int3(
                      load.offsets[0], load.offsets[1], load.offsets[2]
                    )
                  );
                  break;
                }
                case air::TextureKind::texture_2d_ms:
                case air::TextureKind::depth_2d_ms: {
                  auto sample_index =
                    co_yield load_src_op<false>(load.src_sample_index.value()
                    ) >>= extract_element(0);
                  ret = co_yield call_read(
                    res, res_h, co_yield truncate_vec(2)(coord),
                    co_yield get_int2(load.offsets[0], load.offsets[1]),
                    nullptr, nullptr, sample_index
                  );
                  break;
                }
                case air::TextureKind::texture_2d_ms_array:
                case air::TextureKind::depth_2d_ms_array: {
                  auto sample_index =
                    co_yield load_src_op<false>(load.src_sample_index.value()
                    ) >>= extract_element(0);
                  ret = co_yield call_read(
                    res, res_h, co_yield truncate_vec(2)(coord),
                    co_yield get_int2(load.offsets[0], load.offsets[1]),
                    nullptr, co_yield extract_element(2)(coord), sample_index
                  );
                  break;
                }
                default: {
                  assert(0 && "invalid load resource type");
                }
                }
                ret = co_yield extract_value(0)(ret) >>=
                  swizzle(load.src_resource.read_swizzle);
                co_return co_yield std::visit(
                  patterns{
                    [&](air::MSLFloat) {
                      return store_dst_op<true>(load.dst, pure(ret));
                    },
                    [&](auto) {
                      return store_dst_op<false>(load.dst, pure(ret));
                    }
                  },
                  res.component_type
                );
              });
            },
            [&](InstLoadUAVTyped load) {
              effect << make_effect_bind([=](struct context ctx) -> IREffect {
                auto [res, res_handle_fn, _] =
                  ctx.resource.uav_range_map[load.src_uav.range_id];
                auto res_h = co_yield res_handle_fn(nullptr);
                auto coord = co_yield load_src_op<false>(load.src_address);
                pvalue ret;
                switch (res.resource_kind) {
                case air::TextureKind::texture_buffer:
                case air::TextureKind::texture_1d: {
                  ret = co_yield call_read(
                    res, res_h, co_yield extract_element(0)(coord)
                  );
                  break;
                }
                case air::TextureKind::texture_1d_array: {
                  ret = co_yield call_read(
                    res, res_h, co_yield extract_element(0)(coord), nullptr,
                    nullptr, co_yield extract_element(1)(coord)
                  );
                  break;
                }
                case air::TextureKind::depth_2d:
                case air::TextureKind::texture_2d: {
                  ret = co_yield call_read(
                    res, res_h, co_yield truncate_vec(2)(coord)
                  );
                  break;
                }
                case air::TextureKind::depth_2d_array:
                case air::TextureKind::texture_2d_array: {
                  ret = co_yield call_read(
                    res, res_h, co_yield truncate_vec(2)(coord), nullptr,
                    nullptr, co_yield extract_element(2)(coord)
                  );
                  break;
                }
                case air::TextureKind::texture_3d: {
                  ret = co_yield call_read(
                    res, res_h, co_yield truncate_vec(3)(coord)
                  );
                  break;
                }
                default: {
                  assert(0 && "invalid load resource type");
                }
                }
                ret = co_yield extract_value(0)(ret) >>=
                  swizzle(load.src_uav.read_swizzle);
                co_return co_yield std::visit(
                  patterns{
                    [&](air::MSLFloat) {
                      return store_dst_op<true>(load.dst, pure(ret));
                    },
                    [&](auto) {
                      return store_dst_op<false>(load.dst, pure(ret));
                    }
                  },
                  res.component_type
                );
              });
            },
            [&](InstStoreUAVTyped store) {
              effect << make_effect_bind([=](struct context ctx) -> IREffect {
                auto [res, res_handle_fn, _] =
                  ctx.resource.uav_range_map[store.dst.range_id];
                auto res_h = co_yield res_handle_fn(nullptr);
                auto address = co_yield load_src_op<false>(store.src_address);
                auto value = co_yield std::visit(
                  patterns{
                    [&](air::MSLFloat) { return load_src_op<true>(store.src); },
                    [&](auto) { return load_src_op<false>(store.src); }
                  },
                  res.component_type
                );

                switch (res.resource_kind) {
                case air::TextureKind::texture_buffer:
                  co_yield call_write(
                    res, res_h, co_yield extract_element(0)(address), nullptr,
                    nullptr, value, ctx.builder.getInt32(0)
                  );
                  break;
                case air::TextureKind::texture_1d:
                  co_yield call_write(
                    res, res_h, co_yield extract_element(0)(address), nullptr,
                    nullptr, value, ctx.builder.getInt32(0)
                  );
                  break;
                case air::TextureKind::texture_1d_array:
                  co_yield call_write(
                    res, res_h, co_yield extract_element(0)(address), nullptr,
                    co_yield extract_element(1)(address), value,
                    ctx.builder.getInt32(0)
                  );
                  break;
                case air::TextureKind::texture_2d:
                  co_yield call_write(
                    res, res_h, co_yield truncate_vec(2)(address), nullptr,
                    nullptr, value, ctx.builder.getInt32(0)
                  );
                  break;
                case air::TextureKind::texture_2d_array: {
                  co_yield call_write(
                    res, res_h, co_yield truncate_vec(2)(address), nullptr,
                    co_yield extract_element(2)(address), value,
                    ctx.builder.getInt32(0)
                  );
                  break;
                }
                case air::TextureKind::texture_3d:
                  co_yield call_write(
                    res, res_h, co_yield truncate_vec(3)(address), nullptr,
                    nullptr, value, ctx.builder.getInt32(0)
                  );
                  break;
                default:
                  assert(0 && "invalid texture kind for uav store");
                }
                co_return {};
              });
            },
            [&](InstLoadStructured load) {
              effect << std::visit(
                patterns{
                  [&](SrcOperandTGSM tgsm) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [stride, tgsm_h] = ctx.resource.tgsm_map[tgsm.id];
                        auto offset = builder.CreateLShr(
                          builder.CreateAdd(
                            builder.CreateMul(
                              builder.getInt32(stride),
                              co_yield load_src_op<false>(load.src_address) >>=
                              extract_element(0)
                            ),
                            co_yield load_src_op<false>(load.src_byte_offset
                            ) >>= extract_element(0)
                          ),
                          2
                        );
                        co_return co_yield store_dst_op<false>(
                          load.dst,
                          load_tgsm(
                            tgsm_h, offset,
                            std::max(
                              {tgsm.read_swizzle.x, tgsm.read_swizzle.y,
                               tgsm.read_swizzle.z, tgsm.read_swizzle.w}
                            ) +
                              1
                          ) >>= swizzle(tgsm.read_swizzle)
                        );
                      }
                    );
                  },
                  [&](SrcOperandResource srv) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [res, res_handle_fn, stride] =
                          ctx.resource.srv_range_map[srv.range_id];
                        auto res_h = co_yield res_handle_fn(nullptr);
                        auto offset = builder.CreateLShr(
                          builder.CreateAdd(
                            builder.CreateMul(
                              builder.getInt32(stride),
                              co_yield load_src_op<false>(load.src_address) >>=
                              extract_element(0)
                            ),
                            co_yield load_src_op<false>(load.src_byte_offset
                            ) >>= extract_element(0)
                          ),
                          2
                        );
                        co_return co_yield store_dst_op<false>(
                          load.dst, load_texture_raw(
                                      res, res_h, offset,
                                      std::max(
                                        {srv.read_swizzle.x, srv.read_swizzle.y,
                                         srv.read_swizzle.z, srv.read_swizzle.w}
                                      ) +
                                        1
                                    ) >>= swizzle(srv.read_swizzle)
                        );
                      }
                    );
                  },
                  [&](SrcOperandUAV uav) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [res, res_handle_fn, stride] =
                          ctx.resource.uav_range_map[uav.range_id];
                        auto res_h = co_yield res_handle_fn(nullptr);
                        auto offset = builder.CreateLShr(
                          builder.CreateAdd(
                            builder.CreateMul(
                              builder.getInt32(stride),
                              co_yield load_src_op<false>(load.src_address) >>=
                              extract_element(0)
                            ),
                            co_yield load_src_op<false>(load.src_byte_offset
                            ) >>= extract_element(0)
                          ),
                          2
                        );
                        co_return co_yield store_dst_op<false>(
                          load.dst, load_texture_raw(
                                      res, res_h, offset,
                                      std::max(
                                        {uav.read_swizzle.x, uav.read_swizzle.y,
                                         uav.read_swizzle.z, uav.read_swizzle.w}
                                      ) +
                                        1
                                    ) >>= swizzle(uav.read_swizzle)
                        );
                      }
                    );
                  }
                },
                load.src
              );
            },
            [&](InstStoreStructured store) {
              effect << std::visit(
                patterns{
                  [&](AtomicOperandTGSM tgsm) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [stride, tgsm_h] = ctx.resource.tgsm_map[tgsm.id];
                        auto offset = builder.CreateLShr(
                          builder.CreateAdd(
                            builder.CreateMul(
                              builder.getInt32(stride),
                              co_yield load_src_op<false>(store.dst_address) >>=
                              extract_element(0)
                            ),
                            co_yield load_src_op<false>(store.dst_byte_offset
                            ) >>= extract_element(0)
                          ),
                          2
                        );
                        co_return co_yield store_tgsm(
                          tgsm_h, offset,
                          mask_to_linear_components_num(tgsm.mask),
                          co_yield load_src_op<false>(store.src)
                        );
                      }
                    );
                  },
                  [&](AtomicDstOperandUAV uav) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [res, res_handle_fn, stride] =
                          ctx.resource.uav_range_map[uav.range_id];
                        auto res_h = co_yield res_handle_fn(nullptr);
                        auto offset = builder.CreateLShr(
                          builder.CreateAdd(
                            builder.CreateMul(
                              builder.getInt32(stride),
                              co_yield load_src_op<false>(store.dst_address) >>=
                              extract_element(0)
                            ),
                            co_yield load_src_op<false>(store.dst_byte_offset
                            ) >>= extract_element(0)
                          ),
                          2
                        );
                        co_return co_yield store_texture_raw(
                          res, res_h, offset,
                          mask_to_linear_components_num(uav.mask),
                          co_yield load_src_op<false>(store.src)
                        );
                      }
                    );
                  }
                },
                store.dst
              );
            },
            [&](InstLoadRaw load) {
              effect << std::visit(
                patterns{
                  [&](SrcOperandTGSM tgsm) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [stride, tgsm_h] = ctx.resource.tgsm_map[tgsm.id];
                        auto offset = builder.CreateLShr(
                          co_yield load_src_op<false>(load.src_byte_offset) >>=
                          extract_element(0),
                          2
                        );
                        co_return co_yield store_dst_op<false>(
                          load.dst,
                          load_tgsm(
                            tgsm_h, offset,
                            std::max(
                              {tgsm.read_swizzle.x, tgsm.read_swizzle.y,
                               tgsm.read_swizzle.z, tgsm.read_swizzle.w}
                            ) +
                              1
                          ) >>= swizzle(tgsm.read_swizzle)
                        );
                      }
                    );
                  },
                  [&](SrcOperandResource srv) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [res, res_handle_fn, stride] =
                          ctx.resource.srv_range_map[srv.range_id];
                        auto res_h = co_yield res_handle_fn(nullptr);
                        auto offset = builder.CreateLShr(
                          co_yield load_src_op<false>(load.src_byte_offset) >>=
                          extract_element(0),
                          2
                        );
                        co_return co_yield store_dst_op<false>(
                          load.dst, load_texture_raw(
                                      res, res_h, offset,
                                      std::max(
                                        {srv.read_swizzle.x, srv.read_swizzle.y,
                                         srv.read_swizzle.z, srv.read_swizzle.w}
                                      ) +
                                        1
                                    ) >>= swizzle(srv.read_swizzle)
                        );
                      }
                    );
                  },
                  [&](SrcOperandUAV uav) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [res, res_handle_fn, stride] =
                          ctx.resource.uav_range_map[uav.range_id];
                        auto res_h = co_yield res_handle_fn(nullptr);
                        auto offset = builder.CreateLShr(
                          co_yield load_src_op<false>(load.src_byte_offset) >>=
                          extract_element(0),
                          2
                        );
                        co_return co_yield store_dst_op<false>(
                          load.dst, load_texture_raw(
                                      res, res_h, offset,
                                      std::max(
                                        {uav.read_swizzle.x, uav.read_swizzle.y,
                                         uav.read_swizzle.z, uav.read_swizzle.w}
                                      ) +
                                        1
                                    ) >>= swizzle(uav.read_swizzle)
                        );
                      }
                    );
                  }
                },
                load.src
              );
            },
            [&](InstStoreRaw store) {
              effect << std::visit(
                patterns{
                  [&](AtomicOperandTGSM tgsm) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [stride, tgsm_h] = ctx.resource.tgsm_map[tgsm.id];
                        auto offset = builder.CreateLShr(
                          co_yield load_src_op<false>(store.dst_byte_offset) >>=
                          extract_element(0),
                          2
                        );
                        co_return co_yield store_tgsm(
                          tgsm_h, offset,
                          mask_to_linear_components_num(tgsm.mask),
                          co_yield load_src_op<false>(store.src)
                        );
                      }
                    );
                  },
                  [&](AtomicDstOperandUAV uav) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [res, res_handle_fn, stride] =
                          ctx.resource.uav_range_map[uav.range_id];
                        auto res_h = co_yield res_handle_fn(nullptr);
                        auto offset = builder.CreateLShr(
                          co_yield load_src_op<false>(store.dst_byte_offset) >>=
                          extract_element(0),
                          2
                        );
                        co_return co_yield store_texture_raw(
                          res, res_h, offset,
                          mask_to_linear_components_num(uav.mask),
                          co_yield load_src_op<false>(store.src)
                        );
                      }
                    );
                  }
                },
                store.dst
              );
            },
            [&](InstIntegerCompare icmp) {
              llvm::CmpInst::Predicate pred;
              switch (icmp.cmp) {
              case IntegerComparison::Equal:
                pred = llvm::CmpInst::ICMP_EQ;
                break;
              case IntegerComparison::NotEqual:
                pred = llvm::CmpInst::ICMP_NE;
                break;
              case IntegerComparison::SignedLessThan:
                pred = llvm::CmpInst::ICMP_SLT;
                break;
              case IntegerComparison::SignedGreaterEqual:
                pred = llvm::CmpInst::ICMP_SGE;
                break;
              case IntegerComparison::UnsignedLessThan:
                pred = llvm::CmpInst::ICMP_ULT;
                break;
              case IntegerComparison::UnsignedGreaterEqual:
                pred = llvm::CmpInst::ICMP_UGE;
                break;
              }
              effect << store_dst_op<false>(
                icmp.dst,
                lift(
                  load_src_op<false>(icmp.src0), load_src_op<false>(icmp.src1),
                  [=](auto a, auto b) { return cmp_integer(pred, a, b); }
                )
              );
            },
            [&](InstIntegerUnaryOp unary) {
              std::function<IRValue(pvalue)> fn;
              switch (unary.op) {
              case IntegerUnaryOp::Neg:
                fn = [=](pvalue a) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateNeg(a);
                  });
                };
                break;
              case IntegerUnaryOp::Not:
                fn = [=](pvalue a) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateNot(a);
                  });
                };
                break;
              case IntegerUnaryOp::ReverseBits:
                fn = [=](pvalue a) {
                  // metal shader spec doesn't make it clear
                  // if this is component-wise...
                  return call_integer_unary_op("reverse_bits", a);
                };
                break;
              case IntegerUnaryOp::CountBits:
                fn = [=](pvalue a) {
                  // metal shader spec doesn't make it clear
                  // if this is component-wise...
                  return call_integer_unary_op("popcount", a);
                };
                break;
              case IntegerUnaryOp::FirstHiBitSigned:
              case IntegerUnaryOp::FirstHiBit:
              case IntegerUnaryOp::FirstLowBit:
                assert(0 && "TODO: should be polyfilled");
                break;
              }
              auto mask = get_dst_mask(unary.dst);
              effect << store_dst_op<false>(
                unary.dst, load_src_op<false>(unary.src, mask) >>= fn
              );
            },
            [&](InstIntegerBinaryOp bin) {
              std::function<IRValue(pvalue, pvalue)> fn;
              switch (bin.op) {
              case IntegerBinaryOp::IShl:
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateShl(
                      a, ctx.builder.CreateAnd(b, 0x1f)
                    );
                  });
                };
                break;
              case IntegerBinaryOp::IShr:
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateAShr(
                      a, ctx.builder.CreateAnd(b, 0x1f)
                    );
                  });
                };
                break;
              case IntegerBinaryOp::UShr:
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateLShr(
                      a, ctx.builder.CreateAnd(b, 0x1f)
                    );
                  });
                };
                break;
              case IntegerBinaryOp::Xor:
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateXor(a, b);
                  });
                };
                break;
              case IntegerBinaryOp::And:
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateAnd(a, b);
                  });
                };
                break;
              case IntegerBinaryOp::Or:
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateOr(a, b);
                  });
                };
                break;
              case IntegerBinaryOp::Add:
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateAdd(a, b);
                  });
                };
                break;
              case IntegerBinaryOp::UMin:
                fn = [=](pvalue a, pvalue b) {
                  return call_integer_binop("min", a, b, false);
                };
                break;
              case IntegerBinaryOp::UMax:
                fn = [=](pvalue a, pvalue b) {
                  return call_integer_binop("max", a, b, false);
                };
                break;
              case IntegerBinaryOp::IMin:
                fn = [=](pvalue a, pvalue b) {
                  return call_integer_binop("min", a, b, true);
                };
                break;
              case IntegerBinaryOp::IMax:
                fn = [=](pvalue a, pvalue b) {
                  return call_integer_binop("max", a, b, true);
                };
                break;
              }
              auto mask = get_dst_mask(bin.dst);
              effect << store_dst_op<false>(
                bin.dst, lift(
                           load_src_op<false>(bin.src0, mask),
                           load_src_op<false>(bin.src1, mask), fn
                         )
              );
            },
            [&](InstFloatCompare fcmp) {
              llvm::CmpInst::Predicate pred;
              switch (fcmp.cmp) {
              case FloatComparison::Equal:
                pred = llvm::CmpInst::FCMP_OEQ;
                break;
              case FloatComparison::NotEqual:
                pred = llvm::CmpInst::FCMP_UNE;
                break;
              case FloatComparison::GreaterEqual:
                pred = llvm::CmpInst::FCMP_OGE;
                break;
              case FloatComparison::LessThan:
                pred = llvm::CmpInst::FCMP_OLT;
                break;
              }
              effect << store_dst_op<false>(
                fcmp.dst,
                lift(
                  load_src_op<true>(fcmp.src0), load_src_op<true>(fcmp.src1),
                  [=](auto a, auto b) { return cmp_float(pred, a, b); }
                )
              );
            },
            [&](InstDotProduct dp) {
              switch (dp.dimension) {
              case 4:
                effect << lift(
                  load_src_op<true>(dp.src0), load_src_op<true>(dp.src1),
                  [=](auto a, auto b) {
                    return store_dst_op<true>(
                      dp.dst, call_dot_product(4, a, b)
                    );
                  }
                );
                break;
              case 3:
                effect << lift(
                  load_src_op<true>(dp.src0) >>= truncate_vec(3),
                  load_src_op<true>(dp.src1) >>= truncate_vec(3),
                  [=](auto a, auto b) {
                    return store_dst_op<true>(
                      dp.dst, call_dot_product(3, a, b)
                    );
                  }
                );
                break;
              case 2:
                effect << lift(
                  load_src_op<true>(dp.src0) >>= truncate_vec(2),
                  load_src_op<true>(dp.src1) >>= truncate_vec(2),
                  [=](auto a, auto b) {
                    return store_dst_op<true>(
                      dp.dst, call_dot_product(2, a, b)
                    );
                  }
                );
                break;
              default:
                assert(0 && "wrong dot product dimension");
              }
            },
            [&](InstFloatMAD mad) {
              auto mask = get_dst_mask(mad.dst);
              effect << lift(
                load_src_op<true>(mad.src0, mask),
                load_src_op<true>(mad.src1, mask),
                load_src_op<true>(mad.src2, mask),
                [=](auto a, auto b, auto c) {
                  return store_dst_op<true>(
                    mad.dst,
                    call_float_mad(a, b, c) >>= saturate(mad._.saturate)
                  );
                }
              );
            },
            [&](InstIntegerMAD mad) {
              // FIXME: this looks wrong, what's the sign specific behavior?
              auto mask = get_dst_mask(mad.dst);
              effect << lift(
                load_src_op<false>(mad.src0, mask),
                load_src_op<false>(mad.src1, mask),
                load_src_op<false>(mad.src2, mask),
                [=](auto a, auto b, auto c) {
                  return store_dst_op<false>(
                    mad.dst, make_irvalue([=](struct context ctx) {
                      return ctx.builder.CreateAdd(
                        ctx.builder.CreateMul(a, b), c
                      );
                    })
                  );
                }
              );
            },
            [&](InstFloatUnaryOp unary) {
              std::function<IRValue(pvalue)> fn;
              switch (unary.op) {
              case FloatUnaryOp::Log2: {
                fn = [=](pvalue a) { return call_float_unary_op("log2", a); };
                break;
              }
              case FloatUnaryOp::Exp2: {
                fn = [=](pvalue a) { return call_float_unary_op("exp2", a); };
                break;
              }
              case FloatUnaryOp::Rcp: {
                fn = [=](pvalue a) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateFDiv(
                      ctx.builder.CreateVectorSplat(
                        4, llvm::ConstantFP::get(ctx.llvm, llvm::APFloat{1.0f})
                      ),
                      a
                    );
                  });
                };
                break;
              }
              case FloatUnaryOp::Rsq: {
                fn = [=](pvalue a) { return call_float_unary_op("rsqrt", a); };
                break;
              }
              case FloatUnaryOp::Sqrt: {
                fn = [=](pvalue a) { return call_float_unary_op("sqrt", a); };
                break;
              }
              case FloatUnaryOp::Fraction: {
                fn = [=](pvalue a) { return call_float_unary_op("fract", a); };
                break;
              }
              case FloatUnaryOp::RoundNearestEven: {
                fn = [=](pvalue a) { return call_float_unary_op("rint", a); };
                break;
              }
              case FloatUnaryOp::RoundNegativeInf: {
                fn = [=](pvalue a) { return call_float_unary_op("floor", a); };
                break;
              }
              case FloatUnaryOp::RoundPositiveInf: {
                fn = [=](pvalue a) { return call_float_unary_op("ceil", a); };
                break;
              }
              case FloatUnaryOp::RoundZero: {
                fn = [=](pvalue a) { return call_float_unary_op("trunc", a); };
                break;
              } break;
              }
              auto mask = get_dst_mask(unary.dst);
              effect << store_dst_op<true>(
                unary.dst, (load_src_op<true>(unary.src, mask) >>= fn) >>=
                           saturate(unary._.saturate)
              );
            },
            [&](InstFloatBinaryOp bin) {
              std::function<IRValue(pvalue, pvalue)> fn;
              switch (bin.op) {
              case FloatBinaryOp::Add: {
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateFAdd(a, b);
                  });
                };
                break;
              }
              case FloatBinaryOp::Mul: {
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateFMul(a, b);
                  });
                };
                break;
              }
              case FloatBinaryOp::Div: {
                fn = [=](pvalue a, pvalue b) {
                  return make_irvalue([=](struct context ctx) {
                    return ctx.builder.CreateFDiv(a, b);
                  });
                };
                break;
              }
              case FloatBinaryOp::Min: {
                fn = [=](pvalue a, pvalue b) {
                  return call_float_binop("fmin", a, b);
                };
                break;
              }
              case FloatBinaryOp::Max: {
                fn = [=](pvalue a, pvalue b) {
                  return call_float_binop("fmax", a, b);
                };
                break;
              }
              }
              auto mask = get_dst_mask(bin.dst);
              effect << store_dst_op<true>(
                bin.dst, lift(
                           load_src_op<true>(bin.src0, mask),
                           load_src_op<true>(bin.src1, mask), fn
                         ) >>= saturate(bin._.saturate)
              );
            },
            [&](InstSinCos sincos) {
              auto mask_cos = get_dst_mask(sincos.dst_cos);
              effect << store_dst_op<true>(
                sincos.dst_cos, load_src_op<true>(sincos.src, mask_cos) >>=
                                [=](auto src) {
                                  return call_float_unary_op("cos", src) >>=
                                         saturate(sincos._.saturate);
                                }
              );

              auto mask_sin = get_dst_mask(sincos.dst_sin);
              effect << store_dst_op<true>(
                sincos.dst_sin, load_src_op<true>(sincos.src, mask_sin) >>=
                                [=](auto src) {
                                  return call_float_unary_op("sin", src) >>=
                                         saturate(sincos._.saturate);
                                }
              );
            },
            [&](InstConvert convert) {
              switch (convert.op) {
              case ConversionOp::HalfToFloat: {
                effect << store_dst_op<true>(
                  convert.dst,
                  make_irvalue_bind([=](struct context ctx) -> IRValue {
                    // FIXME: neg modifier might be wrong?!
                    auto src = co_yield load_src_op<false>(convert.src); // int4
                    auto half8 = ctx.builder.CreateBitCast(
                      src, llvm::FixedVectorType::get(ctx.types._half, 8)
                    );
                    co_return co_yield call_convert(
                      ctx.builder.CreateShuffleVector(half8, {0, 2, 4, 6}),
                      ctx.types._half4, ctx.types._float4, ".f.v4f16",
                      ".f.v4f32"
                    );
                  })
                );
                break;
              }
              case ConversionOp::FloatToHalf: {
                effect << store_dst_op<false>(
                  convert.dst,
                  make_irvalue_bind([=](struct context ctx) -> IRValue {
                    // FIXME: neg modifier might be wrong?!
                    auto src =
                      co_yield load_src_op<true>(convert.src); // float4
                    auto half4 = co_yield call_convert(
                      src, ctx.types._float4, ctx.types._half4, ".f.v4f32",
                      ".f.v4f16"
                    );
                    co_return ctx.builder.CreateBitCast(
                      ctx.builder.CreateShuffleVector(
                        half4,
                        llvm::ConstantAggregateZero::get(half4->getType()),
                        {0, 4, 1, 5, 2, 6, 3, 7}
                      ),
                      ctx.types._int4
                    );
                  })
                );
                break;
              }
              case ConversionOp::FloatToSigned: {
                effect << store_dst_op<false>(
                  convert.dst,
                  make_irvalue_bind([=](struct context ctx) -> IRValue {
                    auto src = co_yield load_src_op<true>(convert.src);
                    co_return co_yield call_convert(
                      src, ctx.types._float4, ctx.types._int4, ".f.v4f32",
                      ".s.v4i32"
                    );
                  })
                );
                break;
              }
              case ConversionOp::SignedToFloat: {
                effect << store_dst_op<true>(
                  convert.dst,
                  make_irvalue_bind([=](struct context ctx) -> IRValue {
                    auto src = co_yield load_src_op<false>(convert.src);
                    co_return co_yield call_convert(
                      src, ctx.types._int4, ctx.types._float4, ".s.v4i32",
                      ".f.v4f32"
                    );
                  })
                );
                break;
              }
              case ConversionOp::FloatToUnsigned: {
                effect << store_dst_op<false>(
                  convert.dst,
                  make_irvalue_bind([=](struct context ctx) -> IRValue {
                    auto src = co_yield load_src_op<true>(convert.src);
                    co_return co_yield call_convert(
                      src, ctx.types._float4, ctx.types._int4, ".f.v4f32",
                      ".u.v4i32"
                    );
                  })
                );
                break;
              }
              case ConversionOp::UnsignedToFloat: {
                effect << store_dst_op<true>(
                  convert.dst,
                  make_irvalue_bind([=](struct context ctx) -> IRValue {
                    auto src = co_yield load_src_op<false>(convert.src);
                    co_return co_yield call_convert(
                      src, ctx.types._int4, ctx.types._float4, ".u.v4i32",
                      ".f.v4f32"
                    );
                  })
                );
                break;
              }
              }
            },
            [&](InstIntegerBinaryOpWithTwoDst bin) {
              switch (bin.op) {
              case IntegerBinaryOpWithTwoDst::IMul: {
                effect << make_effect_bind([=](struct context ctx) -> IREffect {
                  auto a = co_yield load_src_op<false>(bin.src0);
                  auto b = co_yield load_src_op<false>(bin.src1);
                  auto mul = ctx.builder.CreateBitCast(
                    ctx.builder.CreateMul(
                      ctx.builder.CreateSExt(a, ctx.types._long4),
                      ctx.builder.CreateSExt(b, ctx.types._long4)
                    ),
                    ctx.types._int8
                  );
                  co_yield store_dst_op<false>(
                    bin.dst_hi,
                    pure(ctx.builder.CreateShuffleVector(mul, {1, 3, 5, 7}))
                  );
                  co_yield store_dst_op<false>(
                    bin.dst_low,
                    pure(ctx.builder.CreateShuffleVector(mul, {0, 2, 4, 6}))
                  );
                  co_return {};
                });
                break;
              }
              case IntegerBinaryOpWithTwoDst::UMul: {
                effect << make_effect_bind([=](struct context ctx) -> IREffect {
                  auto a = co_yield load_src_op<false>(bin.src0);
                  auto b = co_yield load_src_op<false>(bin.src1);
                  auto mul = ctx.builder.CreateBitCast(
                    ctx.builder.CreateMul(
                      ctx.builder.CreateZExt(a, ctx.types._long4),
                      ctx.builder.CreateZExt(b, ctx.types._long4)
                    ),
                    ctx.types._int8
                  );
                  co_yield store_dst_op<false>(
                    bin.dst_hi,
                    pure(ctx.builder.CreateShuffleVector(mul, {1, 3, 5, 7}))
                  );
                  co_yield store_dst_op<false>(
                    bin.dst_low,
                    pure(ctx.builder.CreateShuffleVector(mul, {0, 2, 4, 6}))
                  );
                  co_return {};
                });
                break;
              }
              case IntegerBinaryOpWithTwoDst::UAddCarry:
              case IntegerBinaryOpWithTwoDst::USubBorrow: {
                assert(0 && "todo");
                break;
              }
              case IntegerBinaryOpWithTwoDst::UDiv: {
                effect << make_effect_bind([=](struct context ctx) -> IREffect {
                  auto a = co_yield load_src_op<false>(bin.src0);
                  auto b = co_yield load_src_op<false>(bin.src1);
                  co_yield store_dst_op<false>(
                    bin.dst_quot, make_irvalue([=](struct context ctx) {
                      return ctx.builder.CreateUDiv(a, b);
                    })
                  );
                  co_yield store_dst_op<false>(
                    bin.dst_rem, make_irvalue([=](struct context ctx) {
                      return ctx.builder.CreateURem(a, b);
                    })
                  );
                  co_return {};
                });
                break;
              }
              }
            },
            [&](InstExtractBits extract) {
              effect << store_dst_op<false>(
                extract.dst,
                make_irvalue_bind([=](struct context ctx) -> IRValue {
                  auto src0 = co_yield load_src_op<false>(extract.src0);
                  auto src1 = co_yield load_src_op<false>(extract.src1);
                  auto src2 = co_yield load_src_op<false>(extract.src2);
                  auto width = ctx.builder.CreateAnd(
                    src0, co_yield get_int4_splat(0b11111)
                  );
                  auto offset = ctx.builder.CreateAnd(
                    src1, co_yield get_int4_splat(0b11111)
                  );
                  auto width_is_zero =
                    ctx.builder.CreateICmpEQ(width, co_yield get_int4_splat(0));
                  auto width_offset_sum = ctx.builder.CreateAdd(width, offset);
                  auto width_cp =
                    ctx.builder.CreateSub(co_yield get_int4_splat(32), width);
                  auto clamp_src = ctx.builder.CreateShl(
                    src2, ctx.builder.CreateSub(
                            co_yield get_int4_splat(32), width_offset_sum
                          )
                  );
                  if (!extract.is_signed) {
                    auto need_clamp = ctx.builder.CreateICmpULT(
                      width_offset_sum, co_yield get_int4_splat(32)
                    );
                    auto no_clamp = ctx.builder.CreateLShr(src2, offset);
                    auto clamped = ctx.builder.CreateLShr(clamp_src, width_cp);
                    co_return ctx.builder.CreateSelect(
                      width_is_zero, co_yield get_int4_splat(0),
                      ctx.builder.CreateSelect(need_clamp, clamped, no_clamp)
                    );
                  } else {
                    auto need_clamp = ctx.builder.CreateICmpSLT(
                      width_offset_sum, co_yield get_int4_splat(32)
                    );
                    auto no_clamp = ctx.builder.CreateAShr(src2, offset);
                    auto clamped = ctx.builder.CreateAShr(clamp_src, width_cp);
                    co_return ctx.builder.CreateSelect(
                      width_is_zero, co_yield get_int4_splat(0),
                      ctx.builder.CreateSelect(need_clamp, clamped, no_clamp)
                    );
                  }
                })
              );
            },
            [&](InstBitFiledInsert bfi) {
              effect << store_dst_op<false>(
                bfi.dst, make_irvalue_bind([=](struct context ctx) -> IRValue {
                  auto src0 = co_yield load_src_op<false>(bfi.src0);
                  auto src1 = co_yield load_src_op<false>(bfi.src1);
                  auto src2 = co_yield load_src_op<false>(bfi.src2);
                  auto src3 = co_yield load_src_op<false>(bfi.src2);
                  auto width = ctx.builder.CreateAnd(
                    src0, co_yield get_int4_splat(0b11111)
                  );
                  auto offset = ctx.builder.CreateAnd(
                    src1, co_yield get_int4_splat(0b11111)
                  );
                  auto bitmask = ctx.builder.CreateAnd(
                    co_yield get_int4_splat(0xffffffff), // is this necessary?
                    ctx.builder.CreateShl(
                      ctx.builder.CreateSub(
                        ctx.builder.CreateLShr(
                          co_yield get_int4_splat(1), width
                        ),
                        co_yield get_int4_splat(1)
                      ),
                      offset
                    )
                  );
                  co_return ctx.builder.CreateOr(
                    ctx.builder.CreateAnd(
                      ctx.builder.CreateLShr(src2, offset), bitmask
                    ),
                    ctx.builder.CreateAnd(src3, ctx.builder.CreateNot(bitmask))
                  );
                })
              );
            },
            [&](InstAtomicBinOp bin) {
              std::string op;
              bool is_signed = false;
              switch (bin.op) {
              case AtomicBinaryOp::And:
                op = "and";
                break;
              case AtomicBinaryOp::Or:
                op = "or";
                break;
              case AtomicBinaryOp::Xor:
                op = "xor";
                break;
              case AtomicBinaryOp::Add:
                op = "add";
                break;
              case AtomicBinaryOp::IMax:
                op = "max";
                is_signed = true;
                break;
              case AtomicBinaryOp::IMin:
                op = "min";
                is_signed = true;
                break;
              case AtomicBinaryOp::UMax:
                op = "max";
                break;
              case AtomicBinaryOp::UMin:
                op = "min";
                break;
                break;
              }
              effect << std::visit(
                patterns{
                  [&](AtomicOperandTGSM tgsm) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [stride, tgsm_h] = ctx.resource.tgsm_map[tgsm.id];
                        auto addr =
                          co_yield load_src_op<false>(bin.dst_address);
                        auto ptr = ctx.builder.CreateInBoundsGEP(
                          llvm::cast<llvm::PointerType>(tgsm_h->getType())
                            ->getNonOpaquePointerElementType(),
                          tgsm_h,
                          {ctx.builder.getInt32(0),
                           builder.CreateLShr(
                             stride > 0 ? builder.CreateAdd(
                                            builder.CreateMul(
                                              builder.getInt32(stride),
                                              co_yield extract_element(0)(addr)
                                            ),
                                            co_yield extract_element(1)(addr)
                                          )
                                        : co_yield extract_element(0)(addr),
                             2
                           )}
                        );
                        auto imm = co_yield call_atomic_fetch_explicit(
                          ptr,
                          co_yield load_src_op<false>(bin.src) >>=
                          extract_element(0),
                          op, is_signed, false
                        );
                        if (bin.dst_original.has_value()) {
                          co_yield store_dst_op<false>(
                            bin.dst_original.value(), pure(imm)
                          );
                        };
                        co_return {};
                      }
                    );
                  },
                  [&](AtomicDstOperandUAV uav) {
                    return make_effect_bind(
                      [=](struct context ctx) -> IREffect {
                        auto &builder = ctx.builder;
                        auto [res, res_handle_fn, stride] =
                          ctx.resource.uav_range_map[uav.range_id];
                        assert(
                          (res.resource_kind == air::TextureKind::texture_buffer
                          ) &&
                          "TODO: handle typed uav atomic operation..."
                        );
                        auto res_h = co_yield res_handle_fn(nullptr);
                        auto addr =
                          co_yield load_src_op<false>(bin.dst_address);
                        auto ptr =
                          stride < 0
                            ? co_yield extract_element(0)(addr
                              ) // TODO: handle 2d/3d..and array uav
                            : builder.CreateLShr(
                                stride > 0
                                  ? builder.CreateAdd(
                                      builder.CreateMul(
                                        builder.getInt32(stride),
                                        co_yield extract_element(0)(addr)
                                      ),
                                      co_yield extract_element(1)(addr)
                                    )
                                  : co_yield extract_element(0)(addr),
                                2
                              );
                        auto imm = co_yield call_texture_atomic_fetch_explicit(
                          res, res_h, op, is_signed, ptr, nullptr,
                          co_yield load_src_op<false>(bin.src)
                        );
                        if (bin.dst_original.has_value()) {
                          co_yield store_dst_op<false>(
                            bin.dst_original.value(), extract_element(0)(imm)
                          );
                        };
                        co_return {};
                      }
                    );
                  }
                },
                bin.dst
              );
            },
            [&](InstSync sync) {
              mem_flags mem_flag;
              if (sync.boundary == InstSync::Boundary::global) {
                mem_flag |= mem_flags::device;
              }
              if (sync.boundary == InstSync::Boundary::group) {
                mem_flag |= mem_flags::threadgroup;
              }
              effect << call_threadgroup_barrier(mem_flag);
            },
            [&](InstPixelDiscard) { effect << call_discard_fragment(); },
            [&](InstPartialDerivative df) {
              effect << store_dst_op<true>(
                df.dst, make_irvalue_bind([=](struct context ctx) -> IRValue {
                          auto fvec4 = co_yield load_src_op<true>(df.src);
                          co_return co_yield call_derivative(fvec4, df.ddy);
                        }) >>= saturate(df._.saturate)
              );
            },
            [&](InstCalcLOD calc) {
              effect << store_dst_op<true>(
                calc.dst,
                make_irvalue_bind([=](struct context ctx) -> IRValue {
                  auto [res, res_handle_fn, _] =
                    ctx.resource.srv_range_map[calc.src_resource.range_id];
                  auto sampler_handle_fn =
                    ctx.resource.sampler_range_map[calc.src_sampler.range_id];
                  auto res_h = co_yield res_handle_fn(nullptr);
                  auto sampler_h = co_yield sampler_handle_fn(nullptr);
                  auto coord = co_yield load_src_op<true>(calc.src_address);
                  pvalue clamped_float = nullptr, unclamped_float = nullptr;
                  switch (res.resource_kind) {
                  case air::TextureKind::texture_1d:
                  case air::TextureKind::texture_1d_array: {
                    // MSL Spec 6.12.2 mipmaps are not supported for 1D texture
                    clamped_float = co_yield get_float(0.0f);
                    unclamped_float = co_yield get_float(0.0f);
                    break;
                  }
                  case air::TextureKind::depth_2d:
                  case air::TextureKind::texture_2d:
                  case air::TextureKind::depth_2d_array:
                  case air::TextureKind::texture_2d_array: {
                    clamped_float = co_yield call_calc_lod(
                      res, res_h, sampler_h, co_yield truncate_vec(2)(coord),
                      false
                    );
                    unclamped_float = co_yield call_calc_lod(
                      res, res_h, sampler_h, co_yield truncate_vec(2)(coord),
                      true
                    );
                    break;
                  }
                  case air::TextureKind::texture_3d:
                  case air::TextureKind::depth_cube:
                  case air::TextureKind::texture_cube:
                  case air::TextureKind::depth_cube_array:
                  case air::TextureKind::texture_cube_array: {
                    clamped_float = co_yield call_calc_lod(
                      res, res_h, sampler_h, co_yield truncate_vec(3)(coord),
                      false
                    );
                    unclamped_float = co_yield call_calc_lod(
                      res, res_h, sampler_h, co_yield truncate_vec(3)(coord),
                      true
                    );
                    break;
                  }
                  default: {
                    assert(0 && "invalid calc_lod resource type");
                  }
                  }
                  co_return ctx.builder.CreateInsertElement(
                    ctx.builder.CreateInsertElement(
                      llvm::ConstantAggregateZero::get(types._float4),
                      clamped_float, (uint64_t)0
                    ),
                    unclamped_float, 1
                  );
                }) >>= swizzle(calc.src_resource.read_swizzle)
              );
            },
            [](InstNop) {}, // nop
            [](auto) { assert(0 && "unhandled instruction"); }
          },
          inst
        );
      }
      auto bb_pop = builder.GetInsertBlock();
      builder.SetInsertPoint(bb);
      effect.build(ctx);
      std::visit(
        patterns{
          [](BasicBlockUndefined) {
            assert(0 && "unexpected undefined basicblock");
          },
          [&](BasicBlockUnconditionalBranch uncond) {
            if (visited.count(uncond.target.get()) == 0) {
              readBasicBlock(uncond.target);
            }
            auto target_bb = visited[uncond.target.get()];
            builder.CreateBr(target_bb);
          },
          [&](BasicBlockConditionalBranch cond) {
            if (visited.count(cond.true_branch.get()) == 0) {
              readBasicBlock(cond.true_branch);
            }
            if (visited.count(cond.false_branch.get()) == 0) {
              readBasicBlock(cond.false_branch);
            }
            auto target_true_bb = visited[cond.true_branch.get()];
            auto target_false_bb = visited[cond.false_branch.get()];
            auto test =
              load_condition(cond.cond.operand, cond.cond.test_nonzero)
                .build(ctx);
            builder.CreateCondBr(test, target_true_bb, target_false_bb);
          },
          [&](BasicBlockSwitch swc) {
            for (auto &[_, case_bb] : swc.cases) {
              if (visited.count(case_bb.get()) == 0) {
                readBasicBlock(case_bb);
              }
            }
            // TODO: ensure default always presented? no...
            if (visited.count(swc.case_default.get()) == 0) {
              readBasicBlock(swc.case_default);
            }
            // builder.CreateSwitch()
            assert(0 && "TODO: switch");
          },
          [&](BasicBlockReturn ret) {
            // we have done here!
            // unconditional jump to return bb
            builder.CreateBr(return_bb);
          }
        },
        current->target
      );
      builder.SetInsertPoint(bb_pop);
    };
  readBasicBlock(entry);
  assert(visited.count(entry.get()) == 1);
  return visited[entry.get()];
}

} // namespace dxmt::dxbc

// NOLINTEND(misc-definitions-in-headers)
#pragma clang diagnostic pop