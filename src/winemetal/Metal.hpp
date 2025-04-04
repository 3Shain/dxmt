#pragma once

#include "./winemetal.h"
#include <type_traits>

namespace WMT {

class Object {
public:
  obj_handle_t handle;

  Object() {
    handle = NULL_OBJECT_HANDLE;
  }

  Object(obj_handle_t h) {
    handle = h;
  }

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
  Reference() {
    this->handle = NULL_OBJECT_HANDLE;
  }
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

  Reference &
  operator=(Class non_retained) {
    this->handle = non_retained.handle;
    this->retain();
    return *this;
  }
  Reference &
  operator=(const Reference &copy) {
    this->handle = copy.handle;
    this->retain();
    return *this;
  }
  Reference &
  operator=(Reference &&move) {
    if (this->handle != NULL_OBJECT_HANDLE)
      this->release();
    this->handle = move.handle;
    move.handle = NULL_OBJECT_HANDLE;
    return *this;
  }
  Reference &
  operator=(std::nullptr_t) {
    if (this->handle != NULL_OBJECT_HANDLE)
      this->release();
    this->handle = NULL_OBJECT_HANDLE;
    return *this;
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
  bool
  getCString(char *buffer, uint64_t maxLength, WMTStringEncoding encoding) {
    return NSString_getCString(handle, buffer, maxLength, encoding);
  }
};

class Event : public Object {
public:
};

class SharedEvent : public Event {
public:
  uint64_t
  signaledValue() {
    return MTLSharedEvent_signaledValue(handle);
  }
};

class CommandBuffer : public Object {
public:
  void
  commit() {
    return MTLCommandBuffer_commit(handle);
  }

  void
  waitUntilCompleted() {
    return MTLCommandBuffer_waitUntilCompleted(handle);
  }

  WMTCommandBufferStatus
  status() {
    return MTLCommandBuffer_status(handle);
  }

  void
  encodeSignalEvent(Event event, uint64_t value) {
    return MTLCommandBuffer_encodeSignalEvent(handle, event.handle, value);
  }
};

class CommandQueue : public Object {
public:
  CommandBuffer
  commandBuffer() {
    return CommandBuffer{MTLCommandQueue_commandBuffer(handle)};
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

  Reference<CommandQueue>
  newCommandQueue(uint64_t maxCommandBufferCount) {
    return Reference<CommandQueue>(MTLDevice_newCommandQueue(handle, maxCommandBufferCount));
  }

  Reference<SharedEvent>
  newSharedEvent() {
    return Reference<SharedEvent>(MTLDevice_newSharedEvent(handle));
  }
};

inline Reference<Array<Device>>
CopyAllDevices() {
  return Reference<Array<Device>>(WMTCopyAllDevices());
}

inline Reference<Object>
MakeAutoreleasePool() {
  return Reference<Object>(NSAutoreleasePool_alloc_init());
}

} // namespace WMT