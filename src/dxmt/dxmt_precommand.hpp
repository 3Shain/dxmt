#pragma once
#include "objc_pointer.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"

#include <variant>

namespace dxmt {

struct MTLInitializeBufferCommand {
  Obj<MTL::Buffer> staging_buffer;
  Obj<MTL::Buffer> ondevice_buffer;

  MTLInitializeBufferCommand(Obj<MTL::Buffer> &&staging, MTL::Buffer *ondevice)
      : staging_buffer(std::move(staging)), ondevice_buffer(ondevice) {}
};

struct MTLInitializeTextureCommand {
  Obj<MTL::Texture> staging_texture;
  Obj<MTL::Texture> ondevice_texture;

  MTLInitializeTextureCommand(Obj<MTL::Texture> &&staging,
                              MTL::Texture *ondevice)
      : staging_texture(std::move(staging)), ondevice_texture(ondevice) {}
};

struct MTLZerofillBufferCommand {
  Obj<MTL::Buffer> ondevice_buffer;
  uint32_t offset;
  uint32_t length;

  MTLZerofillBufferCommand(MTL::Buffer *ondevice, uint32_t offset,
                           uint32_t length)
      : ondevice_buffer(ondevice), offset(offset), length(length) {}
};

using MTLPreCommand =
    std::variant<MTLInitializeBufferCommand, MTLInitializeTextureCommand,
                 MTLZerofillBufferCommand>;
} // namespace dxmt