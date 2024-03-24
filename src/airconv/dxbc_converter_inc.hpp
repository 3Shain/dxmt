#pragma once
#include "adt.hpp"
#include "air_type.hpp"
#include "dxbc_converter.hpp"
#include "ftl.hpp"
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
  std::unordered_map<uint32_t, IndexedIRValue> srv_range_map;
  std::unordered_map<uint32_t, IndexedIRValue> uav_range_map;
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
  return IREffect([fs = std::forward<S>(fs)](auto ctx) {
    return fs(ctx).build(ctx);
  });
}

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

auto read_float(pvalue vec4, Swizzle swizzle) {
  return bitcast_float4(vec4) >>= [=](auto bitcasted) -> IRValue {
    return make_irvalue([=](context s) {
      return s.builder.CreateExtractElement(bitcasted, swizzle.r);
    });
  };
};

IRValue read_float2(pvalue vec4, Swizzle swizzle) {
  return bitcast_float4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(bitcasted, {swizzle.r, swizzle.g});
    });
  };
};

auto read_float3(pvalue vec4, Swizzle swizzle) {
  return bitcast_float4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(
        bitcasted, {swizzle.r, swizzle.g, swizzle.b}
      );
    });
  };
};

auto read_float4(pvalue vec4, Swizzle swizzle) {
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

auto to_int_vec4(pvalue value) {
  return make_irvalue([=](context ctx) {
    auto ty = value->getType();
    auto &types = ctx.types;
    std::function<pvalue(pvalue)> convert = //
      [&](pvalue value) -> pvalue {
      if (ty == types._int4)
        return value;
      if (ty == types._float4)
        return ctx.builder.CreateBitCast(value, types._int4);
      return convert(extend_to_vec4(value).build(ctx));
      assert(0 && "unhandled value type");
    };
    return convert(value);
  });
};

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

auto get_item_in_argbuf_binding_table(uint32_t argbuf_index, uint32_t id) {
  return make_irvalue([=](context ctx) {
    auto argbuf = ctx.function->getArg(argbuf_index);
    return ctx.builder.CreateStructGEP(
      llvm::cast<llvm::PointerType>(argbuf->getType())
        ->getNonOpaquePointerElementType(),
      argbuf, id
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
  unsigned checked_component = 0;
  if (writeMask & 1) {
    mask[0] = 4 + swizzle[checked_component];
    checked_component++;
  }
  if (writeMask & 2) {
    mask[1] = 4 + swizzle[checked_component];
    checked_component++;
  }
  if (writeMask & 4) {
    mask[2] = 4 + swizzle[checked_component];
    checked_component++;
  }
  if (writeMask & 8) {
    mask[3] = 4 + swizzle[checked_component];
    checked_component++;
  }
  return mask;
}

auto load_at_alloca_array(llvm::AllocaInst *array, pvalue index) -> IRValue {
  return make_irvalue([=](context ctx) {
    // convert arg to int4
    auto ptr = ctx.builder.CreateInBoundsGEP(
      llvm::cast<llvm::ArrayType>(array->getAllocatedType()), array,
      {ctx.builder.getInt32(0), index}
    );
    return ctx.builder.CreateLoad(
      llvm::cast<llvm::ArrayType>(array->getAllocatedType())->getElementType(),
      // ctx.types._int4,
      ptr
    );
  });
};

auto store_at_alloca_array(
  llvm::AllocaInst *array, pvalue index, pvalue item_with_matched_type
) -> IREffect {
  return make_effect([=](context ctx) {
    // convert arg to int4
    auto ptr = ctx.builder.CreateInBoundsGEP(
      array->getAllocatedType(), array, {ctx.builder.getInt32(0), index}
    );
    ctx.builder.CreateStore(item_with_matched_type, ptr);
    return std::monostate();
  });
};

auto store_at_alloca_int_vec4_array(
  llvm::AllocaInst *array, pvalue index, pvalue item, uint32_t mask
) {
  return to_int_vec4(item) >>= [=](pvalue ivec4) {
    return make_effect_bind([=](context ctx) {
      return load_at_alloca_array(array, index) >>= [=](auto current) {
        auto new_value = ctx.builder.CreateShuffleVector(
          current, ivec4, (llvm::ArrayRef<int>)create_shuffle_swizzle_mask(mask)
        );
        return store_at_alloca_array(
          ctx.resource.input_register_file, index, new_value
        );
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
    return store_at_alloca_int_vec4_array(
      ctx.resource.input_register_file, const_index, arg, mask
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

// auto defineOperation(StringRef name, air::EType returnType,
//                      ArrayRef<air::EType> argumentsType)
//     -> std::function<IRValue(ArrayRef<pvalue>)> {
//   std::string name_ = name.str();
//   auto args_ = argumentsType.vec();
//   return [name_, args_, returnType](ArrayRef<pvalue> params) -> IRValue {
//     return make_irvalue([params = params.vec()](context ctx) {
//       // ctx.module.getOrInsertFunction()
//       return ctx.builder.CreateCall(nullptr, params);
//     });
//   };
// }

// auto air_dot_v4f32 = defineOperation("air.dot.v4f32", air::EType::_float,
//                                      {air::EType::float4,
//                                      air::EType::float4});

// auto air_dot_v3f32 = defineOperation("air.dot.v4f32", air::EType::_float,
//                                      {air::EType::float2,
//                                      air::EType::float2});

// auto air_dot_v2f32 = defineOperation("air.dot.v4f32", air::EType::_float,
//                                      {air::EType::float2,
//                                      air::EType::float2});

// IRValue dp4(pvalue a_v4f32, pvalue b_v4f32) {
//   return air_dot_v4f32({a_v4f32, b_v4f32});
// };

// IRValue dp3(pvalue a_v3f32, pvalue b_v3f32){

// };

// IRValue dp3(const IRValue &a_v3f32, const IRValue &b_v3f32) {
//   return lift(a_v3f32, b_v3f32, [](auto a, auto b) { return dp3(a, b); });
// };

// IRValue dp2(pvalue a_v2f32, pvalue b_v2f32) {

// };

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
              // r
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
            builder.CreateCondBr(nullptr, target_true_bb, target_false_bb);
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