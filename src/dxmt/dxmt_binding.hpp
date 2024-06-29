#pragma once

#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "Metal/MTLTypes.hpp"
#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"

DEFINE_COM_INTERFACE("12c69ed2-ebae-438d-ac9c-ecdb7c08065b", BackBufferSource)
    : public IUnknown {
  virtual MTL::Texture *GetCurrentFrameBackBuffer() = 0;
};

namespace dxmt {

struct EncodingContext {
  // virtual MTL::Texture *GetCurrentSwapchainBackbuffer() = 0;
};

struct tag_swapchain_backbuffer_t {};
constexpr tag_swapchain_backbuffer_t tag_swapchain_backbuffer{};

class BindingRef {
  enum class Type : uint64_t {
    Null = 0,
    UnboundedBuffer = 0b1,
    BoundedBuffer = 0b11,
    UAVWithCounter = 0b111,
    JustTexture = 0b10000000,
    BackBufferSource = 0b100000000,
  };
  Type type;
  MTL::Resource *resource_ptr;
  MTL::Buffer *counter_;
  uint32_t element_width = 0;
  uint32_t byte_offset = 0;
  Com<IUnknown> reference_holder = nullptr;

public:
  BindingRef() noexcept : type(Type::Null) {};
  BindingRef(IUnknown *ref, MTL::Buffer *buffer) noexcept
      : type(Type::UnboundedBuffer), resource_ptr(buffer),
        reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Buffer *buffer, uint32_t element_width,
             uint32_t offset) noexcept
      : type(Type::BoundedBuffer), resource_ptr(buffer),
        element_width(element_width), byte_offset(offset),
        reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Buffer *buffer, uint32_t element_width,
             uint32_t offset, MTL::Buffer *counter) noexcept
      : type(Type::UAVWithCounter), resource_ptr(buffer), counter_(counter),
        element_width(element_width), byte_offset(offset),
        reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Texture *texture) noexcept
      : type(Type::JustTexture), resource_ptr(texture), reference_holder(ref) {}
  BindingRef(std::nullopt_t _) noexcept : type(Type::Null) {};
  BindingRef(BackBufferSource *t) noexcept
      : type(Type::BackBufferSource), reference_holder(t) {}

  operator bool() const noexcept { return type != Type::Null; }
  BindingRef(const BindingRef &copy) = delete;
  BindingRef(BindingRef &&move) {
    type = move.type;
    resource_ptr = move.resource_ptr;
    counter_ = move.counter_;
    element_width = move.element_width;
    byte_offset = move.byte_offset;
    reference_holder = std::move(move.reference_holder);
    move.type = Type::Null;
    move.byte_offset = 0;
    move.element_width = 0;
    // move.reference_holder = nullptr;
  };

  BindingRef &operator=(BindingRef &&move) {
    // ahh this is redunant
    type = move.type;
    resource_ptr = move.resource_ptr;
    counter_ = move.counter_;
    element_width = move.element_width;
    byte_offset = move.byte_offset;
    reference_holder = std::move(move.reference_holder);
    move.type = Type::Null;
    move.byte_offset = 0;
    move.element_width = 0;
    // move.reference_holder = 0;
    return *this;
  };

  ~BindingRef() { type = Type::Null; }

  bool operator==(const BindingRef &other) const {
    if (type != other.type) {
      return false;
    }
    switch (type) {
    case Type::Null:
      return true; // FIXME: is this intended?
    case Type::JustTexture:
    case Type::UnboundedBuffer:
      return resource_ptr == other.resource_ptr &&
             byte_offset == other.byte_offset;
      ;
    case Type::BoundedBuffer:
      return resource_ptr == other.resource_ptr &&
             byte_offset == other.byte_offset &&
             element_width == other.element_width;
    case Type::UAVWithCounter:
      return resource_ptr == other.resource_ptr &&
             byte_offset == other.byte_offset &&
             element_width == other.element_width && counter_ == other.counter_;
    case Type::BackBufferSource:
      return reference_holder == other.reference_holder;
    }
  };

  bool requiresContext() const { return type == Type::BackBufferSource; };

  MTL::Buffer *buffer() const {
    if ((uint64_t)type & (uint64_t)Type::UnboundedBuffer) {
      return (MTL::Buffer *)resource_ptr;
    }
    return nullptr;
  }
  MTL::Texture *texture() const {
    if ((uint64_t)type & (uint64_t)Type::JustTexture) {
      return (MTL::Texture *)resource_ptr;
    }
    return nullptr;
  }
  MTL::Texture *texture(EncodingContext *context) const {
    if ((uint64_t)type & (uint64_t)Type::JustTexture) {
      return (MTL::Texture *)resource_ptr;
    }
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer();
    }
    return nullptr;
  }
  uint32_t width() const {
    if (((uint64_t)type & (uint64_t)Type::BoundedBuffer) ==
        (uint64_t)Type::BoundedBuffer) {
      return element_width;
    }
    return 0;
  }

  uint32_t offset() const {
    if (((uint64_t)type & (uint64_t)Type::BoundedBuffer) ==
        (uint64_t)Type::BoundedBuffer) {
      return byte_offset;
    }
    return 0;
  }

  MTL::Buffer *counter() const {
    if (((uint64_t)type & (uint64_t)Type::UAVWithCounter) ==
        (uint64_t)Type::UAVWithCounter) {
      return counter_;
    }
    return nullptr;
  }

  MTL::Resource *resource() const { return resource_ptr; }

  MTL::Resource *resource(EncodingContext *context) const {
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer();
    }
    return resource();
  }
};

class ArgumentData {
  enum class Type : uint64_t {
    UnboundedBuffer = 0b1,
    BoundedBuffer = 0b11,
    UAVWithCounter = 0b111,
    JustTexture = 0b10000000,
    BackBufferSource = 0b100000000,
  };
  Type type;
  uint64_t resource_handle;
  uint32_t size;
  union {
    uint64_t counter_handle;
    BackBufferSource *ptr;
  };

public:
  ArgumentData(uint64_t h) noexcept
      : type(Type::UnboundedBuffer), resource_handle(h) {}
  ArgumentData(uint64_t h, uint32_t c) noexcept
      : type(Type::BoundedBuffer), resource_handle(h), size(c) {}
  ArgumentData(uint64_t h, uint32_t c, uint64_t ctr) noexcept
      : type(Type::UnboundedBuffer), resource_handle(h), size(c) {
    counter_handle = ctr;
  }
  ArgumentData(MTL::ResourceID id, MTL::Texture *) noexcept
      : type(Type::JustTexture), resource_handle(id._impl) {}
  ArgumentData(BackBufferSource *t) noexcept
      : type(Type::BackBufferSource), ptr(t) {}

  bool requiresContext() const { return type == Type::BackBufferSource; };

  uint64_t buffer() const {
    if ((uint64_t)type & (uint64_t)Type::UnboundedBuffer) {
      return resource_handle;
    }
    return 0;
  }
  uint64_t texture() const {
    if ((uint64_t)type & (uint64_t)Type::JustTexture) {
      return resource_handle;
    }
    return 0;
  }

  uint64_t width() const {
    if (((uint64_t)type & (uint64_t)Type::BoundedBuffer) ==
        (uint64_t)Type::BoundedBuffer) {
      return size + (((uint64_t)size) << 32);
    }
    return 0;
  }

  uint64_t counter() const {
    if (((uint64_t)type & (uint64_t)Type::UAVWithCounter) ==
        (uint64_t)Type::UAVWithCounter) {
      return counter_handle;
    }
    return 0;
  }

  uint64_t resource() const { return resource_handle; }

  uint64_t resource(EncodingContext *context) const {
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource) {
      return ptr->GetCurrentFrameBackBuffer()->gpuResourceID()._impl;
    }
    return resource();
  }
};
} // namespace dxmt