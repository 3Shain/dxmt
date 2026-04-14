#pragma once
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

/**
 * This is a poor man's automatic memory barrier insertion pass for threadgroup memory read-after-write
 * Some games expect shared memery to be coherent within a wrap(simdgroup), but this is not the case on Metal
 * where `simdgroup_barrier()` is explicitly introduced.
 */

namespace llvm::air {
class SimdgroupImplicitMemBarrierPass : public PassInfoMixin<SimdgroupImplicitMemBarrierPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};
} // namespace llvm::air