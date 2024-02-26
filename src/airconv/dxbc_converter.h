
#pragma once
#include <cstdint>
#include <memory>
#include <optional>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Support/raw_ostream.h"
#include "DXBCParser/ShaderBinary.h"

#include "air_builder.h"
#include "air_constants.h"
#include "air_metadata.h"
#include "air_operation.h"
#include "air_type.h"
#include "dxbc_analyzer.h"
#include "dxbc_utils.h"

namespace dxmt {

class DxbcConverter {

public:
  DxbcConverter(DxbcAnalyzer &analyzer, AirType &types,
                llvm::LLVMContext &context, llvm::Module &pModule);

protected:
  DxbcAnalyzer &analyzer;
  AirType &types;
  llvm::LLVMContext &context_;
  llvm::Module &pModule_;
  llvm::IRBuilder<> pBuilder_;
  AirBuilder airBuilder;
  AirOp airOp;

  unsigned m_DxbcMajor;
  unsigned m_DxbcMinor;

  llvm::Function *function;
  llvm::BasicBlock *epilogue;

  __ShaderContext shaderContext;

public:
  llvm::MDNode *Pre(AirMetadata &metadata);

  llvm::MDNode *Post(AirMetadata &metadata, llvm::MDNode *input);

  void ConvertInstructions(D3D10ShaderBinary::CShaderCodeParser &Parser);

  void Optimize(llvm::OptimizationLevel leve);

  void SerializeAIR(llvm::raw_ostream &OS);

  /* internal helpers */

  llvm::Value *LoadOperand(const D3D10ShaderBinary::CInstruction &Inst,
                           const unsigned OpIdx);

  llvm::Value *
  LoadOperandIndex(const D3D10ShaderBinary::COperandIndex &OpIndex,
                   const D3D10_SB_OPERAND_INDEX_REPRESENTATION IndexType);

  void StoreOperand(llvm::Value *, const D3D10ShaderBinary::CInstruction &Inst,
                    const unsigned OpIdx, uint32_t mask);

  llvm::Value *ApplyOperandModifiers(llvm::Value *pValue,
                                     const D3D10ShaderBinary::COperandBase &O);

  void GetResourceRange(const D3D10ShaderBinary::CInstruction &Inst) {
    unsigned RangeID = Inst.m_Operands[0].m_Index[0].m_RegIndex;
    unsigned LB, RangeSize;
    if (analyzer.IsSM51Plus()) {
      LB = Inst.m_Operands[0].m_Index[1].m_RegIndex;
      RangeSize = Inst.m_Operands[0].m_Index[2].m_RegIndex != UINT_MAX
                      ? Inst.m_Operands[0].m_Index[2].m_RegIndex - LB + 1
                      : UINT_MAX;
    } else {
      LB = RangeID;
      RangeSize = 1;
    }
  }
};

class Scope {};
} // namespace dxmt