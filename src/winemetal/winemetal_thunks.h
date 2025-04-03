
#ifndef __WINEMETAL_THUNKS_H
#define __WINEMETAL_THUNKS_H

#include "winemetal.h"

struct unixcall_generic_obj_ret {
  obj_handle_t ret;
};

struct unixcall_generic_obj_obj_ret {
  obj_handle_t handle;
  obj_handle_t ret;
};

struct unixcall_generic_obj_uint64_obj_ret {
  obj_handle_t handle;
  uint64_t arg;
  obj_handle_t ret;
};

struct unixcall_generic_obj_uint64_ret {
  obj_handle_t handle;
  uint64_t ret;
};

#endif
