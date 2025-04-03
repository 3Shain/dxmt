#ifndef _WINEMETAL_H
#define _WINEMETAL_H

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
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

#endif