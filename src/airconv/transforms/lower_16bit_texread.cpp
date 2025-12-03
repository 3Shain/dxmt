#include "lower_16bit_texread.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"

namespace llvm::air {

PreservedAnalyses
Lower16BitTexReadPass::run(Function &F, FunctionAnalysisManager &AM) {
  bool Changed = false;

  SmallVector<Instruction *, 4> ToErase;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      // we only care about BitCastInst
      auto *BC = dyn_cast<BitCastInst>(&I);
      if (!BC)
        continue;

      FixedVectorType *DestVT = dyn_cast<FixedVectorType>(BC->getType());
      FixedVectorType *SrcVT = dyn_cast<FixedVectorType>(BC->getOperand(0)->getType());
      if (!DestVT || !SrcVT)
        continue;

      // match <4 x i32> -> <8 x i16>
      if (SrcVT->getNumElements() != 4)
        continue;
      if (DestVT->getNumElements() != 8)
        continue;

      Type *SrcElt = SrcVT->getElementType();
      Type *DstElt = DestVT->getElementType();
      if (!SrcElt->isIntegerTy(32))
        continue;
      if (!DstElt->isIntegerTy(16))
        continue;

      // collect users to avoid modifying use-iterator while mutating
      SmallVector<ExtractElementInst *, 4> ExtractUses;
      for (User *U : BC->users()) {
        if (auto *EE = dyn_cast<ExtractElementInst>(U)) {
          // only constant index supported
          if (isa<ConstantInt>(EE->getIndexOperand())) {
            ExtractUses.push_back(EE);
          }
        }
      }

      if (ExtractUses.empty())
        continue;

      // For each extract use, replace it
      for (ExtractElementInst *EE : ExtractUses) {
        ConstantInt *IdxC = cast<ConstantInt>(EE->getIndexOperand());
        uint64_t idx = IdxC->getZExtValue();
        // sanity check
        if (idx >= DestVT->getNumElements())
          continue;

        uint64_t srcLane = idx / 2; // which i32 lane in <4 x i32>
        bool isHigh = (idx % 2) == 1;

        // Create new extract from the original src vector (operand 0 of bitcast)
        IRBuilder<> B(EE);
        Value *SrcVec = BC->getOperand(0);

        // newExtract: i32 extracted from <4 x i32>
        Value *NewExtract = B.CreateExtractElement(SrcVec, srcLane);

        Value *AfterShift = NewExtract;
        if (isHigh) {
          AfterShift = B.CreateLShr(NewExtract, 16u, NewExtract->getName() + ".lshr16");
        }

        // truncate to i16
        Value *Trunc = B.CreateTrunc(AfterShift, DstElt, EE->getName() + ".trunc_to_i16");

        // replace uses of EE with Trunc
        EE->replaceAllUsesWith(Trunc);

        // mark old extract for erasure
        ToErase.push_back(EE);
        Changed = true;
      }

      ToErase.push_back(BC);
    }
  }

  for (Instruction *Inst : ToErase) {
    if (Inst->use_empty()) {
      Inst->eraseFromParent();
    }
  }

  return (Changed ? PreservedAnalyses::none() : PreservedAnalyses::all());
}
} // namespace llvm::air