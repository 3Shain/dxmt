#define WIN_EXPORT
#include "wineunixlib.h"
#include "unix_thunks.h"

AIRCONV_API int
SM50Initialize(
    const void *pBytecode, size_t BytecodeSize, SM50Shader **ppShader, struct MTL_SHADER_REFLECTION *pRefl,
    SM50Error **ppError
) {
  struct sm50_initialize_params params;
  params.bytecode = pBytecode;
  params.bytecode_size = BytecodeSize;
  params.shader = ppShader;
  params.reflection = pRefl;
  params.error = ppError;
  NTSTATUS status;
  status = UNIX_CALL(sm50_initialize, &params);
  if (status)
    return -1;
  return params.ret;
}

AIRCONV_API void
SM50Destroy(SM50Shader *pShader) {
  struct sm50_destroy_params params;
  params.shader = pShader;
  UNIX_CALL(sm50_destroy, &params);
}

AIRCONV_API int
SM50Compile(
    SM50Shader *pShader, struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs, const char *FunctionName,
    SM50CompiledBitcode **ppBitcode, SM50Error **ppError
) {
  struct sm50_compile_params params;
  params.shader = (sm50_shader_t)pShader;
  params.args = pArgs;
  params.func_name = FunctionName;
  params.bitcode = ppBitcode;
  params.error = ppError;
  NTSTATUS status;
  status = UNIX_CALL(sm50_compile, &params);
  if (status)
    return -1;
  return params.ret;
}

AIRCONV_API void
SM50GetCompiledBitcode(SM50CompiledBitcode *pBitcode, struct MTL_SHADER_BITCODE *pData) {
  struct sm50_get_compiled_bitcode_params params;
  params.bitcode = (sm50_compiled_bitcode_t)pBitcode;
  params.data_out = pData;
  UNIX_CALL(sm50_get_compiled_bitcode, &params);
}

AIRCONV_API void
SM50DestroyBitcode(SM50CompiledBitcode *pBitcode) {
  struct sm50_destroy_bitcode_params params;
  params.bitcode = (sm50_compiled_bitcode_t)pBitcode;
  UNIX_CALL(sm50_destroy_bitcode, &params);
}

AIRCONV_API const char *
SM50GetErrorMesssage(SM50Error *pError) {
  struct sm50_get_error_message_params params;
  params.error = pError;
  NTSTATUS status;
  status = UNIX_CALL(sm50_get_error_message, &params);
  if (status)
    return "";
  return params.ret_message;
}

AIRCONV_API void
SM50FreeError(SM50Error *pError) {
  struct sm50_free_error_params params;
  params.error = pError;
  UNIX_CALL(sm50_free_error, &params);
}

AIRCONV_API int
SM50CompileTessellationPipelineVertex(
    SM50Shader *pVertexShader, SM50Shader *pHullShader, struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pVertexShaderArgs,
    const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
) {
  struct sm50_compile_tessellation_pipeline_vertex_params params;
  params.vertex = pVertexShader;
  params.hull = pHullShader;
  params.vertex_args = pVertexShaderArgs;
  params.func_name = FunctionName;
  params.bitcode = ppBitcode;
  params.error = ppError;
  NTSTATUS status;
  status = UNIX_CALL(sm50_compile_tessellation_vertex, &params);
  if (status)
    return -1;
  return params.ret;
}

AIRCONV_API int
SM50CompileTessellationPipelineHull(
    SM50Shader *pVertexShader, SM50Shader *pHullShader, struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pHullShaderArgs,
    const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
) {
  struct sm50_compile_tessellation_pipeline_hull_params params;
  params.vertex = pVertexShader;
  params.hull = pHullShader;
  params.hull_args = pHullShaderArgs;
  params.func_name = FunctionName;
  params.bitcode = ppBitcode;
  params.error = ppError;
  NTSTATUS status;
  status = UNIX_CALL(sm50_compile_tessellation_hull, &params);
  if (status)
    return -1;
  return params.ret;
}

AIRCONV_API int
SM50CompileTessellationPipelineDomain(
    SM50Shader *pHullShader, SM50Shader *pDomainShader, struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pDomainShaderArgs,
    const char *FunctionName, SM50CompiledBitcode **ppBitcode, SM50Error **ppError
) {
  struct sm50_compile_tessellation_pipeline_domain_params params;
  params.domain = pDomainShader;
  params.hull = pHullShader;
  params.domain_args = pDomainShaderArgs;
  params.func_name = FunctionName;
  params.bitcode = ppBitcode;
  params.error = ppError;
  NTSTATUS status;
  status = UNIX_CALL(sm50_compile_tessellation_domain, &params);
  if (status)
    return -1;
  return params.ret;
}

AIRCONV_API int
SM50CompileGeometryPipelineVertex(
    SM50Shader *pVertexShader, SM50Shader *pGeometryShader,
    struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pVertexShaderArgs, const char *FunctionName,
    SM50CompiledBitcode **ppBitcode, SM50Error **ppError
) {
  struct sm50_compile_geometry_pipeline_vertex_params params;
  params.vertex = pVertexShader;
  params.geometry = pGeometryShader;
  params.vertex_args = pVertexShaderArgs;
  params.func_name = FunctionName;
  params.bitcode = ppBitcode;
  params.error = ppError;
  NTSTATUS status;
  status = UNIX_CALL(sm50_compile_geometry_pipeline_vertex, &params);
  if (status)
    return -1;
  return params.ret;
}

AIRCONV_API int
SM50CompileGeometryPipelineGeometry(
    SM50Shader *pVertexShader, SM50Shader *pGeometryShader,
    struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pGeometryShaderArgs, const char *FunctionName,
    SM50CompiledBitcode **ppBitcode, SM50Error **ppError
) {
  struct sm50_compile_geometry_pipeline_geometry_params params;
  params.vertex = pVertexShader;
  params.geometry = pGeometryShader;
  params.geometry_args = pGeometryShaderArgs;
  params.func_name = FunctionName;
  params.bitcode = ppBitcode;
  params.error = ppError;
  NTSTATUS status;
  status = UNIX_CALL(sm50_compile_geometry_pipeline_geometry, &params);
  if (status)
    return -1;
  return params.ret;
}