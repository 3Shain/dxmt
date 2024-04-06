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

struct io_binding_map {
  llvm::GlobalVariable *icb;
  llvm::Value *icb_float;
  std::unordered_map<uint32_t, IndexedIRValue> cb_range_map;
  std::unordered_map<uint32_t, IndexedIRValue> sampler_range_map;
  std::unordered_map<uint32_t, std::pair<air::MSLTexture, IndexedIRValue>>
    srv_range_map;
  std::unordered_map<uint32_t, std::pair<air::MSLTexture, IndexedIRValue>>
    uav_range_map;
  std::unordered_map<uint32_t, llvm::AllocaInst *> indexable_temp_map;
  llvm::AllocaInst *input_register_file = nullptr;
  llvm::Value *input_register_file_float = nullptr;
  llvm::AllocaInst *output_register_file = nullptr;
  llvm::Value *output_register_file_float = nullptr;
  llvm::AllocaInst *temp_register_file = nullptr;
  llvm::Value *temp_register_file_float = nullptr;
  // TODO: tgsm

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
  //   return ReaderIO<context, IRValue>(std::forward<S>(fs)) >>=
  //          [](auto inside) { return inside; };
  return IRValue([fs = std::forward<S>(fs)](auto ctx) {
    return fs(ctx).build(ctx);
  });
}

template <typename S> IREffect make_effect(S &&fs) {
  return IREffect(std::forward<S>(fs));
}

template <typename S> IREffect make_effect_bind(S &&fs) {
  //   return ReaderIO<context, IREffect>(std::forward<S>(fs)) >>=
  //          [](auto inside) { return inside; };
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
    return make_irvalue([=](context ctx) {
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
    return make_irvalue([=](context ctx) {
      return ctx.builder.CreateShuffleVector(
        vec, {swizzle.x, swizzle.y, swizzle.z, swizzle.w}
      );
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

auto create_shuffle_swizzle_mask(
  uint32_t writeMask, Swizzle swizzle_ = swizzle_identity
) {
  // original value: 0 1 2 3
  // new value: 4 5 6 7
  // if writeMask at specific component is 0, use original value
  std::array<int, 4> mask = {0, 1, 2, 3}; // identity selection
  char *swizzle = (char *)&swizzle_;
  // unsigned checked_component = 0;
  if (writeMask & 1) {
    mask[0] = 4 + swizzle[0];
    // checked_component++;
  }
  if (writeMask & 2) {
    mask[1] = 4 + swizzle[1];
    // checked_component++;
  }
  if (writeMask & 4) {
    mask[2] = 4 + swizzle[2];
    // checked_component++;
  }
  if (writeMask & 8) {
    mask[3] = 4 + swizzle[3];
    // checked_component++;
  }
  return mask;
}

/* it should return sign extended uint4 (all bits 0 or all bits 1) */
auto cmp_integer(llvm::CmpInst::Predicate cmp, pvalue a, pvalue b) {
  return make_irvalue([=](context ctx) {
    return ctx.builder.CreateSExt(
      ctx.builder.CreateICmp(llvm::CmpInst::ICMP_EQ, a, b), ctx.types._int4
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

auto load_at_alloca_array(llvm::Value *array, pvalue index) -> IRValue {
  return make_irvalue([=](context ctx) {
    auto ptr = ctx.builder.CreateInBoundsGEP(
      llvm::cast<llvm::PointerType>(array->getType())
        ->getNonOpaquePointerElementType(),
      array, {ctx.builder.getInt32(0), index}
    );
    return ctx.builder.CreateLoad(
      llvm::cast<llvm::ArrayType>(llvm::cast<llvm::PointerType>(array->getType()
                                  )
                                    ->getNonOpaquePointerElementType())
        ->getElementType(),
      ptr
    );
  });
};

auto store_at_alloca_array(
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
  llvm::Value *array, pvalue index, pvalue item, uint32_t mask
) -> IREffect {
  return extend_to_vec4(item) >>= [=](pvalue vec4) {
    return make_effect_bind([=](context ctx) {
      if (mask == 0b1111) {
        return store_at_alloca_array(array, index, vec4);
      }
      return load_at_alloca_array(array, index) >>= [=](auto current) {
        assert(current->getType() == vec4->getType());
        auto new_value = ctx.builder.CreateShuffleVector(
          current, vec4,
          (llvm::ArrayRef<int>)
            create_shuffle_swizzle_mask(mask, swizzle_identity)
        );
        return store_at_alloca_array(array, index, new_value);
      };
    });
  };
};

auto init_input_reg(uint32_t with_fnarg_at, uint32_t to_reg, uint32_t mask)
  -> IREffect {
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
        ctx.resource.input_register_file, const_index, arg, mask
      );
    } else if (arg->getType()->getScalarType()->isFloatTy()) {
      return store_at_vec4_array_masked(
        ctx.resource.input_register_file_float, const_index, arg, mask
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
      return load_at_alloca_array(
               ctx.resource.output_register_file, const_index
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

auto call_float_mad(uint32_t dimension, pvalue a, pvalue b, pvalue c) {
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
    auto &types = ctx.types;
    assert(a->getType()->getScalarType()->isIntegerTy());
    auto dimension = get_value_dimension(a);
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
    auto operand_type = dimension == 4   ? types._int4
                        : dimension == 3 ? types._int3
                                         : types._int2;
    auto fn = (module.getOrInsertFunction(
      "air." + op + ".v" + std::to_string(dimension) + "i32",
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
    auto &types = ctx.types;
    assert(a->getType()->getScalarType()->isIntegerTy());
    assert(b->getType()->getScalarType()->isIntegerTy());
    assert(a->getType() == b->getType());
    auto dimension = get_value_dimension(a);
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
    auto operand_type = dimension == 4   ? types._int4
                        : dimension == 3 ? types._int3
                        : dimension == 2 ? types._int2
                                         : types._int;
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
    auto &types = ctx.types;
    assert(a->getType()->getScalarType()->isFloatTy());
    auto dimension = get_value_dimension(a);
    auto att = AttributeList::get(
      context, {{~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
                {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
                {~0U, Attribute::get(context, Attribute::AttrKind::ReadNone)}}
    );
    auto operand_type = dimension == 4   ? types._float4
                        : dimension == 3 ? types._float3
                        : dimension == 2 ? types._float2
                                         : types._float;
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
    auto &types = ctx.types;
    assert(a->getType()->getScalarType()->isFloatTy());
    assert(b->getType()->getScalarType()->isFloatTy());
    assert(a->getType() == b->getType());
    auto dimension = get_value_dimension(a);
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
    auto operand_type = dimension == 4   ? types._float4
                        : dimension == 3 ? types._float3
                                         : types._float2;
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

auto call_abs(uint32_t dimension, pvalue fvec) {
  return call_float_unary_op("fabs", fvec);
};

auto pure(pvalue p) {
  return make_irvalue([=](auto) { return p; });
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
  air::MSLTexture texture_type, pvalue handle, pvalue coord, pvalue cube_face,
  pvalue array_index, pvalue sample_index, pvalue lod
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

  args_type.push_back(op_info.coord_type);
  args_value.push_back(coord);

  if (op_info.is_cube) {
    args_type.push_back(types._int);
    args_value.push_back(cube_face);
  }

  if (op_info.is_array) {
    args_type.push_back(types._int);
    args_value.push_back(array_index);
  }

  if (op_info.is_ms) {
    args_type.push_back(types._int);
    args_value.push_back(sample_index);
  }

  if (!op_info.is_ms && texture_type.resource_kind != air::TextureKind::texture_buffer) {
    args_type.push_back(types._int);
    if (op_info.is_1d_or_1d_array) {
      args_value.push_back(ctx.builder.getInt32(0));
    } else {
      args_value.push_back(lod);
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
  air::MSLTexture texture_type, pvalue handle, pvalue coord, pvalue cube_face,
  pvalue array_index, pvalue vec4, pvalue lod
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
  args_value.push_back(coord);

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
      args_value.push_back(lod);
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
    llvm::FunctionType::get(
      Type::getVoidTy(context), {Type::getVoidTy(context)}, false
    ),
    att
  ));
  ctx.builder.CreateCall(fn, {});
  co_return {};
}

enum class mem_flags : uint32_t { device = 1, threadgroup = 2, texture = 4 };

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
    context, {{0U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
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

IRValue call_texture_atomic_fetch(
  air::MSLTexture texture_type, pvalue handle, pvalue operand, std::string op,
  bool is_signed = false
) {
  assert(0 && "TODO");
};

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

IRValue
apply_float_src_operand_modifier(SrcOperandCommon c, llvm::Value *fvec4) {
  auto ctx = co_yield get_context();
  assert(fvec4->getType() == ctx.types._float4);

  auto ret = fvec4;
  if (c.swizzle != swizzle_identity) {
    ret = ctx.builder.CreateShuffleVector(
      ret, {c.swizzle.x, c.swizzle.y, c.swizzle.z, c.swizzle.w}
    );
  }
  if (c.abs) {
    ret = co_yield call_abs(4, ret);
  }
  if (c.neg) {
    ret = ctx.builder.CreateFNeg(ret);
  }
  co_return ret;
};

IRValue
apply_integer_src_operand_modifier(SrcOperandCommon c, llvm::Value *ivec4) {
  auto ctx = co_yield get_context();
  assert(ivec4->getType() == ctx.types._int4);
  auto ret = ivec4;
  if (c.swizzle != swizzle_identity) {
    ret = ctx.builder.CreateShuffleVector(
      ret, {c.swizzle.x, c.swizzle.y, c.swizzle.z, c.swizzle.w}
    );
  }
  if (c.abs) {
    assert(0 && "otherwise dxbc spec is wrong");
  }
  if (c.neg) {
    ret = ctx.builder.CreateNeg(ret);
  }
  co_return ret;
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
            load_at_alloca_array(
              ctx.resource.temp_register_file, ctx.builder.getInt32(ot.regid)
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

template <bool ReadFloat> IRValue load_src_op(SrcOperand src) {
  return std::visit(
    [](auto src) { return load_src<decltype(src), ReadFloat>(src); }, src
  );
};

template <>
IRValue load_src<SrcOperandConstantBuffer, false>(SrcOperandConstantBuffer cb) {
  auto ctx = co_yield get_context();
  auto cb_handle = co_yield ctx.resource.cb_range_map[cb.rangeid](nullptr);
  assert(cb_handle);
  auto ptr = ctx.builder.CreateGEP(
    ctx.types._int4, cb_handle, {load_operand_index(cb.regindex).build(ctx)}
  );
  co_return co_yield apply_integer_src_operand_modifier(
    cb._, ctx.builder.CreateLoad(ctx.types._int4, ptr)
  );
};

template <>
IRValue load_src<SrcOperandConstantBuffer, true>(SrcOperandConstantBuffer cb) {
  auto ctx = co_yield get_context();
  auto cb_handle = co_yield ctx.resource.cb_range_map[cb.rangeid](nullptr);
  assert(cb_handle);
  auto ptr = ctx.builder.CreateGEP(
    ctx.types._int4, cb_handle, {load_operand_index(cb.regindex).build(ctx)}
  );
  auto vec = ctx.builder.CreateBitCast(
    ctx.builder.CreateLoad(ctx.types._int4, ptr), ctx.types._float4
  );
  co_return co_yield apply_float_src_operand_modifier(cb._, vec);
};

template <>
IRValue load_src<SrcOperandImmediateConstantBuffer, false>(
  SrcOperandImmediateConstantBuffer cb
) {
  auto ctx = co_yield get_context();
  auto vec = co_yield load_at_alloca_array(
    ctx.resource.icb, co_yield load_operand_index(cb.regindex)
  );
  co_return co_yield apply_integer_src_operand_modifier(cb._, vec);
};

template <>
IRValue load_src<SrcOperandImmediateConstantBuffer, true>(
  SrcOperandImmediateConstantBuffer cb
) {
  auto ctx = co_yield get_context();
  auto vec = co_yield load_at_alloca_array(
    ctx.resource.icb_float, co_yield load_operand_index(cb.regindex)
  );
  co_return co_yield apply_float_src_operand_modifier(cb._, vec);
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
  auto s = co_yield load_at_alloca_array(
    ctx.resource.temp_register_file_float, ctx.builder.getInt32(temp.regid)
  );
  co_return co_yield apply_float_src_operand_modifier(temp._, s);
};

template <> IRValue load_src<SrcOperandTemp, false>(SrcOperandTemp temp) {
  auto ctx = co_yield get_context();
  auto s = co_yield load_at_alloca_array(
    ctx.resource.temp_register_file, ctx.builder.getInt32(temp.regid)
  );
  co_return (co_yield apply_integer_src_operand_modifier(temp._, s));
};

template <> IRValue load_src<SrcOperandInput, true>(SrcOperandInput input) {
  auto ctx = co_yield get_context();
  auto s = co_yield load_at_alloca_array(
    ctx.resource.input_register_file_float, ctx.builder.getInt32(input.regid)
  );
  co_return co_yield apply_float_src_operand_modifier(input._, s);
};

template <> IRValue load_src<SrcOperandInput, false>(SrcOperandInput input) {
  auto ctx = co_yield get_context();
  auto s = co_yield load_at_alloca_array(
    ctx.resource.input_register_file, ctx.builder.getInt32(input.regid)
  );
  co_return co_yield apply_integer_src_operand_modifier(input._, s);
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
  co_return co_yield apply_integer_src_operand_modifier(attr._, vec);
};

template <>
IRValue load_src<SrcOperandAttribute, true>(SrcOperandAttribute attr) {
  auto ctx = co_yield get_context();
  pvalue vec;
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
  co_return co_yield apply_float_src_operand_modifier(attr._, vec);
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
        ctx.resource.temp_register_file, co_yield get_int(temp.regid),
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
        ctx.resource.temp_register_file_float, co_yield get_int(temp.regid),
        co_yield std::move(value), temp._.mask
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
        ctx.resource.output_register_file, co_yield get_int(output.regid),
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
        ctx.resource.output_register_file_float, co_yield get_int(output.regid),
        co_yield std::move(value), output._.mask
      );
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

auto convertBasicBlocks(
  std::shared_ptr<BasicBlock> entry, context &ctx, llvm::BasicBlock *return_bb
) {
  auto &context = ctx.llvm;
  auto &builder = ctx.builder;
  auto function = ctx.function;
  std::unordered_map<BasicBlock *, llvm::BasicBlock *> visited;
  std::function<void(std::shared_ptr<BasicBlock>)> readBasicBlock =
    [&](std::shared_ptr<BasicBlock> current) {
      auto bb = llvm::BasicBlock::Create(context, "", function);
      assert(visited.insert({current.get(), bb}).second);
      air::AirType types(context);
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
                   auto [res, res_handle_fn] =
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
                   auto [res, res_handle_fn] =
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
                   auto [res, res_handle_fn] =
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
                   auto [res, res_handle_fn] =
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
                   auto [res, res_handle_fn] =
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
                       co_yield truncate_vec(2)(coord),        // .rg
                       co_yield extract_element(2)(coord),     // array_index
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
                       co_yield truncate_vec(3)(coord),        // .rgb
                       co_yield extract_element(3)(coord),     // array_index
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
                 }) >>= extract_value(0)) >>=
                swizzle(sample.src_resource.read_swizzle)
              );
            },
            [&](InstGather sample) {
              effect << store_dst_op<true>(
                sample.dst,
                (make_irvalue_bind([=](struct context ctx) -> IRValue {
                   auto [res, res_handle_fn] =
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
                   auto [res, res_handle_fn] =
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
            [&](InstStoreUAVTyped store) {
              effect << make_effect_bind([=](struct context ctx) -> IREffect {
                auto [res, res_handle_fn] =
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

                // case air::TextureKind::texture_buffer:
                //   co_yield call_write(
                //     res, res_h, co_yield extract_element(0)(coord), nullptr,
                //     nullptr, value, ctx.builder.getInt32(0)
                //   );
                //   break;
                case air::TextureKind::texture_cube:
                case air::TextureKind::texture_cube_array:
                default:
                  assert(0 && "invalid texture kind for uav store");
                }
                co_return {};
              });
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
              effect << store_dst_op<false>(
                unary.dst, load_src_op<false>(unary.src) >>= fn
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
              effect << store_dst_op<false>(
                bin.dst,
                lift(
                  load_src_op<false>(bin.src0), load_src_op<false>(bin.src1), fn
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
              effect << lift(
                load_src_op<true>(mad.src0), load_src_op<true>(mad.src1),
                load_src_op<true>(mad.src2),
                [=](auto a, auto b, auto c) {
                  return store_dst_op<true>(
                    mad.dst,
                    call_float_mad(4, a, b, c) >>= saturate(mad._.saturate)
                  );
                }
              );
            },
            [&](InstIntegerMAD mad) {
              // FIXME: this looks wrong, what's the sign specific behavior?
              effect << lift(
                load_src_op<false>(mad.src0), load_src_op<false>(mad.src1),
                load_src_op<false>(mad.src2),
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
              effect << store_dst_op<true>(
                unary.dst, (load_src_op<true>(unary.src) >>= fn) >>=
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
              effect << store_dst_op<true>(
                bin.dst,
                lift(
                  load_src_op<true>(bin.src0), load_src_op<true>(bin.src1), fn
                ) >>= saturate(bin._.saturate)
              );
            },
            [&](InstSinCos sincos) {
              effect << store_dst_op<true>(
                sincos.dst_cos, load_src_op<true>(sincos.src) >>=
                                [=](auto src) {
                                  return call_float_unary_op("cos", src) >>=
                                         saturate(sincos._.saturate);
                                }
              );

              effect << store_dst_op<true>(
                sincos.dst_sin, load_src_op<true>(sincos.src) >>=
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
            // [&](InstIntegerBinaryOpWithTwoDst bin) {
            //   switch (bin.op) {
            //   case IntegerBinaryOpWithTwoDst::IMul:
            //   case IntegerBinaryOpWithTwoDst::UMul: {
            //     assert(0 && "todo");
            //     break;
            //   }
            //   case IntegerBinaryOpWithTwoDst::UAddCarry:
            //   case IntegerBinaryOpWithTwoDst::USubBorrow:
            //   case IntegerBinaryOpWithTwoDst::UDiv:
            //     assert(0 && "todo");
            //     break;
            //   }
            // },
            // [&](InstSync sync) {
            //   mem_flags mem_flag;
            //   if (sync.boundary == InstSync::Boundary::global) {
            //     // mem_flag |= mem_flags::device;
            //   }
            //   if (sync.boundary == InstSync::Boundary::group) {
            //     // mem_flag |= mem_flags::threadgroup;
            //   }
            //   effect << call_threadgroup_barrier(mem_flag);
            // },
            [&](InstPixelDiscard) { effect << call_discard_fragment(); },
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