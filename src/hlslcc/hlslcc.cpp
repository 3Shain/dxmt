#include "hlslcc.h"

#include <memory>
#include <sstream>
#include "hlslcc_context.h"
#include "metal.h"
#include "shader.h"
#include "decode.h"

// char *TranslateToMSL(DXBCShader *_shader) {
//   char *glslcstr = NULL;
//   int success = 0;

//   Shader *shader = ((Shader *)_shader);

//   HLSLccReflection reflectionCallbacks;

//   HLSLCrossCompilerContext context(reflectionCallbacks);

//   context.psShader = shader;
//   context.flags = HLSLCC_FLAG_INCLUDE_INSTRUCTIONS_COMMENTS;

//   std::unique_ptr<CrossShaderDependency> depPtr = nullptr;

//   depPtr.reset(new CrossShaderDependency());
//   context.dependencies = depPtr.get();

//   for (uint32_t i = 0; i < shader->phases.size(); ++i) {
//     shader->phases[i].hasPostShaderCode = 0;
//   }

//   ToMetal translator(&context);
//   if (!translator.Translate()) {
//     bdestroy(context.glsl);
//     for (uint32_t i = 0; i < shader->phases.size(); ++i) {
//       bdestroy(shader->phases[i].postShaderCode);
//       bdestroy(shader->phases[i].earlyMain);
//     }

//     return 0;
//   }

//   glslcstr = bstr2cstr(context.glsl, '\0');
//   // result->sourceCode = glslcstr;
//   // bcstrfree(glslcstr);

//   bdestroy(context.glsl);
//   for (uint32_t i = 0; i < shader->phases.size(); ++i) {
//     bdestroy(shader->phases[i].postShaderCode);
//     bdestroy(shader->phases[i].earlyMain);
//   }

//   return glslcstr;
// }

//  std::string
// TranslateOrdinaryRenderPipelineToMSL(DXBCShader *vertex, DXBCShader *fragment) {

// }

//  DXBCShader *DecodeDXBCShader(const void *bytecode) {
//   auto shader = DecodeDXBC((uint32_t *)bytecode);
//   return (DXBCShader *)shader;
// }

//  void GetDXBCShaderInfo(DXBCShader *_shader,
//                                   ShaderInfo **ShaderInfo) {
//   auto shader = (Shader *)_shader;
//   *ShaderInfo = &shader->info;
// }

// HLSLCC_API void ReleaseDXBCShader(DXBCShader *_shader) {
//   auto shader = (Shader *)_shader;
//   delete shader;
// }

// HLSLCC_API SHADER_TYPE GetDXBCShaderType(DXBCShader *_shader) {
//   auto shader = (Shader *)_shader;
//   return shader->eShaderType;
// }

// HLSLCC_API void ReflectVertexStageInfo(DXBCShader *_shader,
//                                        DXBCVertexStageInfo *info) {
//   auto shader = (Shader *)_shader;

//   // TODO
// }

// HLSLCC_API void ReflectHullStageInfo(DXBCShader *_shader,
//                                      DXBCHullStageInfo *info) {
//   auto shader = (Shader *)_shader;

//   info->domain = shader->info.reflection.Tess.TessDomain;
//   info->outputPrimitive = shader->info.reflection.Tess.TessOutPrim;
//   info->partition = shader->info.reflection.Tess.TessPartitioning;
//   info->maxTessFactor = shader->info.reflection.Tess.MaxTessFactor;
//   info->outputControlPoints = shader->info.reflection.Tess.outputControlPoints;
// }

// HLSLCC_API void ReflectDomainStageInfo(DXBCShader *_shader,
//                                        DXBCDomainStageInfo *info) {
//   auto shader = (Shader *)_shader;

//   info->inputControlPoints = shader->info.reflection.Domain.inputControlPoints;
// }

// HLSLCC_API void ReflectFragmentStageInfo(DXBCShader *_shader,
//                                          DXBCFragmentStageInfo *info) {
//   auto shader = (Shader *)_shader;

//   // TODO
// }

// HLSLCC_API void ReflectGeometryStageInfo(DXBCShader *_shader,
//                                          DXBCGeometryStageInfo *info) {
//   auto shader = (Shader *)_shader;

//   info->inputPrimitive = shader->info.reflection.Geometry.inputPrimitive;
//   info->outputTopology = shader->info.reflection.Geometry.outputTopology;
//   info->maxVertexOut = shader->info.reflection.Geometry.maxVertexOutput;
// }

// HLSLCC_API void ReflectComputeStageInfo(DXBCShader *_shader,
//                                         DXBCComputeStageInfo *info) {
//   auto shader = (Shader *)_shader;
//   info->threadGroupSize[0] = shader->info.reflection.WorkGroupSize[0];
//   info->threadGroupSize[1] = shader->info.reflection.WorkGroupSize[1];
//   info->threadGroupSize[2] = shader->info.reflection.WorkGroupSize[2];
// }

// HLSLCC_API size_t GetShaderInputSignatureCount(DXBCShader *_shader) {
//   auto shader = (Shader *)_shader;
//   return shader->info.inputSignatures.size() +
//          shader->info.patchConstantSignatures.size();
// }

// uint32_t GetNumberBitsSet(uint32_t a) {
//   // Calculate number of bits in a
//   // Taken from
//   // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSet64 Works
//   // only up to 14 bits (we're only using up to 4)
//   return (a * 0x200040008001ULL & 0x111111111111111ULL) % 0xf;
// }

// HLSLCC_API void ReflectShaderInputSignature(DXBCShader *_shader,
//                                             DXBCInputOutputSignature *_array) {
//   auto shader = (Shader *)_shader;
//   size_t i = 0;
//   for (auto sig : shader->info.inputSignatures) {
//     auto array = &_array[i];
//     strcpy(array->semanticName, sig.semanticName.c_str());
//     array->semanticIndex = sig.semanticIndex;
//     array->vertexAttribute = sig.vertexAttribute;
//     array->componentType = sig.eComponentType;
//     array->numComponent = GetNumberBitsSet(sig.ui32Mask);
//     array->isPatchConstant = false;
//     i++;
//   }
//   for (auto sig : shader->info.patchConstantSignatures) {
//     auto array = &_array[i];
//     strcpy(array->semanticName, sig.semanticName.c_str());
//     array->semanticIndex = sig.semanticIndex;
//     array->vertexAttribute = sig.vertexAttribute;
//     array->componentType = sig.eComponentType;
//     array->numComponent = GetNumberBitsSet(sig.ui32Mask);
//     array->isPatchConstant = true;
//     i++;
//   }
// }