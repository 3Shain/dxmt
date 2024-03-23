#pragma once
#include "adt.hpp"
#include "air_type.hpp"
#include "dxbc_converter.hpp"
#include "ftl.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <cstdint>
#include <functional>
#include <unordered_map>
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
using IREffect = ReaderIO<context, void>;
using IndexedIRValue = std::function<IRValue(pvalue)>;

struct resource_binding_map {
  std::unordered_map<uint32_t, IndexedIRValue> cbRangeMap;
  std::unordered_map<uint32_t, IndexedIRValue> samplerRangeMap;
  std::unordered_map<uint32_t, IndexedIRValue> srvRangeMap;
  std::unordered_map<uint32_t, IndexedIRValue> uavRangeMap;
};

struct context {
  llvm::IRBuilder<> &builder;
  llvm::Module &module;
  llvm::LLVMContext &llvm;
  llvm::Function *function;
  air::AirType &types; // hmmm
  resource_binding_map &resource;
};

template <typename S> IRValue make_irvalue(S &&fs) {
  return IRValue(std::forward<S>(fs));
}

template <typename S> IREffect make_effect(S &&fs) {
  return IREffect(std::forward<S>(fs));
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
  std::shared_ptr<BasicBlock> entry, llvm::LLVMContext &context,
  llvm::Module &module, llvm::Function *function, resource_binding_map &resource
) {

  std::unordered_map<BasicBlock *, llvm::BasicBlock *> visited;
  std::function<void(std::shared_ptr<BasicBlock>)> readBasicBlock =
    [&](std::shared_ptr<BasicBlock> current) {
      auto llvm_bb = llvm::BasicBlock::Create(context, "", function);
      assert(visited.insert({current.get(), llvm_bb}).second);
      llvm::IRBuilder<> builder(llvm_bb);
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
      std::visit(
        patterns{
          [](BasicBlockUndefined) {
            assert(0 && "unexpect undefined basicblock");
          },
          [&](BasicBlockUnconditionalBranch uncond) {
            if (visited.count(uncond.target.get()) == 0) {
              readBasicBlock(uncond.target);
            }
          },
          [&](BasicBlockConditionalBranch cond) {
            if (visited.count(cond.true_branch.get()) == 0) {
              readBasicBlock(cond.true_branch);
            }
            if (visited.count(cond.false_branch.get()) == 0) {
              readBasicBlock(cond.false_branch);
            }
          },
          [&](BasicBlockSwitch swc) {
            for (auto &[_, case_bb] : swc.cases) {
              if (visited.count(case_bb.get()) == 0) {
                readBasicBlock(case_bb);
              }
            }
            if (visited.count(swc.case_default.get()) == 0) {
              readBasicBlock(swc.case_default);
            }
          },
          [&](BasicBlockReturn ret) {
            // we have done here!
          }
        },
        current->target
      );
    };
}

} // namespace dxmt::dxbc

// NOLINTEND(misc-definitions-in-headers)
#pragma clang diagnostic pop