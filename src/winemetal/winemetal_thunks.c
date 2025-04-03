#define WINEMETAL_API __declspec(dllexport)

#include "winemetal_thunks.h"
#include "wineunixlib.h"
#include "assert.h"

#define UNIX_CALL(code, params)                                                                                        \
  {                                                                                                                    \
    NTSTATUS status = WINE_UNIX_CALL(0x50 + code, params);                                                             \
    assert(!status && "unix call failed");                                                                             \
  }

WINEMETAL_API void
NSObject_retain(obj_handle_t obj) {
  UNIX_CALL(0, &obj);
}

WINEMETAL_API void
NSObject_release(obj_handle_t obj) {
  UNIX_CALL(1, &obj);
}

WINEMETAL_API obj_handle_t
NSArray_object(obj_handle_t array, uint64_t index) {
  struct unixcall_generic_obj_uint64_obj_ret params;
  params.handle = array;
  params.arg = index;
  params.ret = 0;
  UNIX_CALL(2, &params);
  return params.ret;
}

WINEMETAL_API uint64_t
NSArray_count(obj_handle_t array) {
  struct unixcall_generic_obj_uint64_ret params;
  params.handle = array;
  params.ret = 0;
  UNIX_CALL(3, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
WMTCopyAllDevices() {
  struct unixcall_generic_obj_ret params;
  params.ret = 0;
  UNIX_CALL(4, &params);
  return params.ret;
}