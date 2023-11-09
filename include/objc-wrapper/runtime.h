#pragma once
#include "abi.h"

/// An opaque type that represents an Objective-C class.
typedef struct objc_class *Class;

/// Represents an instance of a class.
struct objc_object {
  Class isa;
};

/// A pointer to an instance of a class.
typedef struct objc_object *id;

/// An opaque type that represents a method selector.
typedef struct objc_selector *SEL;

typedef struct objc_object Protocol;

#ifdef __cplusplus
extern "C" {
#endif
extern Class SYSV_ABI objc_lookUpClass(const char *name);
extern SEL SYSV_ABI sel_registerName(const char *str);
extern Protocol *SYSV_ABI objc_getProtocol(const char *name);
#ifdef __cplusplus
}
#endif