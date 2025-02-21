#define WIN_EXPORT
#include "wineunixlib.h"
#include "unix_thunks.h"

AIRCONV_API int SM50Initialize(const void *pBytecode, size_t BytecodeSize,
                               SM50Shader **ppShader,
                               struct MTL_SHADER_REFLECTION *pRefl,
                               SM50Error **ppError) {
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

AIRCONV_API void SM50Destroy(SM50Shader *pShader) {
  struct sm50_destroy_params params;
  params.shader = pShader;
  UNIX_CALL(sm50_destroy, &params);
}

AIRCONV_API int SM50Compile(SM50Shader *pShader,
                            struct SM50_SHADER_COMPILATION_ARGUMENT_DATA *pArgs,
                            const char *FunctionName,
                            SM50CompiledBitcode **ppBitcode,
                            SM50Error **ppError) {
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

__attribute__((dllexport)) void* CreateMTLFXTemporalScaler(void * d, void * device) {
  struct create_fxscaler_params p;
  p.desc = d;
  p.device = device;
  p.ret = NULL;
  WINE_UNIX_CALL(4, &p);
  return p.ret;
};
