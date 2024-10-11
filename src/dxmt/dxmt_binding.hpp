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

namespace impl {
enum class BindingType : uint32_t {
  Null = 0,
  JustBuffer = 0b1,
  WithCounter = 0b100,
  WithBoundInformation = 0b10,
  JustTexture = 0b1000'0000,
  WithBackedBuffer = 0b0100'0000,
  WithLODClamp = 0b0010'0000,
  WithElementOffset = 0b0001'0000,
  BackBufferSource_sRGB = 0b1'0000'0000,
  BackBufferSource_Linear = 0b10'0000'0000,
};

inline bool operator&(BindingType a, BindingType b) {
  return static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
}

inline BindingType operator|(BindingType a, BindingType b) {
  return static_cast<BindingType>(static_cast<uint32_t>(a) |
                                  static_cast<uint32_t>(b));
}
}

class BindingRef {
  using Type = impl::BindingType;
  Type type;
  union {
    float min_lod;
    uint32_t unused;
  };
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
      : type(Type::JustBuffer | Type::WithBoundInformation),
        resource_ptr(buffer), byte_width(byte_width), byte_offset(offset),
        reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Buffer *buffer, uint32_t element_width,
             uint32_t offset, uint64_t counter_handle) noexcept
      : type(Type::JustBuffer | Type::WithBoundInformation | Type::WithCounter),
        resource_ptr(buffer), counter_handle_(counter_handle),
        byte_width(element_width), byte_offset(offset), reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Texture *texture) noexcept
      : type(Type::JustTexture), resource_ptr(texture), reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Texture *texture, float min_lod) noexcept
      : type(Type::JustTexture | Type::WithLODClamp), min_lod(min_lod),
        resource_ptr(texture), reference_holder(ref) {}
  BindingRef(IUnknown *ref, MTL::Texture *texture, MTL::Buffer *buffer,
             uint32_t byte_width, uint32_t byte_offset) noexcept
      : type(Type::JustTexture | Type::WithBackedBuffer |
             Type::WithBoundInformation),
        resource_ptr(texture), buffer_ptr(buffer), byte_width(byte_width),
        byte_offset(byte_offset), reference_holder(ref) {}
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
    if (type & (Type::JustBuffer | Type::JustTexture)) {
      if (resource_ptr != other.resource_ptr)
        return false;
    }
    if (type & Type::WithBoundInformation) {
      if (byte_offset != other.byte_offset)
        return false;
      if (byte_width != other.byte_width)
        return false;
    }
    if (type & Type::WithCounter) {
      if (counter_handle_ != other.counter_handle_)
        return false;
    }
    if (type & Type::WithBackedBuffer) {
      if (buffer_ptr != other.buffer_ptr)
        return false;
    }
    if (type & Type::WithLODClamp) {
      if (min_lod != other.min_lod)
        return false;
    }
    if (type & (Type::BackBufferSource_sRGB | Type::BackBufferSource_Linear)) {
      if (reference_holder != other.reference_holder)
        return false;
    }

    return true;
  };

  bool requiresContext() const {
    return type == Type::BackBufferSource_sRGB ||
           type == Type::BackBufferSource_Linear;
  };

  MTL::Buffer *buffer() const {
    if (type & Type::JustBuffer) {
      return (MTL::Buffer *)resource_ptr;
    }
    if (type & Type::WithBackedBuffer) {
      return buffer_ptr;
    }
    return nullptr;
  }
  MTL::Texture *texture() const {
    if (type & Type::JustTexture) {
      return (MTL::Texture *)resource_ptr;
    }
    return nullptr;
  }
  MTL::Texture *texture(EncodingContext *context) const {
    if (type & Type::JustTexture) {
      return (MTL::Texture *)resource_ptr;
    }
    if (type & Type::BackBufferSource_sRGB) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer(true);
    }
    if (type & Type::BackBufferSource_Linear) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer(false);
    }
    return nullptr;
  }
  uint32_t width() const {
    if (type & Type::WithBoundInformation) {
      return byte_width;
    }
    return 0;
  }

  uint32_t offset() const {
    if (type & Type::WithBoundInformation) {
      return byte_offset;
    }
    return 0;
  }

  MTL::Resource *resource() const { return resource_ptr; }

  MTL::Resource *resource(EncodingContext *context) const {
    if (type & Type::BackBufferSource_sRGB) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer(true);
    }
    if (type & Type::BackBufferSource_Linear) {
      return ((BackBufferSource *)reference_holder.ptr())
          ->GetCurrentFrameBackBuffer(false);
    }
    return resource();
  }

  bool withBackedBuffer() const { return type & Type::WithBackedBuffer; };
};

class ArgumentData {
  using Type = impl::BindingType;
  Type type;
  union {
    float min_lod_;
    struct {
      uint32_t size : 28;
      uint32_t element_offset : 4;
    } packed_tbuffer_data;
  };
  uint64_t resource_handle;
  union {
    uint64_t buffer_handle;
    uint64_t counter_handle;
    BackBufferSource *ptr;
  };

public:
  ArgumentData(uint64_t h, uint32_t c) noexcept
      : type(Type::JustBuffer | Type::WithBoundInformation),
        packed_tbuffer_data({c, 0}), resource_handle(h) {}
  ArgumentData(uint64_t h, uint32_t c, uint64_t ctr) noexcept
      : type(Type::JustBuffer | Type::WithBoundInformation | Type::WithCounter),
        packed_tbuffer_data({c, 0}), resource_handle(h), counter_handle(ctr) {}
  ArgumentData(MTL::ResourceID id, MTL::Texture *) noexcept
      : type(Type::JustTexture), resource_handle(id._impl) {}
  ArgumentData(MTL::ResourceID id, float min_lod) noexcept
      : type(Type::JustTexture | Type::WithLODClamp), min_lod_(min_lod),
        resource_handle(id._impl) {}
  ArgumentData(MTL::ResourceID id, MTL::Texture *, uint64_t buffer_handle,
               uint32_t size) noexcept
      : type(Type::JustTexture | Type::WithBackedBuffer |
             Type::WithBoundInformation),
        packed_tbuffer_data({size, 0}), resource_handle(id._impl),
        buffer_handle(buffer_handle) {}
  ArgumentData(MTL::ResourceID id, uint64_t buffer_handle, uint32_t size,
               uint32_t element_offset) noexcept
      : type(Type::JustTexture | Type::WithBackedBuffer |
             Type::WithBoundInformation | Type::WithElementOffset),
        packed_tbuffer_data({size, element_offset}), resource_handle(id._impl),
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
    if (type & Type::JustBuffer) {
      return resource_handle;
    }
    if (type & Type::WithBackedBuffer) {
      return buffer_handle;
    }
    return 0;
  }

  uint64_t texture() const {
    if (type & Type::JustTexture) {
      return resource_handle;
    }
    return 0;
  }

  uint64_t texture(EncodingContext *context) const {
    if (type & Type::BackBufferSource_sRGB) {
      return ptr->GetCurrentFrameBackBuffer(true)->gpuResourceID()._impl;
    }
    if (type & Type::BackBufferSource_Linear) {
      return ptr->GetCurrentFrameBackBuffer(false)->gpuResourceID()._impl;
    }
    if (type & Type::JustTexture) {
      return resource_handle;
    }
    return 0;
  }

  uint32_t width() const {
    if (type & Type::WithBoundInformation) {
      return packed_tbuffer_data.size;
    }
    return 0;
  }

  uint64_t counter() const {
    if (type & Type::WithCounter) {
      return counter_handle;
    }
    return DXMT_NO_COUNTER;
  }

  float min_lod() const {
    if (type & Type::WithLODClamp) {
      return min_lod_;
    }
    return 0.0f;
  }

  uint32_t element_offset() const {
    if (type & Type::WithElementOffset) {
      return packed_tbuffer_data.element_offset;
    }
    return 0;
  }
};
} // namespace dxmt