#pragma once

#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "ftl.hpp"
#include <map>

namespace dxmt {
enum class FormatCapability : int {
  None = 0,
  Atomic = 0x1,
  Filter = 0x2,
  Write = 0x4,
  Color = 0x8,
  Blend = 0x10,
  MSAA = 0x20,
  Sparse = 0x40,
  Resolve = 0x80,
  DepthStencil = 0x100,
  TextureBufferRead = 0x200,
  TextureBufferWrite = 0x400,
  TextureBufferReadWrite = 0x800
};

const FormatCapability ALL_CAP = static_cast<FormatCapability>(
    FormatCapability::Filter | FormatCapability::Write |
    FormatCapability::Color | FormatCapability::MSAA | FormatCapability::Blend |
    FormatCapability::Sparse | FormatCapability::Resolve);
const FormatCapability TEXTURE_BUFFER_ALL_CAP = static_cast<FormatCapability>(
    FormatCapability::TextureBufferRead | FormatCapability::TextureBufferWrite |
    FormatCapability::TextureBufferReadWrite);
const FormatCapability NO_ATOMIC_RESOLVE = static_cast<FormatCapability>(
    FormatCapability::Filter | FormatCapability::Write |
    FormatCapability::Color | FormatCapability::MSAA | FormatCapability::Blend |
    FormatCapability::Sparse);
const FormatCapability APPLE_INT_FORMAT_CAP =
    FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA |
    FormatCapability::Sparse;

const FormatCapability APPLE_INT_FORMAT_CAP_32 =
    FormatCapability::Write | FormatCapability::Color |
    FormatCapability::Sparse | FormatCapability::Atomic;

class FormatCapabilityInspector {
public:
  std::map<MTL::PixelFormat, FormatCapability> textureCapabilities{};
  void Inspect(MTL::Device *device);
};

MTL::PixelFormat Forget_sRGB(MTL::PixelFormat format);

constexpr bool Is_sRGBVariant(MTL::PixelFormat format) {
  return Forget_sRGB(format) != format;
}

bool IsBlockCompressionFormat(MTL::PixelFormat format);

size_t FormatBytesPerTexel(MTL::PixelFormat format);

} // namespace dxmt