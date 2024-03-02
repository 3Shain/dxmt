#include "./dxbc_analyzer.hpp"
#include "./dxbc_converter.hpp"
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
#include "dxbc_signature.hpp"
#include "metallib_writer.hpp"

using namespace llvm;
using namespace dxmt::air;

namespace dxmt {

void Convert(LPCVOID dxbc, UINT dxbcSize, LPVOID *ppAIR, UINT *pAIRSize) {

  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  AirType types(context);
  AirMetadata metadata(context);

  IRBuilder<> builder(context);
  auto pModule = std::make_unique<Module>("shader.air", context);
  pModule->setSourceFileName("airconv_generated.metal");
  pModule->setTargetTriple("air64-apple-macosx14.0.0");
  pModule->setDataLayout(
      "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:"
      "64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-"
      "v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024-n8:"
      "16:32");
  pModule->setSDKVersion(VersionTuple(14, 0));
  pModule->addModuleFlag(Module::ModFlagBehavior::Error, "wchar_size", 4);
  pModule->addModuleFlag(Module::ModFlagBehavior::Max, "frame-pointer", 2);
  pModule->addModuleFlag(Module::ModFlagBehavior::Max, "air.max_device_buffers",
                         31);
  pModule->addModuleFlag(Module::ModFlagBehavior::Max,
                         "air.max_constant_buffers", 31);
  pModule->addModuleFlag(Module::ModFlagBehavior::Max,
                         "air.max_threadgroup_buffers", 31);
  pModule->addModuleFlag(Module::ModFlagBehavior::Max, "air.max_textures", 128);
  pModule->addModuleFlag(Module::ModFlagBehavior::Max,
                         "air.max_read_write_textures", 8);
  pModule->addModuleFlag(Module::ModFlagBehavior::Max, "air.max_samplers", 16);

  auto airVersion = pModule->getOrInsertNamedMetadata("air.version");
  airVersion->addOperand(
      MDTuple::get(context, {metadata.createUnsignedInteger(2),
                             metadata.createUnsignedInteger(6),
                             metadata.createUnsignedInteger(0)}));
  auto airLangVersion =
      pModule->getOrInsertNamedMetadata("air.language_version");
  airLangVersion->addOperand(MDTuple::get(
      context,
      {metadata.createString("Metal"), metadata.createUnsignedInteger(3),
       metadata.createUnsignedInteger(0), metadata.createUnsignedInteger(0)}));

  auto airCompileOptions =
      pModule->getOrInsertNamedMetadata("air.compile_options");
  airCompileOptions->addOperand(MDTuple::get(
      context, {metadata.createString("air.compile.denorms_disable")}));
  airCompileOptions->addOperand(MDTuple::get(
      context, {metadata.createString("air.compile.fast_math_enable")}));
  airCompileOptions->addOperand(MDTuple::get(
      context,
      {metadata.createString("air.compile.framebuffer_fetch_enable")}));

  CDXBCParser DXBCParser;
  assert((DXBCParser.ReadDXBC(dxbc, dxbcSize) == S_OK) && "invalid dxbc blob");

  UINT codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShaderEx);
  if (codeBlobIdx == DXBC_BLOB_NOT_FOUND) {
    codeBlobIdx = DXBCParser.FindNextMatchingBlob(DXBC_GenericShader);
  }
  assert((codeBlobIdx != DXBC_BLOB_NOT_FOUND) && "invalid dxbc blob");
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

  DxbcAnalyzer analyzer(context, types, metadata);
  analyzer.AnalyzeShader(
      CodeParser,
      [&](auto matcher) -> dxbc::Signature {
        const D3D11_SIGNATURE_PARAMETER *parameters;
        inputParser.GetParameters(&parameters);
        for (unsigned i = 0; i < inputParser.GetNumParameters(); i++) {
          auto sig = dxbc::Signature(parameters[i]);
          if (matcher(sig)) {
            return sig;
          }
        }
        assert(0 && "try to access an undefined input");
      },
      [&](auto matcher) -> dxbc::Signature {
        const D3D11_SIGNATURE_PARAMETER *parameters;
        outputParser.GetParameters(&parameters);
        for (unsigned i = 0; i < outputParser.GetNumParameters(); i++) {
          auto sig = dxbc::Signature(parameters[i]);
          if (matcher(sig)) {
            return sig;
          }
        }
        assert(0 && "try to access an undefined output");
      });

  DxbcConverter converter(analyzer, types, context, *pModule);

  // 4. convert instructions
  CodeParser.SetShader(ShaderCode); // reset parser
  auto inputMD = converter.Pre(metadata);
  converter.ConvertInstructions(CodeParser);
  auto functionMD = converter.Post(metadata, inputMD);

  converter.Optimize(OptimizationLevel::O2);

  if (analyzer.IsVS()) {
    pModule->getOrInsertNamedMetadata("air.vertex")->addOperand(functionMD);
  } else if (analyzer.IsPS()) {
    pModule->getOrInsertNamedMetadata("air.fragment")->addOperand(functionMD);
  } else if (analyzer.IsCS()) {
    pModule->getOrInsertNamedMetadata("air.kernel")->addOperand(functionMD);
  } else {
    // throw
    assert(0 && "Unsupported shader type");
  }

  // Serialize AIR
  SmallVector<char, 0> vec;
  raw_svector_ostream OS(vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  auto ptr = malloc(vec.size());
  memcpy(ptr, vec.data(), vec.size());

  *ppAIR = ptr;
  *pAIRSize = vec.size();

  pModule->print(outs(), nullptr);

  pModule.reset();
}

extern "C" void ConvertDXBC(const void *pDXBC, uint32_t DXBCSize,
                            void **ppMetalLib, uint32_t *pMetalLibSize) {
  Convert(pDXBC, DXBCSize, ppMetalLib, pMetalLibSize);
};

} // namespace dxmt