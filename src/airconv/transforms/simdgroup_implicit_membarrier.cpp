#include "simdgroup_implicit_membarrier.hpp"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "../nt/air_builder.hpp"

namespace llvm::air {

PreservedAnalyses
SimdgroupImplicitMemBarrierPass::run(Function &F, FunctionAnalysisManager &AM) {
  if (!F.getParent()->getNamedMetadata("air.kernel"))
    return PreservedAnalyses::all();

  bool Changed = false;

  SmallVector<LoadInst *, 4> ToCheck;
  SmallDenseSet<Instruction *, 4> Fenced;

  auto &MemSSA = AM.getResult<MemorySSAAnalysis>(F).getMSSA();

  for (auto &BB : F) {
    for (auto &Inst : BB) {
      if (auto Load = llvm::dyn_cast<LoadInst>(&Inst)) {
        if (Load->getPointerAddressSpace() == 3)
          ToCheck.push_back(Load);
      }
    }
  }

  for (auto &Load : ToCheck) {
    auto MA = MemSSA.getMemoryAccess(Load);
    auto DefAccess = MA->getDefiningAccess();
    /**
    * We are only checking the simplest case: MemoryDef is a store to threadgroup memory
    * It's not complete, but good enough for all games I known requiring such warp-coherency behavior
    * FIXME: uniformity can be a issue because we are using `simdgroup_barrier`
    */
    if (auto Def = llvm::dyn_cast<MemoryDef>(DefAccess)) {
      auto DefInst = Def->getMemoryInst();
      if (Fenced.contains(DefInst))
        continue;
      if (auto Store = llvm::dyn_cast<StoreInst>(DefInst)) {
        if (Store->getPointerAddressSpace() == 3) {
          Fenced.insert(DefInst);
          IRBuilder<> builder(Store->getParent());
          if (Store->getNextNode())
            builder.SetInsertPoint(Store->getNextNode());
          AIRBuilder air(builder, nulls());
          // I expect atomic_thread_fence() to work, but no luck
          air.CreateBarrier(MemFlags::Threadgroup, true);
          Changed = true;
        }
      }
    }
  }

  return (Changed ? PreservedAnalyses::none() : PreservedAnalyses::all());
}
} // namespace llvm::air