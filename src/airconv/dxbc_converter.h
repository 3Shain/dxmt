
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

public:
  void AnalyzeShader(D3D10ShaderBinary::CShaderCodeParser &Parser);

  void ConvertInstructions(D3D10ShaderBinary::CShaderCodeParser &Parser);

  void LoadOperand(int src, const D3D10ShaderBinary::CInstruction &Inst,
                   const unsigned OpIdx, const CMask &Mask,
                   const int ValueType);

  void StoreOperand(int Dst, const D3D10ShaderBinary::CInstruction &Inst,
                    const unsigned OpIdx, const CMask &Mask,
                    const int ValueType);

  void EmitAIRMetadata();

  void Optimize();

  void SerializeAIR(raw_ostream& OS);

protected:
  Type *voidTy = Type::getVoidTy(context_);
  IntegerType *int32Ty = Type::getInt32Ty(context_);
  Type *floatTy = Type::getFloatTy(context_);
  // Type *voidTy = Type::getVoidTy(context_);
  // Type *voidTy = Type::getVoidTy(context_);
  // Type *voidTy = Type::getVoidTy(context_);
};
} // namespace dxmt