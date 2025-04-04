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

WINEMETAL_API uint64_t
MTLDevice_recommendedMaxWorkingSetSize(obj_handle_t device) {
  struct unixcall_generic_obj_uint64_ret params;
  params.handle = device;
  params.ret = 0;
  UNIX_CALL(5, &params);
  return params.ret;
}

WINEMETAL_API uint64_t
MTLDevice_currentAllocatedSize(obj_handle_t device) {
  struct unixcall_generic_obj_uint64_ret params;
  params.handle = device;
  params.ret = 0;
  UNIX_CALL(6, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLDevice_name(obj_handle_t device) {
  struct unixcall_generic_obj_obj_ret params;
  params.handle = device;
  params.ret = 0;
  UNIX_CALL(7, &params);
  return params.ret;
}

WINEMETAL_API uint32_t
NSString_getCString(obj_handle_t str, char *buffer, uint64_t maxLength, enum WMTStringEncoding encoding) {
  struct unixcall_nsstring_getcstring params;
  params.str = str;
  params.buffer_ptr = (uint64_t)(buffer);
  params.max_length = maxLength;
  params.encoding = encoding;
  UNIX_CALL(8, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLDevice_newCommandQueue(obj_handle_t device, uint64_t maxCommandBufferCount) {
  struct unixcall_generic_obj_uint64_obj_ret params;
  params.handle = device;
  params.arg = maxCommandBufferCount;
  params.ret = 0;
  UNIX_CALL(9, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
NSAutoreleasePool_alloc_init() {
  struct unixcall_generic_obj_ret params;
  params.ret = 0;
  UNIX_CALL(10, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLCommandQueue_commandBuffer(obj_handle_t queue) {
  struct unixcall_generic_obj_obj_ret params;
  params.handle = queue;
  params.ret = 0;
  UNIX_CALL(11, &params);
  return params.ret;
}

WINEMETAL_API void
MTLCommandBuffer_commit(obj_handle_t cmdbuf) {
  struct unixcall_generic_obj_noret params;
  params.handle = cmdbuf;
  UNIX_CALL(12, &params);
  return;
}

WINEMETAL_API void
MTLCommandBuffer_waitUntilCompleted(obj_handle_t cmdbuf) {
  struct unixcall_generic_obj_noret params;
  params.handle = cmdbuf;
  UNIX_CALL(13, &params);
  return;
}

WINEMETAL_API enum WMTCommandBufferStatus
MTLCommandBuffer_status(obj_handle_t cmdbuf) {
  struct unixcall_generic_obj_uint64_ret params;
  params.handle = cmdbuf;
  params.ret = 0;
  UNIX_CALL(14, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLDevice_newSharedEvent(obj_handle_t device) {
  struct unixcall_generic_obj_obj_ret params;
  params.handle = device;
  params.ret = 0;
  UNIX_CALL(15, &params);
  return params.ret;
}

WINEMETAL_API uint64_t
MTLSharedEvent_signaledValue(obj_handle_t event) {
  struct unixcall_generic_obj_uint64_ret params;
  params.handle = event;
  params.ret = 0;
  UNIX_CALL(16, &params);
  return params.ret;
}

WINEMETAL_API void
MTLCommandBuffer_encodeSignalEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value) {
  struct unixcall_generic_obj_obj_uint64_noret params;
  params.handle = cmdbuf;
  params.arg0 = event;
  params.arg1 = value;
  UNIX_CALL(17, &params);
  return;
}
