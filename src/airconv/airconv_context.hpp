#pragma once
#include "llvm/IR/Module.h"
#include "llvm/Passes/OptimizationLevel.h"

namespace dxmt {

struct ModuleOptions {
  bool enableFastMath;
};

void initializeModule(llvm::Module &M, const ModuleOptions &opts);

void runOptimizationPasses(llvm::Module &M);

void linkMSAD(llvm::Module &M);

void linkSamplePos(llvm::Module &M);

void linkTessellation(llvm::Module &M);

} // namespace dxmt