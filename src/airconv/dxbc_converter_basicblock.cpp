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

auto
load_condition(SrcOperand src, bool non_zero_test) {
  return make_irvalue([=](context ctx) {
    dxbc::Converter dxbc(ctx.air, ctx, ctx.resource);
    auto element = dxbc.LoadOperand(src, kMaskComponentX);
    if (non_zero_test) {
      return ctx.builder.CreateICmpNE(element, ctx.builder.getInt32(0));
    } else {
      return ctx.builder.CreateICmpEQ(element, ctx.builder.getInt32(0));
    }
  });
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
    builder.SetInsertPoint(bb);

    dxbc::Converter dxbc(ctx.air, ctx, ctx.resource);
    current->instructions.for_each(dxbc);

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
              dxbc::Converter dxbc(ctx.air, ctx, ctx.resource);
              auto value = dxbc.LoadOperand(swc.value, kMaskComponentX);
              auto switch_inst = builder.CreateSwitch(
                value, visited[swc.case_default.get()], swc.cases.size()
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

              builder.CreateStore(
                builder.CreateLoad(
                  ctx.resource.hull_cp_passthrough_type,
                  ctx.resource.hull_cp_passthrough_src
              ), ctx.resource.hull_cp_passthrough_dst);

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