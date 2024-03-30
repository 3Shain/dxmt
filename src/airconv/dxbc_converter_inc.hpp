#pragma once
#include "adt.hpp"
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
  std::unordered_map<uint32_t, IndexedIRValue> cb_range_map;
  std::unordered_map<uint32_t, IndexedIRValue> sampler_range_map;
  std::unordered_map<uint32_t, std::pair<air::MSLTexture, IndexedIRValue>>
    srv_range_map;
  std::unordered_map<uint32_t, std::pair<air::MSLTexture, IndexedIRValue>>
    uav_range_map;
  std::unordered_map<uint32_t, llvm::AllocaInst *> indexable_temp_map;
  llvm::AllocaInst *input_register_file;
  llvm::AllocaInst *output_register_file;
  llvm::AllocaInst *temp_register_file;
};

struct context {
  llvm::IRBuilder<> &builder;
  llvm::LLVMContext &llvm;
  llvm::Module &module;
  llvm::Function *function;
  io_binding_map &resource;
  air::AirType &types; // hmmm
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

// auto read_float(pvalue vec4, Swizzle swizzle) {
//   return bitcast_float4(vec4) >>= [=](auto bitcasted) -> IRValue {
//     return make_irvalue([=](context s) {
//       return s.builder.CreateExtractElement(bitcasted, swizzle.r);
//     });
//   };
// };

IRValue to_float2_swizzled(pvalue vec4, Swizzle swizzle) {
  auto bitcasted = co_yield bitcast_float4(vec4);
  co_return co_yield make_irvalue([=](context s) {
    return s.builder.CreateShuffleVector(
      bitcasted, (llvm::ArrayRef<int>){swizzle.r, swizzle.g}
    );
  });
};

auto to_float2(pvalue vec4) {
  return to_float2_swizzled(vec4, swizzle_identity);
};

auto to_float3_swizzled(pvalue vec4, Swizzle swizzle) {
  return bitcast_float4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(
        bitcasted, {swizzle.r, swizzle.g, swizzle.b}
      );
    });
  };
};

auto to_float3(pvalue vec4) {
  return to_float3_swizzled(vec4, swizzle_identity);
};

auto to_float4_swizzled(pvalue vec4, Swizzle swizzle) {
  if (swizzle == swizzle_identity) {
    return bitcast_float4(vec4);
  }
  return bitcast_float4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(
        bitcasted, {swizzle.r, swizzle.g, swizzle.b, swizzle.a}
      );
    });
  };
};

auto to_float4(pvalue vec4) {
  return to_float4_swizzled(vec4, swizzle_identity);
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
  static llvm::ArrayRef<int> shuffle_mask[4] = {
    {0, 0, 0, 0},
    {0, 1, 1, 1},
    {0, 1, 2, 2},
    {0, 1, 2, 3},
  };
  return make_irvalue([=](context ctx) {
    auto ty = value->getType();
    if (ty->isVectorTy()) {
      auto vecTy = llvm::cast<llvm::FixedVectorType>(ty);
      if (vecTy->getNumElements() == 4)
        return value;
      assert(vecTy->getNumElements() > 0);
      assert(vecTy->getNumElements() < 4);
      return ctx.builder.CreateShuffleVector(
        value, shuffle_mask[vecTy->getNumElements() - 1]
      );
    } else {
      return ctx.builder.CreateVectorSplat(4, value);
    }
  });
}

auto extend_to_int_vec4(pvalue value) {
  return make_irvalue([=](context ctx) {
    auto &types = ctx.types;
    std::function<pvalue(pvalue)> convert = //
      [&](pvalue value) -> pvalue {
      auto ty = value->getType();
      if (ty == types._int4)
        return value;
      if (ty == types._float4)
        return ctx.builder.CreateBitCast(
          value, types._int4, "cast_to_int4_by_need_"
        );
      return convert(extend_to_vec4(value).build(ctx));
      assert(0 && "unhandled value type");
    };
    return convert(value);
  });
};

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

auto load_at_alloca_array(llvm::AllocaInst *array, pvalue index) -> IRValue {
  return make_irvalue([=](context ctx) {
    auto ptr = ctx.builder.CreateInBoundsGEP(
      llvm::cast<llvm::ArrayType>(array->getAllocatedType()), array,
      {ctx.builder.getInt32(0), index}, "load_at_alloca_array_gep_"
    );
    return ctx.builder.CreateLoad(
      llvm::cast<llvm::ArrayType>(array->getAllocatedType())->getElementType(),
      ptr, "load_at_alloca_array_"
    );
  });
};

auto store_at_alloca_array(
  llvm::AllocaInst *array, pvalue index, pvalue item_with_matched_type
) -> IREffect {
  return make_effect([=](context ctx) {
    auto ptr = ctx.builder.CreateInBoundsGEP(
      array->getAllocatedType(), array, {ctx.builder.getInt32(0), index},
      "store_at_alloca_array_gep_"
    );
    ctx.builder.CreateStore(item_with_matched_type, ptr);
    return std::monostate();
  });
};

auto store_at_alloca_int_vec4_array_masked(
  llvm::AllocaInst *array, pvalue index, pvalue item, uint32_t mask,
  Swizzle swizzle
) -> IREffect {
  return extend_to_int_vec4(item) >>= [=](pvalue ivec4) {
    return make_effect_bind([=](context ctx) {
      return load_at_alloca_array(array, index) >>= [=](auto current) {
        auto new_value = ctx.builder.CreateShuffleVector(
          current, ivec4,
          (llvm::ArrayRef<int>)create_shuffle_swizzle_mask(mask, swizzle),
          "value_to_store_after_swizzle_mask_"
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
    return store_at_alloca_int_vec4_array_masked(
      ctx.resource.input_register_file, const_index, arg, mask, swizzle_identity
    );
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
      "air.dot.v" + std::to_string(dimension) + "f32",
      llvm::FunctionType::get(
        types._float, {operand_type, operand_type}, false
      ),
      att
    ));
    return ctx.builder.CreateCall(fn, {a, b});
  });
};

auto call_mad(uint32_t dimension, pvalue a, pvalue b, pvalue c) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    auto &types = ctx.types;
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
      "air.fma.v" + std::to_string(dimension) + "f32",
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
                                         : types._int2;
    auto fn = (module.getOrInsertFunction(
      "air." + op + (is_signed ? ".s" : ".u") + ".v" +
        std::to_string(dimension) + "i32",
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
      "air." + op + ".v" + std::to_string(dimension) + "f32",
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
      "air." + op + ".v" + std::to_string(dimension) + "f32",
      llvm::FunctionType::get(
        operand_type, {operand_type, operand_type}, false
      ),
      att
    ));
    return ctx.builder.CreateCall(fn, {a, b});
  });
};

auto call_abs(uint32_t dimension, pvalue fvec) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    auto &types = ctx.types;
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
      ctx.builder.getFastMathFlags().isFast()
        ? "air.fast_fabs.v" + std::to_string(dimension) + "f32"
        : "air.fabs.v" + std::to_string(dimension) + "f32",
      llvm::FunctionType::get(operand_type, {operand_type}, false), att
    ));
    return ctx.builder.CreateCall(fn, {fvec});
  });
};

auto call_sample(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler, pvalue coord,
  pvalue array_index, pvalue offset, pvalue ctrl, pvalue para1, pvalue para2
) {
  return make_irvalue([=](context ctx) {
    using namespace llvm;
    auto &context = ctx.llvm;
    auto &module = ctx.module;
    auto &types = ctx.types;
    auto att = AttributeList::get(
      context,
      {
        {0U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
        {0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
        {1U, Attribute::get(context, Attribute::AttrKind::NoCapture)},
        {1U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
        {~0U, Attribute::get(context, Attribute::AttrKind::ArgMemOnly)},
        {~0U, Attribute::get(context, Attribute::AttrKind::Convergent)},
        {~0U, Attribute::get(context, Attribute::AttrKind::NoUnwind)},
        {~0U, Attribute::get(context, Attribute::AttrKind::WillReturn)},
        {~0U, Attribute::get(context, Attribute::AttrKind::ReadOnly)},
      }
    );
    // TODO: .v4i32 and .u.v4i32
    auto fn_name = "air.sample_" + texture_type.get_air_symbol() + ".v4f32";
    auto [coord_type, offset_type] =
      texture_type.get_coord_offset_type(context);
    std::vector<llvm::Type *> args_type;
    args_type.push_back(texture_type.get_llvm_type(context));
    args_type.push_back(types._sampler->getPointerTo(2));
    args_type.push_back(coord_type);
    args_type.push_back(types._bool);
    args_type.push_back(offset_type);
    args_type.push_back(types._bool);
    args_type.push_back(types._float);
    args_type.push_back(types._float);
    args_type.push_back(types._int);
    auto return_type = StructType::get(
      context, {texture_type.get_return_type(context), ctx.types._bool}
    );
    auto fn = module.getOrInsertFunction(
      fn_name, llvm::FunctionType::get(return_type, args_type, false), att
    );
    return ctx.builder.CreateCall(
      fn,
      {
        handle,
        sampler,
        coord,
        texture_type.resource_kind == air::TextureKind::texture_1d
          ? ctx.builder.getInt1(false)
          : ctx.builder.getInt1(true),
        offset,
        ctx.builder.getInt1(false),
        ConstantFP::get(context, APFloat{0.0f}),
        ConstantFP::get(context, APFloat{0.0f}),
        ctx.builder.getInt32(0),
      }
    );
  });
}

auto call_sample_grad(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler_handle,
  pvalue coord, pvalue array_index, pvalue dpdx, pvalue dpdy, pvalue minlod,
  pvalue offset
) {}

auto call_sample_compare(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler_handle,
  pvalue coord, pvalue array_index, pvalue reference, pvalue offset,
  pvalue ctrl, pvalue para1, pvalue para2
) {}

auto call_gather(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler_handle,
  pvalue coord, pvalue array_index, pvalue offset, pvalue component
) {}

auto call_gather_compare(
  air::MSLTexture texture_type, pvalue handle, pvalue sampler_handle,
  pvalue coord, pvalue array_index, pvalue reference, pvalue offset
) {}

auto call_read(
  air::MSLTexture texture_type, pvalue handle, pvalue coord, pvalue cube_face,
  pvalue array_index, pvalue sample_index, pvalue lod
) {}

auto call_write(
  air::MSLTexture texture_type, pvalue handle, pvalue coord, pvalue cube_face,
  pvalue array_index, pvalue vec4, pvalue lod
) {}

std::function<IRValue(pvalue)>
apply_src_operand_modifier(SrcOperandCommon c, bool float_op) {
  return [=](auto ivec4) {
    return make_irvalue([=](context ctx) {
      auto ret = ctx.builder.CreateShuffleVector(
        ivec4, ivec4, create_shuffle_swizzle_mask(0b1111, c.swizzle),
        "operand_swizzle_"
      );
      if (c.abs) {
        assert(float_op);
        ret = ctx.builder.CreateBitCast(
          call_abs(4, ctx.builder.CreateBitCast(ret, ctx.types._float4))
            .build(ctx),
          ctx.types._int4
        );
      }
      if (c.neg) {
        if (float_op) {
          ret = ctx.builder.CreateBitCast(
            ctx.builder.CreateFNeg(
              ctx.builder.CreateBitCast(ret, ctx.types._float4)
            ),
            ctx.types._int4
          );
        } else {
          ret = ctx.builder.CreateNeg(ret);
        }
      }
      return ret;
    });
  };
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

auto load_src_operand(SrcOperand src, bool float_op = true) -> IRValue {
  return std::visit(
    patterns{
      [&](SrcOperandInput input) {
        return make_irvalue_bind([=](context ctx) {
                 return load_at_alloca_array(
                   ctx.resource.input_register_file,
                   ctx.builder.getInt32(input.regid)
                 );
               }) >>= apply_src_operand_modifier(input._, float_op);
      },
      [&](SrcOperandImmediate32 imm) {
        return make_irvalue([=](context ctx) {
                 return llvm::ConstantDataVector::get(ctx.llvm, imm.uvalue);
               }) >>= apply_src_operand_modifier(imm._, float_op);
      },
      [&](SrcOperandTemp temp) {
        return make_irvalue_bind([=](context ctx) {
                 return load_at_alloca_array(
                   ctx.resource.temp_register_file,
                   ctx.builder.getInt32(temp.regid)
                 );
               }) >>= apply_src_operand_modifier(temp._, float_op);
      },
      [&](SrcOperandConstantBuffer cb) {
        return make_irvalue([=](context ctx) {
                 auto cb_handle =
                   ctx.resource
                     .cb_range_map
                       [cb.rangeid](nullptr /* TODO: rangeindex for SM51*/)
                     .build(ctx);
                 assert(cb_handle);
                 auto ptr = ctx.builder.CreateGEP(
                   ctx.types._int4, cb_handle,
                   {load_operand_index(cb.regindex).build(ctx)}
                 );
                 return ctx.builder.CreateLoad(ctx.types._int4, ptr);
               }) >>= apply_src_operand_modifier(cb._, float_op);
      },
      [](auto) {
        assert(0 && "unhandled operand");
        return make_irvalue([=](context ctx) { return nullptr; });
      }
    },
    src
  );
};

auto store_dst_operand(
  DstOperand dst, IRValue &&value, Swizzle s = swizzle_identity
) -> IREffect {
  return std::visit(
    patterns{
      [&](DstOperandNull) {
        return make_effect([=, value = std::move(value)](auto ctx) mutable {
          value.build(ctx);
          return std::monostate();
        });
      },
      [&](DstOperandTemp o) {
        return make_effect_bind([=, value =
                                      std::move(value)](context ctx) mutable {
          auto to_ = value.build(ctx);
          return store_at_alloca_int_vec4_array_masked(
            ctx.resource.temp_register_file, ctx.builder.getInt32(o.regid), to_,
            o._.mask, s
          );
        });
      },
      [&](DstOperandOutput o) {
        return make_effect([=, value = std::move(value)](context ctx) mutable {
          auto to_ = value.build(ctx);
          store_at_alloca_int_vec4_array_masked(
            ctx.resource.output_register_file, ctx.builder.getInt32(o.regid),
            to_, o._.mask, s
          )
            .build(ctx);
          return std::monostate();
        });
      },
      [&](auto) {
        assert(0 && "unhandled dst operand");
        return make_effect([](auto) { return std::monostate(); });
      }
    },
    dst
  );
};

auto load_condition(SrcOperand src, bool non_zero_test) {
  return load_src_operand(src, false) >>= [=](pvalue condition_src) {
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
              effect << store_dst_operand(mov.dst, load_src_operand(mov.src));
            },
            [&](InstSample sample) {
              auto [res, res_handle_fn] =
                ctx.resource.srv_range_map[sample.src_resource.range_id];
              auto sampler_handle_fn =
                ctx.resource.sampler_range_map[sample.src_sampler.range_id];
              auto offset_const = llvm::ConstantVector::get(
                {llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[0], true}
                 ),
                 llvm::ConstantInt::get(
                   context, llvm::APInt{32, (uint64_t)sample.offsets[1], true}
                 )}
              );
              effect << lift(
                res_handle_fn(nullptr), sampler_handle_fn(nullptr),
                load_src_operand(sample.src_address) >>= to_float2,
                [=, res = res](pvalue res_h, pvalue sampler_h, pvalue coord) {
                  return store_dst_operand(
                    sample.dst,
                    call_sample(
                      res, res_h, sampler_h, coord, nullptr, offset_const,
                      nullptr, nullptr, nullptr
                    ) >>= extract_value(0),
                    sample.src_resource.read_swizzle
                  );
                }
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
              effect << store_dst_operand(
                icmp.dst,
                lift(
                  load_src_operand(icmp.src0, false),
                  load_src_operand(icmp.src1, false),
                  [=](auto a, auto b) { return cmp_integer(pred, a, b); }
                )
              );
            },
            [&](InstIntegerUnaryOp unary) {
              auto src = load_src_operand(unary.src, false);
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
              effect << store_dst_operand(unary.dst, std::move(src) >>= fn);
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
              effect << store_dst_operand(
                bin.dst, lift(
                           load_src_operand(bin.src0, false),
                           load_src_operand(bin.src1, false), fn
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
              effect << store_dst_operand(
                fcmp.dst,
                lift(
                  load_src_operand(fcmp.src0), load_src_operand(fcmp.src1),
                  [=](auto a, auto b) { return cmp_float(pred, a, b); }
                )
              );
            },
            [&](InstDotProduct dp) {
              switch (dp.dimension) {
              case 4:
                effect << lift(
                  load_src_operand(dp.src0) >>= to_float4,
                  load_src_operand(dp.src1) >>= to_float4,
                  [=](auto a, auto b) {
                    return store_dst_operand(dp.dst, call_dot_product(4, a, b));
                  }
                );
                break;
              case 3:
                effect << lift(
                  load_src_operand(dp.src0) >>= to_float3,
                  load_src_operand(dp.src1) >>= to_float3,
                  [=](auto a, auto b) {
                    return store_dst_operand(dp.dst, call_dot_product(3, a, b));
                  }
                );
                break;
              case 2:
                effect << lift(
                  load_src_operand(dp.src0) >>= to_float2,
                  load_src_operand(dp.src1) >>= to_float2,
                  [=](auto a, auto b) {
                    return store_dst_operand(dp.dst, call_dot_product(2, a, b));
                  }
                );
                break;
              default:
                assert(0 && "wrong dot product dimension");
              }
            },
            [&](InstFloatMAD mad) {
              effect << lift(
                load_src_operand(mad.src0) >>= to_float4,
                load_src_operand(mad.src1) >>= to_float4,
                load_src_operand(mad.src2) >>= to_float4,
                [=](auto a, auto b, auto c) {
                  return store_dst_operand(mad.dst, call_mad(4, a, b, c));
                }
              );
            },
            // [&](InstSignedMAD mad) {
            // effect << lift(
            //   load_src_operand(mad.src0, false),
            //   load_src_operand(mad.src1, false),
            //   load_src_operand(mad.src2, false),
            //   [=](auto a, auto b, auto c) {
            //     return store_dst_operand(mad.dst, call_mad(4, a, b, c));
            //   }
            // );
            // },
            // [&](InstUnsignedMAD mad) {
            // effect << lift(
            //   load_src_operand(mad.src0, false),
            //   load_src_operand(mad.src1, false),
            //   load_src_operand(mad.src2, false),
            //   [=](auto a, auto b, auto c) {
            //     return store_dst_operand(mad.dst, make_irvalue([=](struct
            //     context ctx) {
            //       // auto a_ext = ctx.builder.CreateZExt(Value *V, Type
            //       *DestTy)

            //     }));
            //   }
            // );
            // },
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
              effect << store_dst_operand(
                unary.dst, (load_src_operand(unary.src) >>= to_float4) >>= fn
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
              effect << store_dst_operand(
                bin.dst, lift(
                           load_src_operand(bin.src0) >>= to_float4,
                           load_src_operand(bin.src1) >>= to_float4, fn
                         )
              );
            },
            [&](InstSinCos sincos) {
              effect << store_dst_operand(
                sincos.dst_cos,
                (load_src_operand(sincos.src) >>= to_float4) >>=
                [=](auto src) { return call_float_unary_op("cos", src); }
              );

              effect << store_dst_operand(
                sincos.dst_sin,
                (load_src_operand(sincos.src) >>= to_float4) >>=
                [=](auto src) { return call_float_unary_op("sin", src); }
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
            assert(0 && "unexpect undefined basicblock");
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