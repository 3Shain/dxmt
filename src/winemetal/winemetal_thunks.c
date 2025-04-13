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

WINEMETAL_API obj_handle_t
MTLDevice_newBuffer(obj_handle_t device, struct WMTBufferInfo *info) {
  struct unixcall_mtldevice_newbuffer params;
  params.device = device;
  params.info = info;
  UNIX_CALL(18, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLDevice_newSamplerState(obj_handle_t device, struct WMTSamplerInfo *info) {
  struct unixcall_mtldevice_newsamplerstate params;
  params.device = device;
  params.info = info;
  UNIX_CALL(19, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLDevice_newDepthStencilState(obj_handle_t device, struct WMTDepthStencilInfo *info) {
  struct unixcall_mtldevice_newdepthstencilstate params;
  params.device = device;
  params.info = info;
  UNIX_CALL(20, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLDevice_newTexture(obj_handle_t device, struct WMTTextureInfo *info) {
  struct unixcall_mtldevice_newtexture params;
  params.device = device;
  params.info = info;
  UNIX_CALL(21, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLBuffer_newTexture(obj_handle_t buffer, struct WMTTextureInfo *info, uint64_t offset, uint64_t bytes_per_row) {
  struct unixcall_mtlbuffer_newtexture params;
  params.buffer = buffer;
  params.info = info;
  params.offset = offset;
  params.bytes_per_row = bytes_per_row;
  UNIX_CALL(22, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLTexture_newTextureView(
    obj_handle_t texture, enum WMTPixelFormat format, enum WMTTextureType texture_type, uint16_t level_start,
    uint16_t level_count, uint16_t slice_start, uint16_t slice_count, struct WMTTextureSwizzleChannels swizzle,
    uint64_t *out_gpu_resource_id
) {
  struct unixcall_mtltexture_newtextureview params;
  params.texture = texture;
  params.format = format;
  params.texture_type = texture_type;
  params.level_start = level_start;
  params.level_count = level_count;
  params.slice_start = slice_start;
  params.slice_count = slice_count;
  params.swizzle = swizzle;
  UNIX_CALL(23, &params);
  *out_gpu_resource_id = params.gpu_resource_id;
  return params.ret;
}

WINEMETAL_API uint64_t
MTLDevice_minimumLinearTextureAlignmentForPixelFormat(obj_handle_t device, enum WMTPixelFormat format) {
  struct unixcall_generic_obj_uint64_uint64_ret params;
  params.handle = device;
  params.arg = (uint64_t)format;
  params.ret = 0;
  UNIX_CALL(24, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLDevice_newLibrary(
    obj_handle_t device, struct WMTMemoryPointer bytecode, uint64_t bytecode_length, obj_handle_t *err_out
) {
  struct unixcall_mtldevice_newlibrary params;
  params.device = device;
  params.bytecode = bytecode;
  params.bytecode_length = bytecode_length;
  params.ret_error = 0;
  params.ret_library = 0;
  UNIX_CALL(25, &params);
  if (err_out)
    *err_out = params.ret_error;
  return params.ret_library;
}

WINEMETAL_API obj_handle_t
MTLLibrary_newFunction(obj_handle_t library, const char *name) {
  struct unixcall_generic_obj_uint64_obj_ret params;
  params.handle = library;
  params.arg = (uint64_t)name;
  params.ret = 0;
  UNIX_CALL(26, &params);
  return params.ret;
}

WINEMETAL_API uint64_t
NSString_lengthOfBytesUsingEncoding(obj_handle_t str, enum WMTStringEncoding encoding) {
  struct unixcall_generic_obj_uint64_uint64_ret params;
  params.handle = str;
  params.arg = (uint64_t)(encoding);
  params.ret = 0;
  UNIX_CALL(27, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
NSError_description(obj_handle_t nserror) {
  struct unixcall_generic_obj_obj_ret params;
  params.handle = nserror;
  UNIX_CALL(28, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLDevice_newComputePipelineState(obj_handle_t device, obj_handle_t function, obj_handle_t *err_out) {
  struct unixcall_mtldevice_newcomputepso params;
  params.device = device;
  params.function = function;
  params.ret_error = 0;
  params.ret_pso = 0;
  UNIX_CALL(29, &params);
  if (err_out)
    *err_out = params.ret_error;
  return params.ret_pso;
}

WINEMETAL_API obj_handle_t
MTLCommandBuffer_blitCommandEncoder(obj_handle_t cmdbuf) {
  struct unixcall_generic_obj_obj_ret params;
  params.handle = cmdbuf;
  params.ret = 0;
  UNIX_CALL(30, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLCommandBuffer_computeCommandEncoder(obj_handle_t cmdbuf, bool concurrent) {
  struct unixcall_generic_obj_uint64_obj_ret params;
  params.handle = cmdbuf;
  params.arg = concurrent;
  params.ret = 0;
  UNIX_CALL(31, &params);
  return params.ret;
}

WINEMETAL_API obj_handle_t
MTLCommandBuffer_renderCommandEncoder(obj_handle_t cmdbuf, struct WMTRenderPassInfo *info) {
  struct unixcall_generic_obj_uint64_obj_ret params;
  params.handle = cmdbuf;
  params.arg = (uint64_t)info;
  params.ret = 0;
  UNIX_CALL(32, &params);
  return params.ret;
}

WINEMETAL_API void
MTLCommandEncoder_endEncoding(obj_handle_t encoder) {
  struct unixcall_generic_obj_noret params;
  params.handle = encoder;
  UNIX_CALL(33, &params);
  return;
}