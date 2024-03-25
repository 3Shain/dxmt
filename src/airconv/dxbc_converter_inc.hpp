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
    return s.builder.CreateBitCast(
      vec4, s.types._float4, "cast_to_float4_by_need_"
    );
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

IRValue to_float2_swizzled(pvalue vec4, Swizzle swizzle) {
  return bitcast_float4(vec4) >>= [=](auto bitcasted) {
    return make_irvalue([=](context s) {
      return s.builder.CreateShuffleVector(
        bitcasted, (llvm::ArrayRef<int>){swizzle.r, swizzle.g}
      );
    });
  };
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
  llvm::AllocaInst *array, pvalue index, pvalue item, uint32_t mask
) -> IREffect {
  return extend_to_int_vec4(item) >>= [=](pvalue ivec4) {
    return make_effect_bind([=](context ctx) {
      return load_at_alloca_array(array, index) >>= [=](auto current) {
        auto new_value = ctx.builder.CreateShuffleVector(
          current, ivec4,
          (llvm::ArrayRef<int>)create_shuffle_swizzle_mask(mask),
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
          ret = ctx.builder.CreateNeg(
            ctx.builder.CreateBitCast(ret, ctx.types._float4)
          );
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

auto store_dst_operand(DstOperand dst, IRValue &&value) -> IREffect {
  return std::visit(
    patterns{
      [&](DstOperandNull) {
        return make_effect([=](auto ctx) {
          value.build(ctx);
          return std::monostate();
        });
      },
      [&](DstOperandTemp o) {
        return make_effect_bind([=](context ctx) {
          auto to_ = value.build(ctx);
          return store_at_alloca_int_vec4_array_masked(
            ctx.resource.temp_register_file, ctx.builder.getInt32(o.regid), to_,
            o._.mask
          );
        });
      },
      [&](DstOperandOutput o) {
        return make_effect([=](context ctx) {
          auto to_ = value.build(ctx);
          store_at_alloca_int_vec4_array_masked(
            ctx.resource.output_register_file, ctx.builder.getInt32(o.regid),
            to_, o._.mask
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
            [&](InstFloatBinaryOp bin) {
              switch (bin.op) {
              case FloatBinaryOp::Add: {
                effect << lift(
                  load_src_operand(bin.src0) >>= to_float4,
                  load_src_operand(bin.src1) >>= to_float4,
                  [=](auto a, auto b) {
                    return store_dst_operand(
                      bin.dst, make_irvalue([=](struct context ctx) {
                        return ctx.builder.CreateFAdd(a, b);
                      })
                    );
                  }
                );
                break;
              }
              case FloatBinaryOp::Mul:
              case FloatBinaryOp::Div:
              case FloatBinaryOp::Min:
              case FloatBinaryOp::Max:
              default:
                assert(0 && "TODO: implement float binop");
              }
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