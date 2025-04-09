#pragma once

#include "./winemetal.h"
#include <cstddef>

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

template <typename Class> class Reference : public Class {
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

template <typename Class> class Array : public Object {
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

class Resource : public Object {
public:
};

class Texture : public Resource {
public:
  Reference<Texture>
  newTextureView(
      WMTPixelFormat format, WMTTextureType texture_type, uint16_t level_start, uint16_t level_count,
      uint16_t slice_start, uint16_t slice_count, struct WMTTextureSwizzleChannels swizzle,
      uint64_t *out_gpu_resource_id
  ) {
    return Reference<Texture>(MTLTexture_newTextureView(
        handle, format, texture_type, level_start, level_count, slice_start, slice_count, swizzle, out_gpu_resource_id
    ));
  }
};

class Buffer : public Resource {
public:
  Reference<Texture>
  newTexture(WMTTextureInfo *info, uint64_t offset, uint64_t bytes_per_row) {
    return Reference<Texture>(MTLBuffer_newTexture(handle, info, offset, bytes_per_row));
  }
};

class SamplerState : public Object {
public:
};

class DepthStencilState : public Object {
public:
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

  Reference<Buffer>
  newBuffer(WMTBufferInfo *info) {
    return Reference<Buffer>(MTLDevice_newBuffer(handle, info));
  }

  Reference<SamplerState>
  newSamplerState(WMTSamplerInfo *info) {
    return Reference<SamplerState>(MTLDevice_newSamplerState(handle, info));
  }

  Reference<DepthStencilState>
  newDepthStencilState(WMTDepthStencilInfo *info) {
    return Reference<DepthStencilState>(MTLDevice_newDepthStencilState(handle, info));
  }

  Reference<Texture>
  newTexture(WMTTextureInfo *info) {
    return Reference<Texture>(MTLDevice_newTexture(handle, info));
  }

  uint64_t
  minimumLinearTextureAlignmentForPixelFormat(WMTPixelFormat format) {
    return MTLDevice_minimumLinearTextureAlignmentForPixelFormat(handle, format);
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