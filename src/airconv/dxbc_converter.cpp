
#include "dxbc_converter.h"
#include "airconv_public.h"
#include "DXBCParser/BlobContainer.h"
#include "DXBCParser/ShaderBinary.h"
#include "DXBCParser/DXBCUtils.h"
#include "dxbc_utils.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include <memory>

namespace dxmt {

DxbcConverter::DxbcConverter() {}

DxbcConverter::~DxbcConverter() {}

void DxbcConverter::AnalyzeShader(D3D10ShaderBinary::CShaderCodeParser &Parser) {}

void DxbcConverter::Convert(LPCVOID dxbc, UINT dxbcSize, LPVOID *ppAIR,
                            UINT *pAIRSize) {

  // module metadata
  pModule_ = std::make_unique<Module>("generated.metal", context_);
  pModule_->setSourceFileName("airconv_generated.metal");
  pModule_->setTargetTriple("air64-apple-macosx13.0.0");
  pModule_->setDataLayout(
      "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:"
      "64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-"
      "v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024-n8:"
      "16:32");

  CDXBCParser DXBCParser;
  DXASSERT(DXBCParser.ReadDXBC(dxbc, dxbcSize) == S_OK,
           "otherwise invalid dxbc blob");

  UINT codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShaderEx);
  if (codeBlobIdx == DXBC_BLOB_NOT_FOUND) {
    codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShader);
  }
  DXASSERT(codeBlobIdx != DXBC_BLOB_NOT_FOUND, "otherwise invalid dxbc blob");
  LPCVOID codeBlob = DXBCParser.GetBlob(codeBlobIdx);

  D3D10ShaderBinary::CShaderCodeParser CodeParser;

  CShaderToken *ShaderCode =
      (CShaderToken *)(BYTE *)codeBlob - sizeof(DXBCBlobHeader);
  // 1. Collect information about the shader.
  CodeParser.SetShader(ShaderCode);
  AnalyzeShader(CodeParser);

  // 2. Parse input signature(s).
  //   DXBCGetInputSignature(const void *pBlobContainer, CSignatureParser
  //   *pParserToUse)

  // 3. Parse output signature(s).

  // 4. Transform DXBC to AIR.
  CodeParser.SetShader(ShaderCode);
  ConvertInstructions(CodeParser);

  // 5. Emit medatada.
  EmitAIRMetadata();

  // 6. Cleanup/Optimize AIR.
  Optimize();

  // Serialize AIR
  SerializeAIR();

  pModule_.reset();
}

void DxbcConverter::ConvertInstructions(
    D3D10ShaderBinary::CShaderCodeParser &Parser) {

        StructType::get(context_, {
            FixedVectorType::get(
                int32Ty, 4
            )
        },false);

  FunctionType *entryFunctionTy = FunctionType::get(
      voidTy, {}, false); // TODO: need proper param and return type definition
      
  Function *pFunction =
      Function::Create(entryFunctionTy, llvm::GlobalValue::ExternalLinkage,
                       "__main__", pModule_.get());

  auto pBasicBlock = BasicBlock::Create(context_, "entry", pFunction);
  pBuilder_ = std::make_unique<IRBuilder<>>(pBasicBlock);

  // TODO: check fastmath

  if (Parser.EndOfShader()) {
    pBuilder_->CreateRetVoid();
    return;
  }

  for (;;) {
  }
}

void DxbcConverter::LoadOperand(int src,
                                const D3D10ShaderBinary::CInstruction &Inst,
                                const unsigned int OpIdx, const CMask &Mask,
                                const int ValueType) {}

void DxbcConverter::EmitAIRMetadata() {
  MDBuilder mdbuilder(context_);
  auto uintMetadata = [&](uint32_t value) {
    return mdbuilder.createConstant(
        ConstantInt::get(Type::getInt32Ty(context_), value));
  };

  auto stringMetadata = [&](StringRef str) {
    return MDString::get(context_, str);
  };
  auto llvmIdent = pModule_->getOrInsertNamedMetadata("llvm.ident");
  llvmIdent->addOperand(
      MDNode::get(context_, {MDString::get(context_, "airconv")}));

  auto airVersion = pModule_->getOrInsertNamedMetadata("air.version");
  airVersion->addOperand(MDNode::get(context_, {
                                                   uintMetadata(2),
                                                   uintMetadata(5),
                                                   uintMetadata(0),
                                               }));
  auto airLangVersion =
      pModule_->getOrInsertNamedMetadata("air.language_version");
  airLangVersion->addOperand(MDNode::get(context_, {
                                                       stringMetadata("Metal"),
                                                       uintMetadata(3),
                                                       uintMetadata(0),
                                                       uintMetadata(0),
                                                   }));
}

void DxbcConverter::Optimize() {
  // TODO
}

void DxbcConverter::SerializeAIR() {}

extern "C" void ConvertDXBC(const void *pDXBC, uint32_t DXBCSize, void **ppMetalLib, uint32_t* pMetalLibSize) {
  DxbcConverter converter;
  converter.Convert(pDXBC, DXBCSize, ppMetalLib, pMetalLibSize);
};

} // namespace dxmt