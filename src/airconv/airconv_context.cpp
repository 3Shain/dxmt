#include "./dxbc_analyzer.h"
#include "./dxbc_converter.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Support/VersionTuple.h"
#include <cassert>
#include "DXBCParser/BlobContainer.h"
#include "DXBCParser/ShaderBinary.h"
#include "DXBCParser/DXBCUtils.h"
#include "air_metadata.h"
#include "air_type.h"

using namespace llvm;

namespace dxmt {

void Convert(LPCVOID dxbc, UINT dxbcSize, LPVOID *ppAIR, UINT *pAIRSize) {

  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  AirType types(context);
  AirMetadata md(context);

  IRBuilder<> builder(context);
  auto pModule_ = std::make_unique<Module>("shader.air", context);
  pModule_->setSourceFileName("airconv_generated.metal");
  pModule_->setTargetTriple("air64-apple-macosx14.0.0");
  pModule_->setDataLayout(
      "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:"
      "64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-"
      "v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024-n8:"
      "16:32");
  pModule_->setSDKVersion(VersionTuple(14, 0));
  pModule_->addModuleFlag(Module::ModFlagBehavior::Error, "wchar_size", 4);
  pModule_->addModuleFlag(Module::ModFlagBehavior::Max, "frame-pointer", 2);
  pModule_->addModuleFlag(Module::ModFlagBehavior::Max,
                          "air.max_device_buffers", 31);
  pModule_->addModuleFlag(Module::ModFlagBehavior::Max,
                          "air.max_constant_buffers", 31);
  pModule_->addModuleFlag(Module::ModFlagBehavior::Max,
                          "air.max_threadgroup_buffers", 31);
  pModule_->addModuleFlag(Module::ModFlagBehavior::Max, "air.max_textures",
                          128);
  pModule_->addModuleFlag(Module::ModFlagBehavior::Max,
                          "air.max_read_write_textures", 8);
  pModule_->addModuleFlag(Module::ModFlagBehavior::Max, "air.max_samplers", 16);

  auto airVersion = pModule_->getOrInsertNamedMetadata("air.version");
  airVersion->addOperand(MDTuple::get(context, {md.createUnsignedInteger(2),
                                                md.createUnsignedInteger(6),
                                                md.createUnsignedInteger(0)}));
  auto airLangVersion =
      pModule_->getOrInsertNamedMetadata("air.language_version");
  airLangVersion->addOperand(MDTuple::get(
      context, {md.createString("Metal"), md.createUnsignedInteger(3),
                md.createUnsignedInteger(0), md.createUnsignedInteger(0)}));

  auto airCompileOptions =
      pModule_->getOrInsertNamedMetadata("air.compile_options");
  airCompileOptions->addOperand(
      MDTuple::get(context, {md.createString("air.compile.denorms_disable")}));
  airCompileOptions->addOperand(
      MDTuple::get(context, {md.createString("air.compile.fast_math_enable")}));
  airCompileOptions->addOperand(MDTuple::get(
      context, {md.createString("air.compile.framebuffer_fetch_enable")}));

  // pModule_

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

  CShaderToken *ShaderCode = (CShaderToken *)(BYTE *)codeBlob;
  // 1. Collect information about the shader.
  CodeParser.SetShader(ShaderCode);
  CSignatureParser inputParser;
  // TODO: throw if failed
  DXBCGetInputSignature(dxbc, &inputParser);
  CSignatureParser outputParser;
  DXBCGetOutputSignature(dxbc, &outputParser);

  DxbcAnalyzer analyzer(context, types, md);
  analyzer.AnalyzeShader(CodeParser, inputParser, outputParser);

  DxbcConverter converter(analyzer, types, context, *pModule_);

  // 4. convert instructions
  CodeParser.SetShader(ShaderCode); // reset parser
  auto inputMD = converter.Pre(md);
  converter.ConvertInstructions(CodeParser);
  auto functionMD = converter.Post(md, inputMD);

  converter.Optimize(OptimizationLevel::O2);

  if (analyzer.IsVS()) {
    pModule_->getOrInsertNamedMetadata("air.vertex")->addOperand(functionMD);
  } else if (analyzer.IsPS()) {
    pModule_->getOrInsertNamedMetadata("air.fragment")->addOperand(functionMD);
  } else if (analyzer.IsCS()) {
    pModule_->getOrInsertNamedMetadata("air.kernel")->addOperand(functionMD);
  } else {
    // throw
    assert(0 && "Unsupported shader type");
  }

  // Serialize AIR
  SmallVector<char, 0> vec;
  raw_svector_ostream OS(vec);
  converter.SerializeAIR(OS);
  auto ptr = malloc(vec.size_in_bytes());
  memcpy(ptr, vec.data(), vec.size_in_bytes());

  *ppAIR = ptr;
  *pAIRSize = vec.size_in_bytes();

  pModule_->print(outs(), nullptr, true);

  pModule_.reset();
}

extern "C" void ConvertDXBC(const void *pDXBC, uint32_t DXBCSize,
                            void **ppMetalLib, uint32_t *pMetalLibSize) {
  Convert(pDXBC, DXBCSize, ppMetalLib, pMetalLibSize);
};

} // namespace dxmt