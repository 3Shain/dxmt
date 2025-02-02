#pragma once

#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLStageInputOutputDescriptor.hpp"
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

class FormatCapabilityInspector {
public:
  std::map<MTL::PixelFormat, FormatCapability> textureCapabilities{};
  void Inspect(MTL::Device *device);
};

MTL::PixelFormat Forget_sRGB(MTL::PixelFormat format);
MTL::PixelFormat Recall_sRGB(MTL::PixelFormat format);

inline bool
Is_sRGBVariant(MTL::PixelFormat format) {
  return Forget_sRGB(format) != format;
}

bool IsBlockCompressionFormat(MTL::PixelFormat format);

size_t FormatBytesPerTexel(MTL::PixelFormat format);

uint32_t DepthStencilPlanarFlags(MTL::PixelFormat format);

enum MTL_DXGI_FORMAT_FLAG {
  MTL_DXGI_FORMAT_TYPELESS = 1,
  MTL_DXGI_FORMAT_BC = 2,
  MTL_DXGI_FORMAT_BACKBUFFER = 4,
  MTL_DXGI_FORMAT_DEPTH_PLANER = 16,
  MTL_DXGI_FORMAT_STENCIL_PLANER = 32,
  MTL_DXGI_FORMAT_EMULATED_D24 = 256,
};

struct MTL_DXGI_FORMAT_DESC {
  MTL::PixelFormat PixelFormat;
  MTL::AttributeFormat AttributeFormat;
  union {
    uint32_t BytesPerTexel;
    uint32_t BlockSize;
  };
  uint32_t Flag;
};

int32_t MTLQueryDXGIFormat(MTL::Device *device, uint32_t format, MTL_DXGI_FORMAT_DESC &description);

uint32_t MTLGetTexelSize(MTL::PixelFormat format);

MTL::PixelFormat MTLGetUnsignedIntegerFormat(MTL::PixelFormat format);

bool IsUnormRenderTargetFormat(MTL::PixelFormat format, bool srgb = false);

} // namespace dxmt