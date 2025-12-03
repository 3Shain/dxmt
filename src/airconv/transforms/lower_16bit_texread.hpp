#pragma once
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm::air {
class Lower16BitTexReadPass : public PassInfoMixin<Lower16BitTexReadPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};
} // namespace llvm::air