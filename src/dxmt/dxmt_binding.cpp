#include "dxmt_binding.hpp"
#include "Metal/MTLTexture.hpp"
#include "adt.hpp"

namespace dxmt {

BindingRef::BindingRef(MTL::Buffer *buffer) noexcept
    : state(impl::UnboundedBuffer{buffer}) {}
BindingRef::BindingRef(MTL::Buffer *buffer, uint32_t element_width,
                       uint32_t offset) noexcept
    : state(impl::BoundedBuffer{buffer, element_width, offset}) {}
BindingRef::BindingRef(MTL::Buffer *buffer, uint32_t element_width,
                       uint32_t offset, MTL::Buffer *counter) noexcept
    : state(impl::UAVWithCounter{buffer, counter, element_width, offset}) {}

BindingRef::BindingRef(MTL::Texture *texture) noexcept
    : state(impl::JustTexture{texture}) {}

// BindingRef::BindingRef(std::function<MTL::Texture *()> &&thunk) noexcept
//     : state(impl::LazyTexture{std::move(thunk)}) {}
BindingRef::BindingRef(BackBufferSource *t) noexcept : state(t) {}
BindingRef::BindingRef(tag_swapchain_backbuffer_t swapchain) noexcept
    : state(impl::SwapchainBackbuffer{}) {}

BindingRef::operator bool() const noexcept {
  return std::visit(
      patterns{[](std::nullopt_t) { return false; }, [](auto) { return true; }},
      state);
}

bool BindingRef::requiresContext() const {
  return std::visit(patterns{[

  ](impl::SwapchainBackbuffer) { return true; },
                             //  [](impl::LazyTexture) { return true; },
                             [](BackBufferSource *s) { return true; },
                             [](auto) { return false; }},
                    state);
};

MTL::Buffer *BindingRef::buffer() const {
  return std::visit(
      patterns{[](impl::UAVWithCounter _) { return _.buffer.ptr(); },
               [](impl::BoundedBuffer _) { return _.buffer.ptr(); },
               [](impl::UnboundedBuffer _) { return _.buffer.ptr(); },
               [](auto) {
                 assert(0 && "");
                 return (MTL::Buffer *)nullptr;
               }},
      state);
}
MTL::Texture *BindingRef::texture() const {
  return std::visit(
      patterns{[](impl::JustTexture _) { return _.texture.ptr(); },
               [](auto) {
                 assert(0 && "");
                 return (MTL::Texture *)nullptr;
               }},
      state);
}
MTL::Texture *BindingRef::texture(EncodingContext *context) const {
  return std::visit(
      patterns{[](impl::JustTexture _) { return _.texture.ptr(); },
              //  [=](impl::SwapchainBackbuffer) {
              //    return context->GetCurrentSwapchainBackbuffer();
              //  },
               //  [](impl::LazyTexture t) { return t.thunk(); },
               [](BackBufferSource *s) {
                 return (MTL::Texture *)s->GetCurrentFrameBackBuffer();
               },
               [](auto) {
                 assert(0 && "");
                 return (MTL::Texture *)nullptr;
               }},
      state);
}
uint32_t BindingRef::width() const {
  return std::visit(
      patterns{[](impl::UAVWithCounter _) { return _.element_width; },
               [](impl::BoundedBuffer _) { return _.element_width; },
               [](auto) {
                 assert(0 && "");
                 return 0u;
               }},
      state);
}

uint32_t BindingRef::offset() const {
  return std::visit(
      patterns{[](impl::UAVWithCounter _) { return _.byte_offset; },
               [](impl::BoundedBuffer _) { return _.byte_offset; },
               [](auto) { return 0u; }},
      state);
}

MTL::Buffer *BindingRef::counter() const {
  return std::visit(
      patterns{[](impl::UAVWithCounter _) { return _.counter.ptr(); },
               [](auto) {
                 assert(0 && "");
                 return (MTL::Buffer *)nullptr;
               }},
      state);
}

MTL::Resource *BindingRef::resource() const {
  return std::visit(
      patterns{
          [](impl::UAVWithCounter _) {
            return (MTL::Resource *)_.buffer.ptr();
          },
          [](impl::BoundedBuffer _) { return (MTL::Resource *)_.buffer.ptr(); },
          [](impl::UnboundedBuffer _) {
            return (MTL::Resource *)_.buffer.ptr();
          },
          [](impl::JustTexture _) { return (MTL::Resource *)_.texture.ptr(); },
          [](auto) {
            assert(0 && "");
            return (MTL::Resource *)nullptr;
          }},
      state);
}

MTL::Resource *BindingRef::resource(EncodingContext *context) const {
  return std::visit(
      patterns{
          [](impl::UAVWithCounter _) {
            return (MTL::Resource *)_.buffer.ptr();
          },
          [](impl::BoundedBuffer _) { return (MTL::Resource *)_.buffer.ptr(); },
          [](impl::UnboundedBuffer _) {
            return (MTL::Resource *)_.buffer.ptr();
          },
          [](impl::JustTexture _) { return (MTL::Resource *)_.texture.ptr(); },
          // [=](impl::SwapchainBackbuffer) {
          //   return (MTL::Resource *)context->GetCurrentSwapchainBackbuffer();
          // },
          // [](impl::LazyTexture t) { return (MTL::Resource *)t.thunk(); },
          [](BackBufferSource *s) {
            return (MTL::Resource *)s->GetCurrentFrameBackBuffer();
          },
          [](auto) {
            assert(0 && "");
            return (MTL::Resource *)nullptr;
          }},
      state);
}

} // namespace dxmt