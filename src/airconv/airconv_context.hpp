#pragma once
#include "llvm/IR/Module.h"
#include "llvm/Passes/OptimizationLevel.h"

namespace dxmt {

void initializeModule(llvm::Module &M);

void runOptimizationPasses(llvm::Module &M);

void linkMSAD(llvm::Module &M);

void linkSamplePos(llvm::Module &M);

void linkTessellation(llvm::Module &M);

} // namespace dxmt