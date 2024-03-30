#pragma once

#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include <map>

namespace dxmt {
enum class FormatCapability {
  Atomic = 0x1,
  Filter = 0x2,
  Write = 0x4,
  Color = 0x8,
  Blend = 0x10,
  MSAA = 0x20,
  Sparse = 0x40,
  Resolve = 0x80,
};

inline FormatCapability operator|(FormatCapability a, FormatCapability b) {
  return static_cast<FormatCapability>(static_cast<int>(a) |
                                       static_cast<int>(b));
}

const FormatCapability ALL_CAP = static_cast<FormatCapability>(0xFE);
const FormatCapability NO_ATOMIC_RESOLVE = static_cast<FormatCapability>(0x88);
const FormatCapability APPLE_INT_FORMAT_CAP =
    FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA |
    FormatCapability::Sparse;

const FormatCapability APPLE_INT_FORMAT_CAP_32 =
    FormatCapability::Write | FormatCapability::Color |
    FormatCapability::Sparse | FormatCapability::Atomic;

class FormatCapabilityInspector {
public:
  std::map<MTL::PixelFormat,FormatCapability> textureCapabilities{};
  void Inspect(MTL::Device *device);
};
} // namespace dxmt