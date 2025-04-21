#include "stddef.h"
#include "airconv_public.h"

#ifndef __WINEMETAL_UNIX_THUNKS_H
#define __WINEMETAL_UNIX_THUNKS_H

enum airconv_unixcalls {
  unix_sm50_initialize = 74,
  unix_sm50_destroy,
  unix_sm50_compile,
  unix_sm50_get_compiled_bitcode,
  unix_sm50_destroy_bitcode,
  unix_sm50_get_error_message,
  unix_sm50_free_error,
  unix_sm50_compile_geometry_pipeline_vertex,
  unix_sm50_compile_geometry_pipeline_geometry,
  unix_sm50_compile_tessellation_vertex,
  unix_sm50_compile_tessellation_hull,
  unix_sm50_compile_tessellation_domain,
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

struct sm50_get_compiled_bitcode_params {
  sm50_compiled_bitcode_t bitcode;
  struct MTL_SHADER_BITCODE *data_out;
};

struct sm50_destroy_bitcode_params {
  sm50_compiled_bitcode_t bitcode;
};

struct sm50_get_error_message_params {
  sm50_error_t error;
  char *ret_message;
};

struct sm50_free_error_params {
  sm50_error_t error;
};

struct sm50_compile_geometry_pipeline_vertex_params {
  sm50_shader_t vertex;
  sm50_shader_t geometry;
  void *vertex_args;
  const char *func_name;
  sm50_compiled_bitcode_t *bitcode;
  sm50_error_t *error;
  int ret;
};

struct sm50_compile_geometry_pipeline_geometry_params {
  sm50_shader_t vertex;
  sm50_shader_t geometry;
  void *geometry_args;
  const char *func_name;
  sm50_compiled_bitcode_t *bitcode;
  sm50_error_t *error;
  int ret;
};

struct sm50_compile_tessellation_pipeline_vertex_params {
  sm50_shader_t vertex;
  sm50_shader_t hull;
  void *vertex_args;
  const char *func_name;
  sm50_compiled_bitcode_t *bitcode;
  sm50_error_t *error;
  int ret;
};

struct sm50_compile_tessellation_pipeline_hull_params {
  sm50_shader_t vertex;
  sm50_shader_t hull;
  void *hull_args;
  const char *func_name;
  sm50_compiled_bitcode_t *bitcode;
  sm50_error_t *error;
  int ret;
};

struct sm50_compile_tessellation_pipeline_domain_params {
  sm50_shader_t hull;
  sm50_shader_t domain;
  void *domain_args;
  const char *func_name;
  sm50_compiled_bitcode_t *bitcode;
  sm50_error_t *error;
  int ret;
};

#define UNIX_CALL(code, params) WINE_UNIX_CALL(unix_##code, params)

#endif