#ifndef _WINEMETAL_H
#define _WINEMETAL_H

#ifdef __cplusplus
#include <cstdint>
#include <cstdbool>
#define STATIC_ASSERT(x) static_assert(x)
#else
#include <stdint.h>
#include <stdbool.h>
#define STATIC_ASSERT(x)
#endif

#ifndef WINEMETAL_API
#ifdef __cplusplus
#define WINEMETAL_API extern "C" __declspec(dllimport)
#else
#define WINEMETAL_API __declspec(dllimport)
#endif
#endif

typedef uint64_t obj_handle_t;

#define NULL_OBJECT_HANDLE 0

WINEMETAL_API void NSObject_retain(obj_handle_t obj);

WINEMETAL_API void NSObject_release(obj_handle_t obj);

WINEMETAL_API obj_handle_t NSArray_object(obj_handle_t array, uint64_t index);

WINEMETAL_API uint64_t NSArray_count(obj_handle_t array);

WINEMETAL_API obj_handle_t WMTCopyAllDevices();

enum WMTStringEncoding : uint64_t {
  WMTASCIIStringEncoding = 1,
  WMTNEXTSTEPStringEncoding = 2,
  WMTJapaneseEUCStringEncoding = 3,
  WMTUTF8StringEncoding = 4,
  WMTWMTISOLatin1StringEncoding = 5,
  SymbolStringEncoding = 6,
  WMTNonLossyASCIIStringEncoding = 7,
  WMTShiftJISStringEncoding = 8,
  WMTISOLatin2StringEncoding = 9,
  WMTUnicodeStringEncoding = 10,
  WMTWindowsCP1251StringEncoding = 11,
  WMTWindowsCP1252StringEncoding = 12,
  WMTWindowsCP1253StringEncoding = 13,
  WMTWindowsCP1254StringEncoding = 14,
  WMTWindowsCP1250StringEncoding = 15,
  WMTISO2022JPStringEncoding = 21,
  WMTMacOSRomanStringEncoding = 30,

  WMTUTF16StringEncoding = WMTUnicodeStringEncoding,

  WMTUTF16BigEndianStringEncoding = 0x90000100,
  WMTUTF16LittleEndianStringEncoding = 0x94000100,

  WMTUTF32StringEncoding = 0x8c000100,
  WMTUTF32BigEndianStringEncoding = 0x98000100,
  WMTUTF32LittleEndianStringEncoding = 0x9c000100
};

WINEMETAL_API uint64_t MTLDevice_recommendedMaxWorkingSetSize(obj_handle_t device);

WINEMETAL_API uint64_t MTLDevice_currentAllocatedSize(obj_handle_t device);

WINEMETAL_API obj_handle_t MTLDevice_name(obj_handle_t device);

WINEMETAL_API uint32_t
NSString_getCString(obj_handle_t str, char *buffer, uint64_t maxLength, enum WMTStringEncoding encoding);

WINEMETAL_API obj_handle_t MTLDevice_newCommandQueue(obj_handle_t device, uint64_t maxCommandBufferCount);

WINEMETAL_API obj_handle_t NSAutoreleasePool_alloc_init();

WINEMETAL_API obj_handle_t MTLCommandQueue_commandBuffer(obj_handle_t queue);

WINEMETAL_API void MTLCommandBuffer_commit(obj_handle_t cmdbuf);

WINEMETAL_API void MTLCommandBuffer_waitUntilCompleted(obj_handle_t cmdbuf);

enum WMTCommandBufferStatus : uint64_t {
  WMTCommandBufferStatusNotEnqueued = 0,
  WMTCommandBufferStatusEnqueued = 1,
  WMTCommandBufferStatusCommitted = 2,
  WMTCommandBufferStatusScheduled = 3,
  WMTCommandBufferStatusCompleted = 4,
  WMTCommandBufferStatusError = 5,
};

WINEMETAL_API enum WMTCommandBufferStatus MTLCommandBuffer_status(obj_handle_t cmdbuf);

WINEMETAL_API obj_handle_t MTLDevice_newSharedEvent(obj_handle_t device);

WINEMETAL_API uint64_t MTLSharedEvent_signaledValue(obj_handle_t event);

WINEMETAL_API void MTLCommandBuffer_encodeSignalEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value);

enum WMTResourceOptions : uint64_t {
  WMTResourceCPUCacheModeDefaultCache = 0,
  WMTResourceCPUCacheModeWriteCombined = 1,
  WMTResourceStorageModeShared = 0,
  WMTResourceStorageModeManaged = 16,
  WMTResourceStorageModePrivate = 32,
  WMTResourceStorageModeMemoryless = 48,
  WMTResourceHazardTrackingModeDefault = 0,
  WMTResourceHazardTrackingModeUntracked = 256,
  WMTResourceHazardTrackingModeTracked = 512,
  WMTResourceOptionCPUCacheModeDefault = 0,
  WMTResourceOptionCPUCacheModeWriteCombined = 1,
};

struct WMTMemoryPointer {
  void *ptr;
#if defined(__i386__)
  uint32_t high_part;
#endif

#ifdef __cplusplus
  void
  set(void *p) {
#if defined(__i386__)
    high_part = 0;
#endif
    ptr = p;
  }

  void *
  get() {
#if defined(__i386__)
    assert(!high_part && "inaccessible 64-bit pointer");
#endif
    return ptr;
  }
#endif
};

STATIC_ASSERT(sizeof(WMTMemoryPointer) == 8);

struct WMTBufferInfo {
  uint64_t length;                 // in
  enum WMTResourceOptions options; // in
  struct WMTMemoryPointer memory;  // inout
  uint64_t gpu_address;            // out
};

STATIC_ASSERT(sizeof(WMTBufferInfo) == 32);

WINEMETAL_API obj_handle_t MTLDevice_newBuffer(obj_handle_t device, struct WMTBufferInfo *info);

#endif