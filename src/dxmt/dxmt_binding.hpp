#pragma once

#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "objc_pointer.hpp"
#include <variant>

namespace dxmt {

struct EncodingContext {
  virtual MTL::Texture *GetCurrentSwapchainBackbuffer() = 0;
};

struct BackBufferSource {
  virtual MTL::Texture *GetCurrentFrameBackBuffer() = 0;
};

struct tag_swapchain_backbuffer_t {};
constexpr tag_swapchain_backbuffer_t tag_swapchain_backbuffer{};

namespace impl {
struct UnboundedBuffer {
  Obj<MTL::Buffer> buffer;
};

struct BoundedBuffer {
  Obj<MTL::Buffer> buffer;
  uint32_t element_width;
  uint32_t byte_offset;
};
struct UAVWithCounter {
  Obj<MTL::Buffer> buffer;
  Obj<MTL::Buffer> counter;
  uint32_t element_width;
  uint32_t byte_offset;
};

struct JustTexture {
  Obj<MTL::Texture> texture;
};
struct SwapchainBackbuffer {};

struct LazyTexture {
  std::function<MTL::Texture *()> thunk;
};

using BindingResource =
    std::variant<UnboundedBuffer, BoundedBuffer, UAVWithCounter, JustTexture,
                 BackBufferSource *, SwapchainBackbuffer, std::nullopt_t>;
}; // namespace impl

class BindingRef {
  impl::BindingResource state;

public:
  BindingRef(MTL::Buffer *buffer) noexcept;
  BindingRef(MTL::Buffer *buffer, uint32_t element_width) noexcept;
  BindingRef(MTL::Buffer *buffer, uint32_t element_width,
             uint32_t byte_offset) noexcept;
  BindingRef(MTL::Buffer *buffer, uint32_t element_width, uint32_t byte_offset,
             MTL::Buffer *counter) noexcept;

  BindingRef(MTL::Texture *texture) noexcept;
  // BindingRef(std::function<MTL::Texture*()> &&thunk) noexcept;
  BindingRef(BackBufferSource *) noexcept;
  BindingRef(tag_swapchain_backbuffer_t swapchain) noexcept;
  BindingRef(std::nullopt_t _) noexcept : state(_) {};

  operator bool() const noexcept;

  /**
  for the sake of simplicity, disable copy constructor
  */
  BindingRef(const BindingRef &copy) = delete;
  BindingRef(BindingRef &&move) = default;

  MTL::Buffer *buffer() const;
  MTL::Texture *texture() const;
  MTL::Texture *texture(EncodingContext *context) const;
  uint32_t width() const;
  uint32_t offset() const;
  MTL::Buffer *counter() const;
  MTL::Resource *resource() const;
  MTL::Resource *resource(EncodingContext *context) const;

  bool requiresContext() const;
};
} // namespace dxmt