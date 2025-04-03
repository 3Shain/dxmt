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
    this->handle = non_retained;
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
  count() {
    return NSArray_count(handle);
  }

  Class
  object(uint64_t index) {
    return Class{NSArray_object(handle, index)};
  }
};

class Device : public Object {
public:
};

inline Reference<Array<Device>>
CopyAllDevices() {
  return Reference<Array<Device>>(WMTCopyAllDevices());
}

} // namespace WMT