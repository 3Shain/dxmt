#include "stddef.h"
#include "airconv_public.h"

#ifndef __WINEMETAL_UNIX_THUNKS_H
#define __WINEMETAL_UNIX_THUNKS_H

enum airconv_unixcalls {
  unix_sm50_initialize = 35,
  unix_sm50_destroy,
  unix_sm50_compile,
};

typedef SM50Shader *sm50_shader_t;
typedef SM50Error *sm50_error_t;
typedef SM50CompiledBitcode *sm50_compiled_bitcode_t;

struct sm50_initialize_params {
  const void *bytecode;
  size_t bytecode_size;
  sm50_shader_t *shader;
  void *reflection;
  sm50_error_t *error;
  int ret;
};

struct sm50_destroy_params {
  sm50_shader_t shader;
};

struct sm50_compile_params {
  sm50_shader_t shader;
  void *args;
  const char *func_name;
  sm50_compiled_bitcode_t *bitcode;
  sm50_error_t *error;
  int ret;
};


struct create_fxscaler_params {
  void* desc;
  void* device;
  void* ret;
};

#define UNIX_CALL(code, params) WINE_UNIX_CALL(unix_##code, params)

#endif