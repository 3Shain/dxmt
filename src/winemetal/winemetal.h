#ifndef _WINEMETAL_H
#define _WINEMETAL_H

#ifdef __cplusplus
#include <cstdint>
#include <cstdbool>
#else
#include <stdint.h>
#include <stdbool.h>
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

#endif