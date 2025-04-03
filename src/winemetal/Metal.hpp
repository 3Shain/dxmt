#pragma once

#include "./winemetal.h"
#include <type_traits>

namespace WMT {

class Object {
public:
  obj_handle_t handle;

  void
  retain() {
    NSObject_retain(handle);
  }

  void
  release() {
    NSObject_release(handle);
  }
};

template <typename T>
concept ObjectType = std::is_base_of_v<Object, T>;

template <ObjectType Class> class Reference : public Class {
public:
  Reference(obj_handle_t retained) {
    this->handle = retained;
  }
  Reference(Class non_retained) {
    this->handle = non_retained.handle;
    this->retain();
  }
  Reference(const Reference &copy) {
    this->handle = copy.handle;
    this->retain();
  }

  Reference(Reference &&move) {
    if (this->handle != NULL_OBJECT_HANDLE)
      this->release();
    this->handle = move.handle;
    move.handle = NULL_OBJECT_HANDLE;
  }

  ~Reference() {
    if (this->handle == NULL_OBJECT_HANDLE)
      return;
    this->release();
    this->handle = NULL_OBJECT_HANDLE;
  }
};

template <ObjectType Class> class Array : public Object {
public:
  uint64_t
  count() const {
    return NSArray_count(handle);
  }

  Class
  object(uint64_t index) const {
    return Class{NSArray_object(handle, index)};
  }
};

class String : public Object {
public:
  bool getCString(char * buffer, uint64_t maxLength, WMTStringEncoding encoding) {
    return NSString_getCString(handle, buffer, maxLength, encoding);
  }
};

class Device : public Object {
public:
  uint64_t
  recommendedMaxWorkingSetSize() const {
    return MTLDevice_recommendedMaxWorkingSetSize(handle);
  }

  uint64_t
  currentAllocatedSize() const {
    return MTLDevice_currentAllocatedSize(handle);
  }

  String
  name() const {
    return String{MTLDevice_name(handle)};
  }
};

inline Reference<Array<Device>>
CopyAllDevices() {
  return Reference<Array<Device>>(WMTCopyAllDevices());
}

} // namespace WMT