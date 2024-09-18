#pragma once

#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "Metal/MTLTypes.hpp"
#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"

DEFINE_COM_INTERFACE("12c69ed2-ebae-438d-ac9c-ecdb7c08065b", BackBufferSource)
    : public IUnknown {
  virtual MTL::Texture *GetCurrentFrameBackBuffer(bool srgb) = 0;
};

#define DXMT_NO_COUNTER ~0uLL

namespace dxmt {

struct EncodingContext {
  // virtual MTL::Texture *GetCurrentSwapchainBackbuffer() = 0;
};

struct tag_swapchain_backbuffer_t {};
constexpr tag_swapchain_backbuffer_t tag_swapchain_backbuffer{};

class BindingRef {
  enum class Type : uint64_t {
    Null = 0,
    JustBuffer = 0b1,
    BoundedBuffer = 0b11,
    UAVWithCounter = 0b111,
    WithBoundInformation = 0b10,
    JustTexture = 0b10000000,
    TextureViewOfBuffer = 0b11000010,
    BackBufferSource_sRGB = 0b100000000,
    BackBufferSource_Linear = 0b1000000000,
  };
  Type type;
  MTL::Resource *resource_ptr;
  union {
    uint64_t counter_handle_;
    MTL::Buffer* buffer_ptr;
  };
  uint32_t byte_width = 0;
  uint32_t byte_offset = 0;
  Com<IUnknown> reference_holder = nullptr;

public:
  BindingRef() noexcept : type(Type::Null) {};
  BindingRef(IUnknown *ref, MTL::Buffer *buffer, uint32_t byte_width,
             uint32_t offset) noexcept
      : type(Type::BoundedBuffer), resource_ptr(buffer), byte_width(byte_width),
        byte_offset(offset), reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Buffer *buffer, uint32_t element_width,
             uint32_t offset, uint64_t counter_handle) noexcept
      : type(Type::UAVWithCounter), resource_ptr(buffer), counter_handle_(counter_handle),
        byte_width(element_width), byte_offset(offset), reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Texture *texture) noexcept
      : type(Type::JustTexture), resource_ptr(texture), reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Texture *texture, MTL::Buffer *buffer,
             uint32_t byte_width, uint32_t byte_offset) noexcept
      : type(Type::TextureViewOfBuffer), resource_ptr(texture),
        buffer_ptr(buffer), byte_width(byte_width), byte_offset(byte_offset),
        reference_holder(ref) {}
  BindingRef(std::nullopt_t _) noexcept : type(Type::Null) {};
  BindingRef(BackBufferSource *t, bool srgb) noexcept
      : type(srgb ? BindingRef::Type::BackBufferSource_sRGB
                  : BindingRef::Type::BackBufferSource_Linear),
        reference_holder(t) {}

  operator bool() const noexcept { return type != Type::Null; }
  BindingRef(const BindingRef &copy) = delete;
  BindingRef(BindingRef &&move) {
    type = move.type;
    resource_ptr = move.resource_ptr;
    buffer_ptr = move.buffer_ptr;
    byte_width = move.byte_width;
    byte_offset = move.byte_offset;
    reference_holder = std::move(move.reference_holder);
    move.type = Type::Null;
    move.byte_offset = 0;
    move.byte_width = 0;
    // move.reference_holder = nullptr;
  };

  BindingRef &operator=(BindingRef &&move) {
    // ahh this is redunant
    type = move.type;
    resource_ptr = move.resource_ptr;
    buffer_ptr = move.buffer_ptr;
    byte_width = move.byte_width;
    byte_offset = move.byte_offset;
    reference_holder = std::move(move.reference_holder);
    move.type = Type::Null;
    move.byte_offset = 0;
    move.byte_width = 0;
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
    case Type::BoundedBuffer:
      return resource_ptr == other.resource_ptr &&
             byte_offset == other.byte_offset &&
             byte_width == other.byte_width;
    case Type::TextureViewOfBuffer:
    case Type::UAVWithCounter:
      return resource_ptr == other.resource_ptr &&
             byte_offset == other.byte_offset &&
             byte_width == other.byte_width && buffer_ptr == other.buffer_ptr;
    case Type::BackBufferSource_sRGB:
    case Type::BackBufferSource_Linear:
      return reference_holder == other.reference_holder;
    default:
      return false; // invalid
    }
  };

  bool requiresContext() const {
    return type == Type::BackBufferSource_sRGB ||
           type == Type::BackBufferSource_Linear;
  };

  MTL::Buffer *buffer() const {
    if ((uint64_t)type & (uint64_t)Type::JustBuffer) {
      return (MTL::Buffer *)resource_ptr;
    }
    if(type == Type::TextureViewOfBuffer) {
      return buffer_ptr;
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
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource_sRGB) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer(true);
    }
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource_Linear) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer(false);
    }
    return nullptr;
  }
  uint32_t width() const {
    if (((uint64_t)type & (uint64_t)Type::WithBoundInformation) ==
        (uint64_t)Type::WithBoundInformation) {
      return byte_width;
    }
    return 0;
  }

  uint32_t offset() const {
    if (((uint64_t)type & (uint64_t)Type::WithBoundInformation) ==
        (uint64_t)Type::WithBoundInformation) {
      return byte_offset;
    }
    return 0;
  }

  uint64_t counter_handle() const {
    if (type == Type::UAVWithCounter) {
      return counter_handle_;
    }
    return DXMT_NO_COUNTER;
  }

  MTL::Resource *resource() const { return resource_ptr; }

  MTL::Resource *resource(EncodingContext *context) const {
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource_sRGB) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer(true);
    }
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource_Linear) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer(false);
    }
    return resource();
  }
};

class ArgumentData {
  enum class Type : uint32_t {
    JustBuffer = 0b1,
    BoundedBuffer = 0b11,
    UAVWithCounter = 0b111,
    WithBoundInformation = 0b10,
    JustTexture = 0b10000000,
    TextureViewOfBuffer = 0b10000010,
    BackBufferSource_sRGB = 0b100000000,
    BackBufferSource_Linear = 0b1000000000,
  };
  Type type;
  uint32_t size;
  uint64_t resource_handle;
  union {
    uint64_t buffer_handle;
    BackBufferSource *ptr;
  };

public:
  ArgumentData(uint64_t h, uint32_t c) noexcept
      : type(Type::BoundedBuffer), size(c), resource_handle(h) {}
  ArgumentData(uint64_t h, uint32_t c, uint64_t ctr) noexcept
      : type(Type::UAVWithCounter), size(c), resource_handle(h),
        buffer_handle(ctr) {}
  ArgumentData(MTL::ResourceID id, MTL::Texture *) noexcept
      : type(Type::JustTexture), resource_handle(id._impl) {}
  ArgumentData(MTL::ResourceID id, MTL::Texture *, uint64_t buffer_handle,
               uint32_t size) noexcept
      : type(Type::TextureViewOfBuffer), size(size), resource_handle(id._impl),
        buffer_handle(buffer_handle) {}
  ArgumentData(BackBufferSource *t, bool srgb) noexcept
      : type(srgb ? Type::BackBufferSource_sRGB
                  : Type::BackBufferSource_Linear),
        ptr(t) {}

  bool requiresContext() const {
    return type == Type::BackBufferSource_sRGB ||
           type == Type::BackBufferSource_Linear;
  };

  uint64_t buffer() const {
    if ((uint64_t)type & (uint64_t)Type::JustBuffer) {
      return resource_handle;
    }
    if (type == Type::TextureViewOfBuffer) {
      return buffer_handle;
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
    if (((uint64_t)type & (uint64_t)Type::WithBoundInformation) ==
        (uint64_t)Type::WithBoundInformation) {
      return size + (((uint64_t)size) << 32);
    }
    return 0;
  }

  uint64_t counter_handle() const {
    if (type == Type::UAVWithCounter) {
      return buffer_handle;
    }
    return DXMT_NO_COUNTER;
  }

  uint64_t resource() const { return resource_handle; }

  uint64_t resource(EncodingContext *context) const {
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource_sRGB) {
      return ptr->GetCurrentFrameBackBuffer(true)->gpuResourceID()._impl;
    }
    if ((uint64_t)type & (uint64_t)Type::BackBufferSource_Linear) {
      return ptr->GetCurrentFrameBackBuffer(false)->gpuResourceID()._impl;
    }
    return resource();
  }
};
} // namespace dxmt