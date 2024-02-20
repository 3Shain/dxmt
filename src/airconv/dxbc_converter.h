
#pragma once
#include <cstdint>
#include <memory>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"
#include "DXBCParser/ShaderBinary.h"
#include "dxbc_utils.h"

using namespace llvm;

namespace dxmt {

class DxbcConverter {

public:
  DxbcConverter();
  ~DxbcConverter();

  void Convert(LPCVOID dxbc, UINT dxbcSize, LPVOID *ppAIR, UINT *pAIRSize);

protected:
  LLVMContext context_;
  std::unique_ptr<Module> pModule_;
  std::unique_ptr<IRBuilder<>> pBuilder_;
  unsigned m_DxbcMajor;
  unsigned m_DxbcMinor;
  bool IsSM51Plus() const {
    return m_DxbcMajor > 5 || (m_DxbcMajor == 5 && m_DxbcMinor >= 1);
  }

public:
  void AnalyzeShader(D3D10ShaderBinary::CShaderCodeParser &Parser);

  void ConvertInstructions(D3D10ShaderBinary::CShaderCodeParser &Parser);

  void LoadOperand(int src, const D3D10ShaderBinary::CInstruction &Inst,
                   const unsigned OpIdx, const CMask &Mask,
                   const int ValueType);

  void StoreOperand(int Dst, const D3D10ShaderBinary::CInstruction &Inst,
                    const unsigned OpIdx, const CMask &Mask,
                    const int ValueType);

  Value *ApplyOperandModifiers(Value *pValue,
                               const D3D10ShaderBinary::COperandBase &O);

  void EmitAIRMetadata();

  void Optimize();

  void SerializeAIR(raw_ostream &OS);

protected:
  Type *voidTy = Type::getVoidTy(context_);
  IntegerType *int32Ty = Type::getInt32Ty(context_);
  Type *floatTy = Type::getFloatTy(context_);
  // Type *voidTy = Type::getVoidTy(context_);
  // Type *voidTy = Type::getVoidTy(context_);
  // Type *voidTy = Type::getVoidTy(context_);

  // helpers
protected:
  void GetResourceRange(const D3D10ShaderBinary::CInstruction &Inst) {
    unsigned RangeID = Inst.m_Operands[0].m_Index[0].m_RegIndex;
    unsigned LB, RangeSize;
    if (IsSM51Plus()) {
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
} // namespace dxmt