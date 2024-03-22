#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/VersionTuple.h"

#include "./air_type.hpp"
#include "dxbc_converter.hpp"
#include "metallib_writer.hpp"

using namespace llvm;
using namespace dxmt::air;

namespace dxmt {

void Convert(
  const void *dxbc, uint32_t dxbcSize, void **ppAIR, uint32_t *pAIRSize
) {

  LLVMContext context;

  context.setOpaquePointers(false); // I suspect Metal uses LLVM 14...

  AirType types(context);

  IRBuilder<> builder(context);
  auto pModule = std::make_unique<Module>("shader.air", context);
  pModule->setSourceFileName("airconv_generated.metal");
  pModule->setTargetTriple("air64-apple-macosx14.0.0");
  pModule->setDataLayout(
    "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:"
    "64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-"
    "v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024-n8:"
    "16:32"
  );
  pModule->setSDKVersion(VersionTuple(14, 0));
  pModule->addModuleFlag(Module::ModFlagBehavior::Error, "wchar_size", 4);
  pModule->addModuleFlag(Module::ModFlagBehavior::Max, "frame-pointer", 2);
  pModule->addModuleFlag(
    Module::ModFlagBehavior::Max, "air.max_device_buffers", 31
  );
  pModule->addModuleFlag(
    Module::ModFlagBehavior::Max, "air.max_constant_buffers", 31
  );
  pModule->addModuleFlag(
    Module::ModFlagBehavior::Max, "air.max_threadgroup_buffers", 31
  );
  pModule->addModuleFlag(Module::ModFlagBehavior::Max, "air.max_textures", 128);
  pModule->addModuleFlag(
    Module::ModFlagBehavior::Max, "air.max_read_write_textures", 8
  );
  pModule->addModuleFlag(Module::ModFlagBehavior::Max, "air.max_samplers", 16);

  //   auto airVersion = pModule->getOrInsertNamedMetadata("air.version");
  //   airVersion->addOperand(
  //       MDTuple::get(context, {metadata.createUnsignedInteger(2),
  //                              metadata.createUnsignedInteger(6),
  //                              metadata.createUnsignedInteger(0)}));
  //   auto airLangVersion =
  //       pModule->getOrInsertNamedMetadata("air.language_version");
  //   airLangVersion->addOperand(MDTuple::get(
  //       context,
  //       {metadata.createString("Metal"), metadata.createUnsignedInteger(3),
  //        metadata.createUnsignedInteger(0),
  //        metadata.createUnsignedInteger(0)}));

  //   auto airCompileOptions =
  //       pModule->getOrInsertNamedMetadata("air.compile_options");
  //   airCompileOptions->addOperand(MDTuple::get(
  //       context, {metadata.createString("air.compile.denorms_disable")}));
  //   airCompileOptions->addOperand(MDTuple::get(
  //       context, {metadata.createString("air.compile.fast_math_enable")}));
  //   airCompileOptions->addOperand(MDTuple::get(
  //       context,
  //       {metadata.createString("air.compile.framebuffer_fetch_enable")}));

  dxbc::convertDXBC(dxbc, dxbcSize, context, *pModule);

  // Create the analysis managers.
  // These must be declared in this order so that they are destroyed in the
  // correct order due to inter-analysis-manager references.
  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  // Create the new pass manager builder.
  // Take a look at the PassBuilder constructor parameters for more
  // customization, e.g. specifying a TargetMachine or various debugging
  // options.
  PassBuilder PB;

  // Register all the basic analyses with the managers.
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  ModulePassManager MPM =
    PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);

  FunctionPassManager FPM;
  FPM.addPass(VerifierPass());

  MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));

  // Optimize the IR!
  MPM.run(*pModule, MAM);

  pModule->print(outs(), nullptr);

  // Serialize AIR
  SmallVector<char, 0> vec;
  raw_svector_ostream OS(vec);

  metallib::MetallibWriter writer;

  writer.Write(*pModule, OS);

  auto ptr = malloc(vec.size());
  memcpy(ptr, vec.data(), vec.size());

  *ppAIR = ptr;
  *pAIRSize = vec.size();

  pModule.reset();
}

extern "C" void ConvertDXBC(
  const void *pDXBC, uint32_t DXBCSize, void **ppMetalLib,
  uint32_t *pMetalLibSize
) {
  Convert(pDXBC, DXBCSize, ppMetalLib, pMetalLibSize);
};

} // namespace dxmt