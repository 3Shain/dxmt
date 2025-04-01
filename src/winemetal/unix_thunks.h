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

struct get_gamma_ramp_capacity_params {
  uint32_t display_id;
  uint32_t capacity;
};

struct set_gamma_ramp_params {
  uint32_t display_id;
  uint32_t table_size;
  float *red_table;
  float *green_table;
  float *blue_table;
  int status;
};

struct get_gamma_ramp_params {
  uint32_t display_id;
  uint32_t capacity;
  float *red_table;
  float *green_table;
  float *blue_table;
  uint32_t *num_samples;
  int status;
};

struct get_display_by_rect_params {
  int x;
  int y;
  int width;
  int height;
  uint32_t *display_id;
  int status;
};

#define UNIX_CALL(code, params) WINE_UNIX_CALL(unix_##code, params)

#endif