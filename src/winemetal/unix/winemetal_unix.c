#import <Metal/Metal.h>
#define WINEMETAL_API
#include "../winemetal_thunks.h"

typedef int NTSTATUS;
#define STATUS_SUCCESS 0

static NTSTATUS
_NSObject_retain(NSObject **obj) {
  [*obj retain];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSObject_release(NSObject **obj) {
  [*obj release];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSArray_object(void *obj) {
  struct unixcall_generic_obj_uint64_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(NSArray *)params->handle objectAtIndex:params->arg];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSArray_count(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(NSArray *)params->handle count];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLCopyAllDevices(void *obj) {
  struct unixcall_generic_obj_ret *params = obj;
  params->ret = (obj_handle_t)MTLCopyAllDevices();
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_recommendedMaxWorkingSetSize(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLDevice>)params->handle recommendedMaxWorkingSetSize];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_currentAllocatedSize(void *obj) {
  struct unixcall_generic_obj_uint64_ret *params = obj;
  params->ret = [(id<MTLDevice>)params->handle currentAllocatedSize];
  return STATUS_SUCCESS;
}

static NTSTATUS
_MTLDevice_name(void *obj) {
  struct unixcall_generic_obj_obj_ret *params = obj;
  params->ret = (obj_handle_t)[(id<MTLDevice>)params->handle name];
  return STATUS_SUCCESS;
}

static NTSTATUS
_NSString_getCString(void *obj) {
  struct unixcall_nsstring_getcstring *params = obj;
  params->ret = (uint32_t)[(NSString *)params->str getCString:(char *)params->buffer_ptr
                                                    maxLength:params->max_length
                                                     encoding:params->encoding];
  return STATUS_SUCCESS;
}

const void *__winemetal_unixcalls[] = {
    &_NSObject_retain,
    &_NSObject_release,
    &_NSArray_object,
    &_NSArray_count,
    &_MTLCopyAllDevices,
    &_MTLDevice_recommendedMaxWorkingSetSize,
    &_MTLDevice_currentAllocatedSize,
    &_MTLDevice_name,
    &_NSString_getCString,
};

const unsigned int __winemetal_unixcalls_num = sizeof(__winemetal_unixcalls) / sizeof(void *);