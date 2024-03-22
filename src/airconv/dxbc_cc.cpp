
#include "air_type.hpp"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include <cstdint>
#include <utility>

#include "dxbc_instruction.hpp"
#include "ftl.hpp"

using namespace llvm;

namespace dxmt {

using pvalue = Value *;
using dxbc::swizzle;
using dxbc::swizzle_identity;

struct context {
  IRBuilder<> &builder;
  Module &module;
  LLVMContext &llvm;
  Function *function;
  air::AirType &types; // hmmm
};

using unit_t = std::tuple<>;
auto unit = std::tuple<>();
using IRValue = ReaderIO<context, pvalue>;
using IREffect = ReaderIO<context, unit_t>;
using IRBasicBlock = ReaderIO<context, BasicBlock *>;

template <typename S> IRValue make_irvalue(S &&fs) {
  return IRValue(std::forward<S>(fs));
}

template <typename S> IREffect make_effect(S &&fs) {
  return IREffect(std::forward<S>(fs));
}

#pragma region effects

auto createBasicBlock(const Twine &name) {
  return IRBasicBlock([=](context ctx) {
    return BasicBlock::Create(ctx.llvm, name, ctx.function);
  });
}

auto setInsertPoint(IRBasicBlock bb) -> IREffect {
  return bb >>= [=](auto bb) {
    return make_effect([=](context ctx) {
      ctx.builder.SetInsertPoint(bb);
      return unit;
    });
  };
};

auto br_cond(IRValue &condition, IRBasicBlock &_true, IRBasicBlock &_false) {
  return condition >>= [=](auto condition) {
    return _true >>= [=](auto _true) {
      return _false >>= [=](auto _false) {
        return make_effect([=](context ctx) {
          ctx.builder.CreateCondBr(condition, _true, _false);
          return unit;
        });
      };
    };
  };
};

auto br_uncond(IRBasicBlock &_true) {
  return _true >>= [=](auto _true) {
    return make_effect([=](context ctx) {
      ctx.builder.CreateBr(_true);
      return unit;
    });
  };
};

auto returnValue(IRValue &value) {
  return value >>= [](auto value) {
    return make_effect([=](context ctx) {
      ctx.builder.CreateRet(value);
      return unit;
    });
  };
}

auto returnVoid() {
  return make_effect([=](context ctx) {
    ctx.builder.CreateRetVoid();
    return unit;
  });
}

#pragma endregion

auto bitcastFloat4(pvalue vec4) {
  return make_irvalue([=](context s) {
    if (vec4->getType() == s.types._float4)
      return vec4;
    return s.builder.CreateBitCast(vec4, s.types._float4);
  });
}

auto bitcastInt4(pvalue vec4) {
  return make_irvalue([=](context s) {
    if (vec4->getType() == s.types._int4)
      return vec4;
    return s.builder.CreateBitCast(vec4, s.types._int4);
  });
}

auto readFloat(pvalue vec4, swizzle swizzle) {
  return bitcastFloat4(vec4) >>= [=](auto bitcasted) -> IRValue {
    return make_irvalue([=](context s) {
      return s.builder.CreateExtractElement(bitcasted, swizzle.r);
    });
  };
};

IRValue readFloat2(pvalue vec4, swizzle swizzle) {
  return bitcastFloat4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(bitcasted, {swizzle.r, swizzle.g});
    });
  };
};

auto readFloat3(pvalue vec4, swizzle swizzle) {
  return bitcastFloat4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(bitcasted,
                                           {swizzle.r, swizzle.g, swizzle.b});
    });
  };
};

auto readFloat4(pvalue vec4, swizzle swizzle) {
  if (swizzle == swizzle_identity) {
    return bitcastFloat4(vec4);
  }
  return bitcastFloat4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(
          bitcasted, {swizzle.r, swizzle.g, swizzle.b, swizzle.a});
    });
  };
};

auto readInt(pvalue vec4, swizzle swizzle) {
  return bitcastInt4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateExtractElement(bitcasted, swizzle.r);
    });
  };
};

auto readInt2(pvalue vec4, swizzle swizzle) {
  return bitcastInt4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(bitcasted, {swizzle.r, swizzle.g});
    });
  };
};

auto readInt3(pvalue vec4, swizzle swizzle) {
  return bitcastInt4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(bitcasted,
                                           {swizzle.r, swizzle.g, swizzle.b});
    });
  };
};

auto readInt4(pvalue vec4, swizzle swizzle) {
  if (swizzle == swizzle_identity) {
    return bitcastInt4(vec4);
  }
  return bitcastInt4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(
          bitcasted, {swizzle.r, swizzle.g, swizzle.b, swizzle.a});
    });
  };
};

auto getInt(uint32_t value) -> IRValue {
  return make_irvalue([=](context ctx) { return ctx.builder.getInt32(value); });
}

auto getInt4(uint32_t value) -> IRValue {
  return make_irvalue([=](context ctx) { return ctx.builder.getInt32(value); });
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

} // namespace dxmt

using range_id = uint32_t;

namespace dxmt::dxbc {} // namespace dxmt::dxbc