#include "dxmt_format.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "util_error.hpp"
#include <cassert>

#define APPEND_CAP(format, caps)                                               \
  textureCapabilities[format] = textureCapabilities[format] | caps;

namespace dxmt {
void FormatCapabilityInspector::Inspect(MTL::Device *device) {
  if (device->supportsFamily(MTL::GPUFamilyApple7)) {
    // Apple 7: M1
    APPEND_CAP(MTL::PixelFormatA8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Unorm_sRGB, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Snorm,
               ALL_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatR8Uint,
               APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Sint,
               APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)

    APPEND_CAP(MTL::PixelFormatR16Unorm,
               NO_ATOMIC_RESOLVE | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatR16Snorm,
               NO_ATOMIC_RESOLVE | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatR16Uint,
               APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR16Sint,
               APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR16Float, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRG8Unorm,
               ALL_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG8Unorm_sRGB, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRG8Snorm,
               ALL_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG8Uint,
               APPLE_INT_FORMAT_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG8Sint,
               APPLE_INT_FORMAT_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)

    APPEND_CAP(MTL::PixelFormatB5G6R5Unorm, NO_ATOMIC_RESOLVE)
    APPEND_CAP(MTL::PixelFormatA1BGR5Unorm, NO_ATOMIC_RESOLVE)
    APPEND_CAP(MTL::PixelFormatABGR4Unorm, NO_ATOMIC_RESOLVE)
    APPEND_CAP(MTL::PixelFormatBGR5A1Unorm, NO_ATOMIC_RESOLVE)

    APPEND_CAP(MTL::PixelFormatR32Uint,
               APPLE_INT_FORMAT_CAP_32 | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR32Sint,
               APPLE_INT_FORMAT_CAP_32 | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR32Float,
               FormatCapability::Write | FormatCapability::Color |
                   FormatCapability::MSAA | FormatCapability::Blend |
                   FormatCapability::Sparse | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRG16Unorm,
               NO_ATOMIC_RESOLVE | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG16Snorm,
               NO_ATOMIC_RESOLVE | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG16Uint,
               APPLE_INT_FORMAT_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG16Sint,
               APPLE_INT_FORMAT_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG16Float,
               ALL_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRGBA8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA8Unorm_sRGB, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA8Snorm,
               ALL_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRGBA8Uint,
               APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA8Sint,
               APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatBGRA8Unorm,
               ALL_CAP | FormatCapability::TextureBufferRead)
    APPEND_CAP(MTL::PixelFormatBGRA8Unorm_sRGB, ALL_CAP)

    // 32-bit packed
    APPEND_CAP(MTL::PixelFormatRGB10A2Unorm,
               ALL_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatBGR10A2Unorm, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGB10A2Uint,
               APPLE_INT_FORMAT_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG11B10Float,
               ALL_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRGB9E5Float, ALL_CAP)

    // 64-bit
    APPEND_CAP(MTL::PixelFormatRG32Uint,
               APPLE_INT_FORMAT_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG32Sint,
               APPLE_INT_FORMAT_CAP | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRG32Float,
               FormatCapability::Write | FormatCapability::Color |
                   FormatCapability::MSAA | FormatCapability::Blend |
                   FormatCapability::Sparse |
                   FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRGBA16Unorm,
               NO_ATOMIC_RESOLVE | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRGBA16Snorm,
               NO_ATOMIC_RESOLVE | FormatCapability::TextureBufferRead |
                   FormatCapability::TextureBufferWrite)
    APPEND_CAP(MTL::PixelFormatRGBA16Uint,
               APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA16Sint,
               APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA16Float, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)

    // 128-bit
    APPEND_CAP(MTL::PixelFormatRGBA32Uint,
               FormatCapability::Write | FormatCapability::Color |
                   FormatCapability::Sparse | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA32Sint,
               FormatCapability::Write | FormatCapability::Color |
                   FormatCapability::Sparse | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA32Float,
               FormatCapability::Write | FormatCapability::Color |
                   FormatCapability::MSAA | FormatCapability::Sparse |
                   TEXTURE_BUFFER_ALL_CAP)

    // compressed : TODO

    // YUV
    APPEND_CAP(MTL::PixelFormatGBGR422, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBGRG422, FormatCapability::Filter)

    // depth & stencil
    APPEND_CAP(MTL::PixelFormatDepth16Unorm,
               FormatCapability::Filter | FormatCapability::MSAA |
                   FormatCapability::Resolve | FormatCapability::DepthStencil)
    APPEND_CAP(MTL::PixelFormatDepth32Float, FormatCapability::MSAA |
                                                 FormatCapability::Resolve |
                                                 FormatCapability::DepthStencil)
    APPEND_CAP(MTL::PixelFormatStencil8,
               FormatCapability::MSAA | FormatCapability::DepthStencil)
    // APPEND_CAP(MTL::PixelFormatDepth24Unorm_Stencil8, 0); // not available
    APPEND_CAP(MTL::PixelFormatDepth32Float_Stencil8,
               FormatCapability::MSAA | FormatCapability::Resolve |
                   FormatCapability::DepthStencil)
    // APPEND_CAP(MTL::PixelFormatX24_Stencil8, 0) // not available
    APPEND_CAP(MTL::PixelFormatX32_Stencil8,
               FormatCapability::MSAA | FormatCapability::DepthStencil)

    // extended range
    APPEND_CAP(MTL::PixelFormatBGRA10_XR, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatBGRA10_XR_sRGB, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatBGR10_XR, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatBGR10_XR_sRGB, ALL_CAP)

    if (!device->supportsFamily(MTL::GPUFamilyApple8)) {
      return;
    }
    // Apple 8: M2

    APPEND_CAP(MTL::PixelFormatRG32Uint, FormatCapability::Atomic)
    APPEND_CAP(MTL::PixelFormatRG32Sint, FormatCapability::Atomic)

    APPEND_CAP(MTL::PixelFormatDepth16Unorm, FormatCapability::Sparse)
    APPEND_CAP(MTL::PixelFormatStencil8, FormatCapability::Sparse)

    if (!device->supportsFamily(MTL::GPUFamilyApple9)) {
      return;
    }
    // Apple 9: M3?

    APPEND_CAP(MTL::PixelFormatR32Float, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRG32Float, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA32Float, ALL_CAP)
  } else {
    if (device->supportsFamily(MTL::GPUFamilyMac2)) {
      assert(0 && "Non-Apple GPU is not supported yet.");
      return;
    }
    throw MTLD3DError("Invalid MTLDevice");
  }
};
} // namespace dxmt