#include "air_signature.hpp"
#include "air_type.hpp"
#include "airconv_error.hpp"
#include "airconv_public.h"
#include "dxbc_converter.hpp"
#include "nt/air_builder.hpp"
#include "nt/air_vertex_puller.hpp"
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

llvm::Expected<llvm::BasicBlock *> convert_basicblocks(
  BasicBlock *entry, context &ctx, llvm::BasicBlock *return_bb
) {
  auto &context = ctx.llvm;
  auto &builder = ctx.builder;
  auto function = ctx.function;
  std::unordered_map<BasicBlock *, llvm::BasicBlock *> visited;
  std::vector<std::pair<BasicBlock *, llvm::BasicBlock *>> visit_order;

  std::stack<BasicBlock *> block_to_visit;
  block_to_visit.push(entry);

  dxbc::Converter dxbc(ctx.air, ctx, ctx.resource);

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
          block_to_visit.push(uncond.target);
        },
        [&](BasicBlockConditionalBranch cond) {
          block_to_visit.push(cond.true_branch);
          block_to_visit.push(cond.false_branch);
        },
        [&](BasicBlockSwitch swc)  {
          block_to_visit.push(swc.case_default);
          for (auto &[val, case_bb] : swc.cases) {
            block_to_visit.push(case_bb);
          }
        },
        [&](BasicBlockInstanceBarrier instance)  {
          block_to_visit.push(instance.active);
          block_to_visit.push(instance.sync);
        },
        [&](BasicBlockHullShaderWriteOutput hull_end)  {
          block_to_visit.push(hull_end.epilogue);
        },
        [&](BasicBlockCall call) {
          block_to_visit.push(call.return_point);
        },
      },
      current->target
    );
  }

  auto load_condition = [&dxbc, &builder](SrcOperand src, bool non_zero_test) {
    auto element = dxbc.LoadOperand(src, kMaskComponentX);
    if (non_zero_test)
      return builder.CreateICmpNE(element, builder.getInt32(0));
    else
      return builder.CreateICmpEQ(element, builder.getInt32(0));
  };

  auto bb_pop = builder.GetInsertBlock();
  for (auto &[current, bb] : visit_order) {
    builder.SetInsertPoint(bb);

    current->instructions.for_each(dxbc);

    if (auto err = std::visit(
          patterns{
            [](BasicBlockUndefined) -> llvm::Error {
              return llvm::make_error<UnsupportedFeature>(
                "unexpected undefined basicblock"
              );
            },
            [&](BasicBlockUnconditionalBranch uncond) -> llvm::Error {
              auto target_bb = visited[uncond.target];
              builder.CreateBr(target_bb);
              return llvm::Error::success();
            },
            [&](BasicBlockConditionalBranch cond) -> llvm::Error {
              auto target_true_bb = visited[cond.true_branch];
              auto target_false_bb = visited[cond.false_branch];
              auto test = load_condition(cond.cond.operand, cond.cond.test_nonzero);
              builder.CreateCondBr(test, target_true_bb, target_false_bb);
              return llvm::Error::success();
            },
            [&](BasicBlockSwitch swc) -> llvm::Error {
              auto value = dxbc.LoadOperand(swc.value, kMaskComponentX);
              auto switch_inst = builder.CreateSwitch(
                value, visited[swc.case_default], swc.cases.size()
              );
              for (auto &[val, case_bb] : swc.cases) {
                switch_inst->addCase(
                  llvm::ConstantInt::get(context, llvm::APInt(32, val)),
                  visited[case_bb]
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
              auto target_true_bb = visited[instance.active];
              auto target_false_bb = visited[instance.sync];
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

              auto src = ctx.resource.hull_cp_passthrough_src;
              if (src->getType()->getNonOpaquePointerElementType() != ctx.resource.hull_cp_passthrough_type) {
                src = builder.CreateBitCast(
                    src, ctx.resource.hull_cp_passthrough_type->getPointerTo(src->getType()->getPointerAddressSpace())
                );
              }
              assert(src->getType()->getNonOpaquePointerElementType() == ctx.resource.hull_cp_passthrough_type);
              builder.CreateStore(
                  builder.CreateLoad(ctx.resource.hull_cp_passthrough_type, src), ctx.resource.hull_cp_passthrough_dst
              );

              builder.CreateBr(sync);
              builder.SetInsertPoint(sync);
              ctx.air.CreateBarrier(llvm::air::MemFlags::Threadgroup);

              auto target_bb = visited[hull_end.epilogue];
              builder.CreateBr(target_bb);
              return llvm::Error::success();
            },
            [](BasicBlockCall) -> llvm::Error {
              return llvm::make_error<UnsupportedFeature>(
                "call terminator must be lowered"
              );
            },
          },
          current->target
        )) {
      return err;
    };
  };
  assert(visited.count(entry) == 1);
  builder.SetInsertPoint(bb_pop);
  return visited[entry];
}

} // namespace dxmt::dxbc
