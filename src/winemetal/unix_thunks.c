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

__attribute__((dllexport)) UINT CGGetDisplayGammaRampCapacity(UINT DisplayId) {
  struct get_gamma_ramp_capacity_params p;
  p.display_id = DisplayId;
  WINE_UNIX_CALL(5, &p);
  return p.capacity;
};

__attribute__((dllexport)) int CGSetDisplayGammaRamp(UINT DisplayId,
                                                     UINT TableSize,
                                                     float *RedTable,
                                                     float *GreenTable,
                                                     float *BlueTable) {
  struct set_gamma_ramp_params p;
  p.display_id = DisplayId;
  p.table_size = TableSize;
  p.red_table = RedTable;
  p.green_table = GreenTable;
  p.blue_table = BlueTable;
  WINE_UNIX_CALL(6, &p);
  return p.status;
};

__attribute__((dllexport)) int CGGetDisplayGammaRamp(UINT DisplayId,
                                                     UINT Capacity,
                                                     float *RedTable,
                                                     float *GreenTable,
                                                     float *BlueTable,
                                                     UINT *pNumSamples) {
  struct get_gamma_ramp_params p;
  p.display_id = DisplayId;
  p.capacity = Capacity;
  p.red_table = RedTable;
  p.green_table = GreenTable;
  p.blue_table = BlueTable;
  p.num_samples = pNumSamples;
  WINE_UNIX_CALL(7, &p);
  return p.status;
};

__attribute__((dllexport)) int CGGetDisplayIdWithRect(int X,
                                                      int Y,
                                                      int Width,
                                                      int Height,
                                                      UINT *pDisplayId) {
  struct get_display_by_rect_params p;
  p.x = X;
  p.y = Y;
  p.width = Width;
  p.height = Height;
  p.display_id = pDisplayId;
  WINE_UNIX_CALL(27, &p);
  return p.status;
};
