#include "ftl.hpp"
#include "dxmt_format.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "util_error.hpp"
#include "dxgi.h"
#include <cassert>
#include <cstdint>

#define APPEND_CAP(format, caps) textureCapabilities[format] = textureCapabilities[format] | caps;

namespace dxmt {

constexpr FormatCapability ALL_CAP = static_cast<FormatCapability>(
    FormatCapability::Filter | FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA |
    FormatCapability::Blend | FormatCapability::Sparse | FormatCapability::Resolve
);
constexpr FormatCapability TEXTURE_BUFFER_ALL_CAP = static_cast<FormatCapability>(
    FormatCapability::TextureBufferRead | FormatCapability::TextureBufferWrite |
    FormatCapability::TextureBufferReadWrite
);
constexpr FormatCapability TEXTURE_BUFFER_READ_OR_WRITE =
    static_cast<FormatCapability>(FormatCapability::TextureBufferRead | FormatCapability::TextureBufferWrite);
constexpr FormatCapability NO_ATOMIC_RESOLVE = static_cast<FormatCapability>(
    FormatCapability::Filter | FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA |
    FormatCapability::Blend | FormatCapability::Sparse
);
constexpr FormatCapability APPLE_INT_FORMAT_CAP =
    FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA | FormatCapability::Sparse;
constexpr FormatCapability NONAPPLE_INT_FORMAT_CAP =
    FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA | FormatCapability::Sparse;

constexpr FormatCapability APPLE_INT_FORMAT_CAP_32 =
    FormatCapability::Write | FormatCapability::Color | FormatCapability::Sparse | FormatCapability::Atomic;

void
FormatCapabilityInspector::Inspect(MTL::Device *device) {
  if (device->supportsFamily(MTL::GPUFamilyApple7)) {
    // Apple 7: M1
    APPEND_CAP(MTL::PixelFormatA8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Unorm_sRGB, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatR8Uint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Sint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)

    APPEND_CAP(MTL::PixelFormatR16Unorm, NO_ATOMIC_RESOLVE | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatR16Snorm, NO_ATOMIC_RESOLVE | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatR16Uint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR16Sint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR16Float, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRG8Unorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG8Unorm_sRGB, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRG8Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG8Uint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG8Sint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)

    APPEND_CAP(MTL::PixelFormatB5G6R5Unorm, NO_ATOMIC_RESOLVE)
    APPEND_CAP(MTL::PixelFormatA1BGR5Unorm, NO_ATOMIC_RESOLVE)
    APPEND_CAP(MTL::PixelFormatABGR4Unorm, NO_ATOMIC_RESOLVE)
    APPEND_CAP(MTL::PixelFormatBGR5A1Unorm, NO_ATOMIC_RESOLVE)

    APPEND_CAP(MTL::PixelFormatR32Uint, APPLE_INT_FORMAT_CAP_32 | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR32Sint, APPLE_INT_FORMAT_CAP_32 | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(
        MTL::PixelFormatR32Float, FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA |
                                      FormatCapability::Blend | FormatCapability::Sparse | TEXTURE_BUFFER_ALL_CAP
    )
    APPEND_CAP(MTL::PixelFormatRG16Unorm, NO_ATOMIC_RESOLVE | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG16Snorm, NO_ATOMIC_RESOLVE | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG16Uint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG16Sint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG16Float, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGBA8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA8Unorm_sRGB, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA8Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGBA8Uint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA8Sint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatBGRA8Unorm, ALL_CAP | FormatCapability::TextureBufferRead)
    APPEND_CAP(MTL::PixelFormatBGRA8Unorm_sRGB, ALL_CAP)

    // 32-bit packed
    APPEND_CAP(MTL::PixelFormatRGB10A2Unorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatBGR10A2Unorm, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGB10A2Uint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG11B10Float, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGB9E5Float, ALL_CAP)

    // 64-bit
    APPEND_CAP(MTL::PixelFormatRG32Uint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG32Sint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(
        MTL::PixelFormatRG32Float, FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA |
                                       FormatCapability::Blend | FormatCapability::Sparse | TEXTURE_BUFFER_READ_OR_WRITE
    )
    APPEND_CAP(MTL::PixelFormatRGBA16Unorm, NO_ATOMIC_RESOLVE | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGBA16Snorm, NO_ATOMIC_RESOLVE | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGBA16Uint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA16Sint, APPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA16Float, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)

    // 128-bit
    APPEND_CAP(
        MTL::PixelFormatRGBA32Uint,
        FormatCapability::Write | FormatCapability::Color | FormatCapability::Sparse | TEXTURE_BUFFER_ALL_CAP
    )
    APPEND_CAP(
        MTL::PixelFormatRGBA32Sint,
        FormatCapability::Write | FormatCapability::Color | FormatCapability::Sparse | TEXTURE_BUFFER_ALL_CAP
    )
    APPEND_CAP(
        MTL::PixelFormatRGBA32Float, FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA |
                                         FormatCapability::Sparse | TEXTURE_BUFFER_ALL_CAP
    )

    // compressed : TODO
    if (device->supportsBCTextureCompression()) {
      // BC
      APPEND_CAP(MTL::PixelFormatBC1_RGBA, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC1_RGBA_sRGB, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC2_RGBA, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC2_RGBA_sRGB, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC3_RGBA, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC3_RGBA_sRGB, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC4_RUnorm, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC4_RSnorm, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC5_RGSnorm, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC5_RGUnorm, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC6H_RGBUfloat, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC6H_RGBFloat, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC7_RGBAUnorm, FormatCapability::Filter | FormatCapability::Sparse)
      APPEND_CAP(MTL::PixelFormatBC7_RGBAUnorm_sRGB, FormatCapability::Filter | FormatCapability::Sparse)
    }

    // YUV
    APPEND_CAP(MTL::PixelFormatGBGR422, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBGRG422, FormatCapability::Filter)

    // depth & stencil
    APPEND_CAP(
        MTL::PixelFormatDepth16Unorm,
        FormatCapability::Filter | FormatCapability::MSAA | FormatCapability::Resolve | FormatCapability::DepthStencil
    )
    APPEND_CAP(
        MTL::PixelFormatDepth32Float,
        FormatCapability::MSAA | FormatCapability::Resolve | FormatCapability::DepthStencil
    )
    APPEND_CAP(MTL::PixelFormatStencil8, FormatCapability::MSAA | FormatCapability::DepthStencil)
    // APPEND_CAP(MTL::PixelFormatDepth24Unorm_Stencil8, 0); // not available
    APPEND_CAP(
        MTL::PixelFormatDepth32Float_Stencil8,
        FormatCapability::MSAA | FormatCapability::Resolve | FormatCapability::DepthStencil
    )
    // APPEND_CAP(MTL::PixelFormatX24_Stencil8, 0) // not available
    APPEND_CAP(MTL::PixelFormatX32_Stencil8, FormatCapability::MSAA | FormatCapability::DepthStencil)

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
    // Apple 9: M3+

    APPEND_CAP(MTL::PixelFormatR32Float, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRG32Float, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA32Float, ALL_CAP)
  } else if (device->supportsFamily(MTL::GPUFamilyMac2)) {

    APPEND_CAP(MTL::PixelFormatA8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatR8Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR8Sint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)

    APPEND_CAP(MTL::PixelFormatR16Unorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatR16Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatR16Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR16Sint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatR16Float, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)

    APPEND_CAP(MTL::PixelFormatRG8Unorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG8Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG8Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG8Sint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)

    APPEND_CAP(MTL::PixelFormatR32Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP | FormatCapability::Atomic)
    APPEND_CAP(MTL::PixelFormatR32Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP | FormatCapability::Atomic)
    APPEND_CAP(MTL::PixelFormatR32Float, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)

    APPEND_CAP(MTL::PixelFormatRG16Unorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG16Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG16Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG16Sint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG16Float, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)

    APPEND_CAP(MTL::PixelFormatRGBA8Unorm, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA8Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(
        MTL::PixelFormatRGBA8Unorm_sRGB, FormatCapability::Filter | FormatCapability::Color | FormatCapability::MSAA |
                                             FormatCapability::Resolve | FormatCapability::Blend
    )
    APPEND_CAP(MTL::PixelFormatRGBA8Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA8Sint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatBGRA8Unorm, ALL_CAP | FormatCapability::TextureBufferRead)
    APPEND_CAP(
        MTL::PixelFormatBGRA8Unorm_sRGB, FormatCapability::Filter | FormatCapability::Color | FormatCapability::MSAA |
                                             FormatCapability::Resolve | FormatCapability::Blend
    )

    // 32-bit packed
    APPEND_CAP(MTL::PixelFormatRGB10A2Unorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatBGR10A2Unorm, ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGB10A2Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG11B10Float, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGB9E5Float, FormatCapability::Filter)

    // 64-bit
    APPEND_CAP(MTL::PixelFormatRG32Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG32Sint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRG32Float, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGBA16Unorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGBA16Snorm, ALL_CAP | TEXTURE_BUFFER_READ_OR_WRITE)
    APPEND_CAP(MTL::PixelFormatRGBA16Uint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA16Sint, NONAPPLE_INT_FORMAT_CAP | TEXTURE_BUFFER_ALL_CAP)
    APPEND_CAP(MTL::PixelFormatRGBA16Float, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)

    // 128-bit
    APPEND_CAP(
        MTL::PixelFormatRGBA32Uint,
        FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA | TEXTURE_BUFFER_ALL_CAP
    )
    APPEND_CAP(
        MTL::PixelFormatRGBA32Sint,
        FormatCapability::Write | FormatCapability::Color | FormatCapability::MSAA | TEXTURE_BUFFER_ALL_CAP
    )
    APPEND_CAP(MTL::PixelFormatRGBA32Float, ALL_CAP | TEXTURE_BUFFER_ALL_CAP)

    // compressed : TODO

    // BC
    APPEND_CAP(MTL::PixelFormatBC1_RGBA, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC1_RGBA_sRGB, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC2_RGBA, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC2_RGBA_sRGB, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC3_RGBA, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC3_RGBA_sRGB, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC4_RUnorm, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC4_RSnorm, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC5_RGSnorm, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC5_RGUnorm, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC6H_RGBUfloat, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC6H_RGBFloat, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC7_RGBAUnorm, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBC7_RGBAUnorm_sRGB, FormatCapability::Filter)

    // YUV
    APPEND_CAP(MTL::PixelFormatGBGR422, FormatCapability::Filter)
    APPEND_CAP(MTL::PixelFormatBGRG422, FormatCapability::Filter)

    // depth & stencil
    APPEND_CAP(
        MTL::PixelFormatDepth16Unorm,
        FormatCapability::Filter | FormatCapability::MSAA | FormatCapability::Resolve | FormatCapability::DepthStencil
    )
    APPEND_CAP(
        MTL::PixelFormatDepth32Float,
        FormatCapability::Filter | FormatCapability::MSAA | FormatCapability::Resolve | FormatCapability::DepthStencil
    )
    APPEND_CAP(MTL::PixelFormatStencil8, FormatCapability::MSAA | FormatCapability::DepthStencil)
    APPEND_CAP(
        MTL::PixelFormatDepth24Unorm_Stencil8,
        FormatCapability::Filter | FormatCapability::MSAA | FormatCapability::Resolve | FormatCapability::DepthStencil
    );
    APPEND_CAP(
        MTL::PixelFormatDepth32Float_Stencil8,
        FormatCapability::MSAA | FormatCapability::Resolve | FormatCapability::DepthStencil
    )
    APPEND_CAP(MTL::PixelFormatX24_Stencil8, FormatCapability::MSAA | FormatCapability::DepthStencil)
    APPEND_CAP(MTL::PixelFormatX32_Stencil8, FormatCapability::MSAA | FormatCapability::DepthStencil)

    // extended range
    // APPEND_CAP(MTL::PixelFormatBGRA10_XR, ALL_CAP) // not available
    // APPEND_CAP(MTL::PixelFormatBGRA10_XR_sRGB, ALL_CAP)  // not available
    // APPEND_CAP(MTL::PixelFormatBGR10_XR, ALL_CAP)  // not available
    // APPEND_CAP(MTL::PixelFormatBGR10_XR_sRGB, ALL_CAP)  // not available

    return;
  } else {
    throw MTLD3DError("Invalid MTLDevice");
  }
};

MTL::PixelFormat
Forget_sRGB(MTL::PixelFormat format) {
  switch (format) {
  case MTL::PixelFormat::PixelFormatR8Unorm_sRGB:
    return MTL::PixelFormatR8Unorm;
  case MTL::PixelFormat::PixelFormatRG8Unorm_sRGB:
    return MTL::PixelFormatRG8Unorm;
  case MTL::PixelFormat::PixelFormatRGBA8Unorm_sRGB:
    return MTL::PixelFormatRGBA8Unorm;
  case MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB:
    return MTL::PixelFormatBGRA8Unorm;
  case MTL::PixelFormat::PixelFormatBGR10_XR_sRGB:
    return MTL::PixelFormatBGR10_XR;
  case MTL::PixelFormat::PixelFormatBGRA10_XR_sRGB:
    return MTL::PixelFormatBGRA10_XR;
  case MTL::PixelFormatBC1_RGBA_sRGB:
    return MTL::PixelFormatBC1_RGBA;
  case MTL::PixelFormat::PixelFormatBC2_RGBA_sRGB:
    return MTL::PixelFormatBC2_RGBA;
  case MTL::PixelFormat::PixelFormatBC3_RGBA_sRGB:
    return MTL::PixelFormatBC3_RGBA;
  case MTL::PixelFormat::PixelFormatBC7_RGBAUnorm_sRGB:
    return MTL::PixelFormatBC7_RGBAUnorm;
  case MTL::PixelFormat::PixelFormatPVRTC_RGBA_2BPP_sRGB:
    return MTL::PixelFormatPVRTC_RGBA_2BPP;
  case MTL::PixelFormat::PixelFormatPVRTC_RGBA_4BPP_sRGB:
    return MTL::PixelFormatPVRTC_RGBA_4BPP;
  case MTL::PixelFormat::PixelFormatPVRTC_RGB_2BPP_sRGB:
    return MTL::PixelFormatPVRTC_RGB_2BPP;
  case MTL::PixelFormat::PixelFormatPVRTC_RGB_4BPP_sRGB:
    return MTL::PixelFormatPVRTC_RGB_4BPP;
  case MTL::PixelFormat::PixelFormatEAC_RGBA8_sRGB:
    return MTL::PixelFormatEAC_RGBA8;
  case MTL::PixelFormat::PixelFormatETC2_RGB8_sRGB:
    return MTL::PixelFormatETC2_RGB8;
  case MTL::PixelFormat::PixelFormatETC2_RGB8A1_sRGB:
    return MTL::PixelFormatETC2_RGB8A1;
  default:
    return format;
  }
}

MTL::PixelFormat
Recall_sRGB(MTL::PixelFormat format) {
  switch (format) {
  case MTL::PixelFormat::PixelFormatR8Unorm:
    return MTL::PixelFormatR8Unorm_sRGB;
  case MTL::PixelFormat::PixelFormatRG8Unorm:
    return MTL::PixelFormatRG8Unorm_sRGB;
  case MTL::PixelFormat::PixelFormatRGBA8Unorm:
    return MTL::PixelFormatRGBA8Unorm_sRGB;
  case MTL::PixelFormat::PixelFormatBGRA8Unorm:
    return MTL::PixelFormatBGRA8Unorm_sRGB;
  case MTL::PixelFormat::PixelFormatBGR10_XR:
    return MTL::PixelFormatBGR10_XR_sRGB;
  case MTL::PixelFormat::PixelFormatBGRA10_XR:
    return MTL::PixelFormatBGRA10_XR_sRGB;
  case MTL::PixelFormatBC1_RGBA:
    return MTL::PixelFormatBC1_RGBA_sRGB;
  case MTL::PixelFormat::PixelFormatBC2_RGBA:
    return MTL::PixelFormatBC2_RGBA_sRGB;
  case MTL::PixelFormat::PixelFormatBC3_RGBA:
    return MTL::PixelFormatBC3_RGBA_sRGB;
  case MTL::PixelFormat::PixelFormatBC7_RGBAUnorm:
    return MTL::PixelFormatBC7_RGBAUnorm_sRGB;
  case MTL::PixelFormat::PixelFormatPVRTC_RGBA_2BPP:
    return MTL::PixelFormatPVRTC_RGBA_2BPP_sRGB;
  case MTL::PixelFormat::PixelFormatPVRTC_RGBA_4BPP:
    return MTL::PixelFormatPVRTC_RGBA_4BPP_sRGB;
  case MTL::PixelFormat::PixelFormatPVRTC_RGB_2BPP:
    return MTL::PixelFormatPVRTC_RGB_2BPP_sRGB;
  case MTL::PixelFormat::PixelFormatPVRTC_RGB_4BPP:
    return MTL::PixelFormatPVRTC_RGB_4BPP_sRGB;
  case MTL::PixelFormat::PixelFormatEAC_RGBA8:
    return MTL::PixelFormatEAC_RGBA8_sRGB;
  case MTL::PixelFormat::PixelFormatETC2_RGB8:
    return MTL::PixelFormatETC2_RGB8_sRGB;
  case MTL::PixelFormat::PixelFormatETC2_RGB8A1:
    return MTL::PixelFormatETC2_RGB8A1_sRGB;
  default:
    return format;
  }
}

bool
IsBlockCompressionFormat(MTL::PixelFormat format) {
  switch (format) {
  case MTL::PixelFormatBC1_RGBA:
  case MTL::PixelFormatBC1_RGBA_sRGB:
  case MTL::PixelFormatBC2_RGBA:
  case MTL::PixelFormatBC2_RGBA_sRGB:
  case MTL::PixelFormatBC3_RGBA:
  case MTL::PixelFormatBC3_RGBA_sRGB:
  case MTL::PixelFormatBC4_RUnorm:
  case MTL::PixelFormatBC4_RSnorm:
  case MTL::PixelFormatBC5_RGUnorm:
  case MTL::PixelFormatBC5_RGSnorm:
  case MTL::PixelFormatBC6H_RGBUfloat:
  case MTL::PixelFormatBC6H_RGBFloat:
  case MTL::PixelFormatBC7_RGBAUnorm:
  case MTL::PixelFormatBC7_RGBAUnorm_sRGB:
    return true;
  default:
    break;
  }
  return false;
}

size_t
FormatBytesPerTexel(MTL::PixelFormat format) {
  switch (format) {
  case MTL::PixelFormatInvalid:
    return 0;
  case MTL::PixelFormatA8Unorm:
  case MTL::PixelFormatR8Unorm:
  case MTL::PixelFormatR8Unorm_sRGB:
  case MTL::PixelFormatR8Snorm:
  case MTL::PixelFormatR8Uint:
  case MTL::PixelFormatR8Sint:
    return 1;
  case MTL::PixelFormatR16Unorm:
  case MTL::PixelFormatR16Snorm:
  case MTL::PixelFormatR16Uint:
  case MTL::PixelFormatR16Sint:
  case MTL::PixelFormatR16Float:
  case MTL::PixelFormatRG8Unorm:
  case MTL::PixelFormatRG8Unorm_sRGB:
  case MTL::PixelFormatRG8Snorm:
  case MTL::PixelFormatRG8Uint:
  case MTL::PixelFormatRG8Sint:
  case MTL::PixelFormatB5G6R5Unorm:
  case MTL::PixelFormatA1BGR5Unorm:
  case MTL::PixelFormatABGR4Unorm:
  case MTL::PixelFormatBGR5A1Unorm:
    return 2;
  case MTL::PixelFormatR32Uint:
  case MTL::PixelFormatR32Sint:
  case MTL::PixelFormatR32Float:
  case MTL::PixelFormatRG16Unorm:
  case MTL::PixelFormatRG16Snorm:
  case MTL::PixelFormatRG16Uint:
  case MTL::PixelFormatRG16Sint:
  case MTL::PixelFormatRG16Float:
  case MTL::PixelFormatRGBA8Unorm:
  case MTL::PixelFormatRGBA8Unorm_sRGB:
  case MTL::PixelFormatRGBA8Snorm:
  case MTL::PixelFormatRGBA8Uint:
  case MTL::PixelFormatRGBA8Sint:
  case MTL::PixelFormatBGRA8Unorm:
  case MTL::PixelFormatBGRA8Unorm_sRGB:
  case MTL::PixelFormatRGB10A2Unorm:
  case MTL::PixelFormatRGB10A2Uint:
  case MTL::PixelFormatRG11B10Float:
  case MTL::PixelFormatRGB9E5Float:
  case MTL::PixelFormatBGR10A2Unorm:
  case MTL::PixelFormatBGR10_XR:
  case MTL::PixelFormatBGR10_XR_sRGB:
    return 4;
  case MTL::PixelFormatRG32Uint:
  case MTL::PixelFormatRG32Sint:
  case MTL::PixelFormatRG32Float:
  case MTL::PixelFormatRGBA16Unorm:
  case MTL::PixelFormatRGBA16Snorm:
  case MTL::PixelFormatRGBA16Uint:
  case MTL::PixelFormatRGBA16Sint:
  case MTL::PixelFormatRGBA16Float:
    return 8;
  case MTL::PixelFormatBGRA10_XR:
  case MTL::PixelFormatBGRA10_XR_sRGB:
    return 8;
  case MTL::PixelFormatRGBA32Uint:
  case MTL::PixelFormatRGBA32Sint:
  case MTL::PixelFormatRGBA32Float:
    return 16;
  case MTL::PixelFormatBC1_RGBA:
  case MTL::PixelFormatBC1_RGBA_sRGB:
  case MTL::PixelFormatBC4_RUnorm:
  case MTL::PixelFormatBC4_RSnorm:
    return 8;
  case MTL::PixelFormatBC2_RGBA:
  case MTL::PixelFormatBC2_RGBA_sRGB:
  case MTL::PixelFormatBC3_RGBA:
  case MTL::PixelFormatBC3_RGBA_sRGB:
  case MTL::PixelFormatBC5_RGUnorm:
  case MTL::PixelFormatBC5_RGSnorm:
  case MTL::PixelFormatBC6H_RGBFloat:
  case MTL::PixelFormatBC6H_RGBUfloat:
  case MTL::PixelFormatBC7_RGBAUnorm:
  case MTL::PixelFormatBC7_RGBAUnorm_sRGB:
    return 16;
  case MTL::PixelFormatPVRTC_RGB_2BPP:
  case MTL::PixelFormatPVRTC_RGB_2BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGB_4BPP:
  case MTL::PixelFormatPVRTC_RGB_4BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGBA_2BPP:
  case MTL::PixelFormatPVRTC_RGBA_2BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGBA_4BPP:
  case MTL::PixelFormatPVRTC_RGBA_4BPP_sRGB:
  case MTL::PixelFormatEAC_R11Unorm:
  case MTL::PixelFormatEAC_R11Snorm:
  case MTL::PixelFormatEAC_RG11Unorm:
  case MTL::PixelFormatEAC_RG11Snorm:
  case MTL::PixelFormatEAC_RGBA8:
  case MTL::PixelFormatEAC_RGBA8_sRGB:
  case MTL::PixelFormatETC2_RGB8:
  case MTL::PixelFormatETC2_RGB8_sRGB:
  case MTL::PixelFormatETC2_RGB8A1:
  case MTL::PixelFormatETC2_RGB8A1_sRGB:
  case MTL::PixelFormatASTC_4x4_sRGB:
  case MTL::PixelFormatASTC_5x4_sRGB:
  case MTL::PixelFormatASTC_5x5_sRGB:
  case MTL::PixelFormatASTC_6x5_sRGB:
  case MTL::PixelFormatASTC_6x6_sRGB:
  case MTL::PixelFormatASTC_8x5_sRGB:
  case MTL::PixelFormatASTC_8x6_sRGB:
  case MTL::PixelFormatASTC_8x8_sRGB:
  case MTL::PixelFormatASTC_10x5_sRGB:
  case MTL::PixelFormatASTC_10x6_sRGB:
  case MTL::PixelFormatASTC_10x8_sRGB:
  case MTL::PixelFormatASTC_10x10_sRGB:
  case MTL::PixelFormatASTC_12x10_sRGB:
  case MTL::PixelFormatASTC_12x12_sRGB:
  case MTL::PixelFormatASTC_4x4_LDR:
  case MTL::PixelFormatASTC_5x4_LDR:
  case MTL::PixelFormatASTC_5x5_LDR:
  case MTL::PixelFormatASTC_6x5_LDR:
  case MTL::PixelFormatASTC_6x6_LDR:
  case MTL::PixelFormatASTC_8x5_LDR:
  case MTL::PixelFormatASTC_8x6_LDR:
  case MTL::PixelFormatASTC_8x8_LDR:
  case MTL::PixelFormatASTC_10x5_LDR:
  case MTL::PixelFormatASTC_10x6_LDR:
  case MTL::PixelFormatASTC_10x8_LDR:
  case MTL::PixelFormatASTC_10x10_LDR:
  case MTL::PixelFormatASTC_12x10_LDR:
  case MTL::PixelFormatASTC_12x12_LDR:
  case MTL::PixelFormatASTC_4x4_HDR:
  case MTL::PixelFormatASTC_5x4_HDR:
  case MTL::PixelFormatASTC_5x5_HDR:
  case MTL::PixelFormatASTC_6x5_HDR:
  case MTL::PixelFormatASTC_6x6_HDR:
  case MTL::PixelFormatASTC_8x5_HDR:
  case MTL::PixelFormatASTC_8x6_HDR:
  case MTL::PixelFormatASTC_8x8_HDR:
  case MTL::PixelFormatASTC_10x5_HDR:
  case MTL::PixelFormatASTC_10x6_HDR:
  case MTL::PixelFormatASTC_10x8_HDR:
  case MTL::PixelFormatASTC_10x10_HDR:
  case MTL::PixelFormatASTC_12x10_HDR:
  case MTL::PixelFormatASTC_12x12_HDR:
    return 0; // invalid
  case MTL::PixelFormatGBGR422:
  case MTL::PixelFormatBGRG422:
    return 1;
  case MTL::PixelFormatDepth16Unorm:
    return 2;
  case MTL::PixelFormatDepth32Float:
    return 4;
  case MTL::PixelFormatStencil8:
    return 1;
  case MTL::PixelFormatDepth24Unorm_Stencil8:
    return 4;
  case MTL::PixelFormatDepth32Float_Stencil8:
    return 8;
  case MTL::PixelFormatX32_Stencil8:
    return 8;
  case MTL::PixelFormatX24_Stencil8:
    return 4;
  case MTL::PixelFormatCount:
    return 0;
  }
}

uint32_t
DepthStencilPlanarFlags(MTL::PixelFormat format) {
  switch (format) {
  case MTL::PixelFormatDepth32Float_Stencil8:
  case MTL::PixelFormatDepth24Unorm_Stencil8:
    return 3;
  case MTL::PixelFormatDepth32Float:
  case MTL::PixelFormatDepth16Unorm:
    return 1;
  case MTL::PixelFormatStencil8:
    return 2;
  default:
    return 0;
  }
}

int32_t
MTLQueryDXGIFormat(MTL::Device *device, uint32_t format, MTL_DXGI_FORMAT_DESC &description) {
  description.PixelFormat = MTL::PixelFormatInvalid;
  description.AttributeFormat = MTL::AttributeFormatInvalid;
  description.BytesPerTexel = 0;
  description.Flag = 0;

  switch (format) {
  case DXGI_FORMAT_R32G32B32A32_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatRGBA32Uint;
    description.BytesPerTexel = 16;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R32G32B32A32_UINT: {
    description.PixelFormat = MTL::PixelFormatRGBA32Uint;
    description.AttributeFormat = MTL::AttributeFormatUInt4;
    description.BytesPerTexel = 16;
    break;
  }
  case DXGI_FORMAT_R32G32B32A32_SINT: {
    description.PixelFormat = MTL::PixelFormatRGBA32Sint;
    description.AttributeFormat = MTL::AttributeFormatInt4;
    description.BytesPerTexel = 16;
    break;
  }
  case DXGI_FORMAT_R32G32B32A32_FLOAT: {
    description.PixelFormat = MTL::PixelFormatRGBA32Float;
    description.AttributeFormat = MTL::AttributeFormatFloat4;
    description.BytesPerTexel = 16;
    break;
  }
  case DXGI_FORMAT_R32G32B32_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatR32Uint;
    description.BytesPerTexel = 12;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R32G32B32_UINT: {
    description.PixelFormat = MTL::PixelFormatR32Uint;
    description.AttributeFormat = MTL::AttributeFormatUInt3;
    description.BytesPerTexel = 12;
    break;
  }
  case DXGI_FORMAT_R32G32B32_SINT: {
    description.PixelFormat = MTL::PixelFormatR32Sint;
    description.AttributeFormat = MTL::AttributeFormatInt3;
    description.BytesPerTexel = 12;
    break;
  }
  case DXGI_FORMAT_R32G32B32_FLOAT: {
    description.PixelFormat = MTL::PixelFormatR32Float;
    description.AttributeFormat = MTL::AttributeFormatFloat3;
    description.BytesPerTexel = 12;
    break;
  }
  case DXGI_FORMAT_R16G16B16A16_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatRGBA16Float;
    description.BytesPerTexel = 8;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R16G16B16A16_FLOAT: {
    description.PixelFormat = MTL::PixelFormatRGBA16Float;
    description.AttributeFormat = MTL::AttributeFormatHalf4;
    description.BytesPerTexel = 8;
    description.Flag = MTL_DXGI_FORMAT_BACKBUFFER; // 11.1
    break;
  }
  case DXGI_FORMAT_R16G16B16A16_UNORM: {
    description.PixelFormat = MTL::PixelFormatRGBA16Unorm;
    description.AttributeFormat = MTL::AttributeFormatUShort4Normalized;
    description.BytesPerTexel = 8;
    break;
  }
  case DXGI_FORMAT_R16G16B16A16_UINT: {
    description.PixelFormat = MTL::PixelFormatRGBA16Uint;
    description.AttributeFormat = MTL::AttributeFormatUShort4;
    description.BytesPerTexel = 8;
    break;
  }
  case DXGI_FORMAT_R16G16B16A16_SNORM: {
    description.PixelFormat = MTL::PixelFormatRGBA16Snorm;
    description.AttributeFormat = MTL::AttributeFormatShort4Normalized;
    description.BytesPerTexel = 8;
    break;
  }
  case DXGI_FORMAT_R16G16B16A16_SINT: {
    description.PixelFormat = MTL::PixelFormatRGBA16Sint;
    description.AttributeFormat = MTL::AttributeFormatShort4;
    description.BytesPerTexel = 8;
    break;
  }
  case DXGI_FORMAT_R32G32_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatRG32Uint;
    description.BytesPerTexel = 8;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R32G32_FLOAT: {
    description.PixelFormat = MTL::PixelFormatRG32Float;
    description.AttributeFormat = MTL::AttributeFormatFloat2;
    description.BytesPerTexel = 8;
    break;
  }
  case DXGI_FORMAT_R32G32_UINT: {
    description.PixelFormat = MTL::PixelFormatRG32Uint;
    description.AttributeFormat = MTL::AttributeFormatUInt2;
    description.BytesPerTexel = 8;
    break;
  }
  case DXGI_FORMAT_R32G32_SINT: {
    description.PixelFormat = MTL::PixelFormatRG32Sint;
    description.AttributeFormat = MTL::AttributeFormatInt2;
    description.BytesPerTexel = 8;
    break;
  }
  case DXGI_FORMAT_R32G8X24_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
    description.BytesPerTexel = 8;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_DEPTH_PLANER | MTL_DXGI_FORMAT_STENCIL_PLANER;
    break;
  }
  case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: {
    description.PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
    description.BytesPerTexel = 8;
    description.Flag = MTL_DXGI_FORMAT_DEPTH_PLANER | MTL_DXGI_FORMAT_STENCIL_PLANER;
    break;
  }
  case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
    description.BytesPerTexel = 8;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_DEPTH_PLANER;
    break;
  }
  case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: {
    description.PixelFormat = MTL::PixelFormatX32_Stencil8;
    description.BytesPerTexel = 8;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_STENCIL_PLANER;
    break;
  }
  case DXGI_FORMAT_R10G10B10A2_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatRGB10A2Uint;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R10G10B10A2_UNORM: {
    description.PixelFormat = MTL::PixelFormatRGB10A2Unorm;
    description.AttributeFormat = MTL::AttributeFormatUInt1010102Normalized;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_BACKBUFFER; // 11.1
    break;
  }
  case DXGI_FORMAT_R10G10B10A2_UINT: {
    description.PixelFormat = MTL::PixelFormatRGB10A2Uint;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R11G11B10_FLOAT: {
    description.PixelFormat = MTL::PixelFormatRG11B10Float;
    description.AttributeFormat = MTL::AttributeFormatFloatRG11B10;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R8G8B8A8_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatRGBA8Unorm;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R8G8B8A8_UNORM: {
    description.PixelFormat = MTL::PixelFormatRGBA8Unorm;
    description.AttributeFormat = MTL::AttributeFormatUChar4Normalized;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_BACKBUFFER; // 11.1, swizzle from BGRA8Unorm
    // FIXME: backbuffer format not supported!
    // need some workaround for GetDisplayModeList
    break;
  }
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: {
    description.PixelFormat = MTL::PixelFormatRGBA8Unorm_sRGB;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_BACKBUFFER; // 11.1, swizzle from BGRA8Unorm
    break;
  }
  case DXGI_FORMAT_R8G8B8A8_UINT: {
    description.PixelFormat = MTL::PixelFormatRGBA8Uint;
    description.AttributeFormat = MTL::AttributeFormatUChar4;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R8G8B8A8_SNORM: {
    description.PixelFormat = MTL::PixelFormatRGBA8Snorm;
    description.AttributeFormat = MTL::AttributeFormatChar4Normalized;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R8G8B8A8_SINT: {
    description.PixelFormat = MTL::PixelFormatRGBA8Sint;
    description.AttributeFormat = MTL::AttributeFormatChar4;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R16G16_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatRG16Uint;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R16G16_FLOAT: {
    description.PixelFormat = MTL::PixelFormatRG16Float;
    description.AttributeFormat = MTL::AttributeFormatHalf2;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R16G16_UNORM: {
    description.PixelFormat = MTL::PixelFormatRG16Unorm;
    description.AttributeFormat = MTL::AttributeFormatUShort2Normalized;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R16G16_UINT: {
    description.PixelFormat = MTL::PixelFormatRG16Uint;
    description.AttributeFormat = MTL::AttributeFormatUShort2;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R16G16_SNORM: {
    description.PixelFormat = MTL::PixelFormatRG16Snorm;
    description.AttributeFormat = MTL::AttributeFormatShort2Normalized;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R16G16_SINT: {
    description.PixelFormat = MTL::PixelFormatRG16Sint;
    description.AttributeFormat = MTL::AttributeFormatShort2;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R32_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatR32Uint;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_D32_FLOAT: {
    description.PixelFormat = MTL::PixelFormatDepth32Float;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R32_FLOAT: {
    description.PixelFormat = MTL::PixelFormatR32Float;
    description.AttributeFormat = MTL::AttributeFormatFloat;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R32_UINT: {
    description.PixelFormat = MTL::PixelFormatR32Uint;
    description.AttributeFormat = MTL::AttributeFormatUInt;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R32_SINT: {
    description.PixelFormat = MTL::PixelFormatR32Sint;
    description.AttributeFormat = MTL::AttributeFormatInt;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R24G8_TYPELESS: {
    if (device->depth24Stencil8PixelFormatSupported()) {
      description.PixelFormat = MTL::PixelFormatDepth24Unorm_Stencil8;
    } else {
      description.Flag |= MTL_DXGI_FORMAT_EMULATED_D24;
      description.PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
    }
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_DEPTH_PLANER | MTL_DXGI_FORMAT_STENCIL_PLANER;
    break;
  }
  case DXGI_FORMAT_D24_UNORM_S8_UINT: {
    description.Flag = MTL_DXGI_FORMAT_DEPTH_PLANER | MTL_DXGI_FORMAT_STENCIL_PLANER;
    if (device->depth24Stencil8PixelFormatSupported()) {
      description.PixelFormat = MTL::PixelFormatDepth24Unorm_Stencil8;
    } else {
      description.Flag |= MTL_DXGI_FORMAT_EMULATED_D24;
      description.PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
    }
    description.Flag = MTL_DXGI_FORMAT_DEPTH_PLANER | MTL_DXGI_FORMAT_STENCIL_PLANER;
    break;
  }
  case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: {
    description.Flag = MTL_DXGI_FORMAT_DEPTH_PLANER;
    if (device->depth24Stencil8PixelFormatSupported()) {
      description.PixelFormat = MTL::PixelFormatDepth24Unorm_Stencil8;
    } else {
      description.Flag |= MTL_DXGI_FORMAT_EMULATED_D24;
      description.PixelFormat = MTL::PixelFormatDepth32Float_Stencil8;
    }
    description.Flag = MTL_DXGI_FORMAT_DEPTH_PLANER;
    break;
  }
  case DXGI_FORMAT_X24_TYPELESS_G8_UINT: {
    description.Flag = MTL_DXGI_FORMAT_STENCIL_PLANER;
    if (device->depth24Stencil8PixelFormatSupported()) {
      description.PixelFormat = MTL::PixelFormatX24_Stencil8;
    } else {
      description.Flag |= MTL_DXGI_FORMAT_EMULATED_D24;
      description.PixelFormat = MTL::PixelFormatX32_Stencil8;
    }
    description.Flag = MTL_DXGI_FORMAT_STENCIL_PLANER;
    break;
  }
  case DXGI_FORMAT_R8G8_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatRG8Uint;
    description.BytesPerTexel = 2;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R8G8_UNORM: {
    description.PixelFormat = MTL::PixelFormatRG8Unorm;
    description.AttributeFormat = MTL::AttributeFormatUChar2Normalized;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R8G8_UINT: {
    description.PixelFormat = MTL::PixelFormatRG8Uint;
    description.AttributeFormat = MTL::AttributeFormatUChar2;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R8G8_SNORM: {
    description.PixelFormat = MTL::PixelFormatRG8Snorm;
    description.AttributeFormat = MTL::AttributeFormatChar2Normalized;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R8G8_SINT: {
    description.PixelFormat = MTL::PixelFormatRG8Sint;
    description.AttributeFormat = MTL::AttributeFormatChar2;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R16_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatR16Uint;
    description.BytesPerTexel = 2;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R16_FLOAT: {
    description.PixelFormat = MTL::PixelFormatR16Float;
    description.AttributeFormat = MTL::AttributeFormatHalf;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_D16_UNORM: {
    description.PixelFormat = MTL::PixelFormatDepth16Unorm;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R16_UNORM: {
    description.PixelFormat = MTL::PixelFormatR16Unorm;
    description.AttributeFormat = MTL::AttributeFormatUShortNormalized;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R16_UINT: {
    description.PixelFormat = MTL::PixelFormatR16Uint;
    description.AttributeFormat = MTL::AttributeFormatUShort;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R16_SNORM: {
    description.PixelFormat = MTL::PixelFormatR16Snorm;
    description.AttributeFormat = MTL::AttributeFormatShortNormalized;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R16_SINT: {
    description.PixelFormat = MTL::PixelFormatR16Sint;
    description.AttributeFormat = MTL::AttributeFormatShort;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_R8_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatR8Uint;
    description.BytesPerTexel = 1;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_R8_UNORM: {
    description.PixelFormat = MTL::PixelFormatR8Unorm;
    description.AttributeFormat = MTL::AttributeFormatUCharNormalized;
    description.BytesPerTexel = 1;
    break;
  }
  case DXGI_FORMAT_R8_UINT: {
    description.PixelFormat = MTL::PixelFormatR8Uint;
    description.AttributeFormat = MTL::AttributeFormatUChar;
    description.BytesPerTexel = 1;
    break;
  }
  case DXGI_FORMAT_R8_SNORM: {
    description.PixelFormat = MTL::PixelFormatR8Snorm;
    description.AttributeFormat = MTL::AttributeFormatCharNormalized;
    description.BytesPerTexel = 1;
    break;
  }
  case DXGI_FORMAT_R8_SINT: {
    description.PixelFormat = MTL::PixelFormatR8Sint;
    description.AttributeFormat = MTL::AttributeFormatChar;
    description.BytesPerTexel = 1;
    break;
  }
  case DXGI_FORMAT_A8_UNORM: {
    description.PixelFormat = MTL::PixelFormatA8Unorm;
    description.BytesPerTexel = 1;
    break;
  }
  case DXGI_FORMAT_R1_UNORM: {
    return E_FAIL;
  }
  case DXGI_FORMAT_R9G9B9E5_SHAREDEXP: {
    description.PixelFormat = MTL::PixelFormatRGB9E5Float;
    description.AttributeFormat = MTL::AttributeFormatFloatRGB9E5;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_R8G8_B8G8_UNORM: {
    description.PixelFormat = MTL::PixelFormatBGRG422;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_G8R8_G8B8_UNORM: {
    description.PixelFormat = MTL::PixelFormatGBGR422;
    description.BytesPerTexel = 4;
    break;
  }
  case DXGI_FORMAT_BC1_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBC1_RGBA;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_BC;
    description.BlockSize = 8;
    break;
  }
  /**
  from doc:
  Reinterpretation of image data between pixel formats is supported:
  - sRGB and non-sRGB forms of the same compressed format
  */
  case DXGI_FORMAT_BC1_UNORM: {
    description.PixelFormat = MTL::PixelFormatBC1_RGBA;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 8;
    break;
  }
  case DXGI_FORMAT_BC1_UNORM_SRGB: {
    description.PixelFormat = MTL::PixelFormatBC1_RGBA_sRGB;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 8;
    break;
  }
  case DXGI_FORMAT_BC2_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBC2_RGBA;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC2_UNORM: {
    description.PixelFormat = MTL::PixelFormatBC2_RGBA;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC2_UNORM_SRGB: {
    description.PixelFormat = MTL::PixelFormatBC2_RGBA_sRGB;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC3_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBC3_RGBA;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC3_UNORM: {
    description.PixelFormat = MTL::PixelFormatBC3_RGBA;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC3_UNORM_SRGB: {
    description.PixelFormat = MTL::PixelFormatBC3_RGBA_sRGB;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC4_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBC4_RUnorm;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_BC;
    description.BlockSize = 8;
    break;
  }
  case DXGI_FORMAT_BC4_UNORM: {
    description.PixelFormat = MTL::PixelFormatBC4_RUnorm;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 8;
    break;
  }
  case DXGI_FORMAT_BC4_SNORM: {
    description.PixelFormat = MTL::PixelFormatBC4_RSnorm;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 8;
    break;
  }
  case DXGI_FORMAT_BC5_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBC5_RGUnorm;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC5_UNORM: {
    description.PixelFormat = MTL::PixelFormatBC5_RGUnorm;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC5_SNORM: {
    description.PixelFormat = MTL::PixelFormatBC5_RGSnorm;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_B5G6R5_UNORM: {
    description.PixelFormat = MTL::PixelFormatB5G6R5Unorm;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_B5G5R5A1_UNORM: {
    description.PixelFormat = MTL::PixelFormatBGR5A1Unorm;
    description.BytesPerTexel = 2;
    break;
  }
  case DXGI_FORMAT_B8G8R8A8_UNORM: {
    description.PixelFormat = MTL::PixelFormatBGRA8Unorm;
    description.AttributeFormat = MTL::AttributeFormatUChar4Normalized_BGRA;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_BACKBUFFER; // 11.1
    break;
  }
  case DXGI_FORMAT_B8G8R8X8_UNORM: {
    description.PixelFormat = MTL::PixelFormatBGRA8Unorm;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_BACKBUFFER;
    break;
  }
  case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    return E_FAIL;
  case DXGI_FORMAT_B8G8R8A8_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBGRA8Unorm;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: {
    description.PixelFormat = MTL::PixelFormatBGRA8Unorm_sRGB;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_BACKBUFFER; // 11.1
    break;
  }
  case DXGI_FORMAT_B8G8R8X8_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBGRA8Unorm;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS;
    break;
  }
  case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: {
    description.PixelFormat = MTL::PixelFormatBGRA8Unorm_sRGB;
    description.BytesPerTexel = 4;
    description.Flag = MTL_DXGI_FORMAT_BACKBUFFER;
    break;
  }
  case DXGI_FORMAT_BC6H_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBC6H_RGBUfloat;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC6H_UF16: {
    description.PixelFormat = MTL::PixelFormatBC6H_RGBUfloat;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC6H_SF16: {
    description.PixelFormat = MTL::PixelFormatBC6H_RGBFloat;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC7_TYPELESS: {
    description.PixelFormat = MTL::PixelFormatBC7_RGBAUnorm;
    description.Flag = MTL_DXGI_FORMAT_TYPELESS | MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC7_UNORM: {
    description.PixelFormat = MTL::PixelFormatBC7_RGBAUnorm;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_BC7_UNORM_SRGB: {
    description.PixelFormat = MTL::PixelFormatBC7_RGBAUnorm_sRGB;
    description.Flag = MTL_DXGI_FORMAT_BC;
    description.BlockSize = 16;
    break;
  }
  case DXGI_FORMAT_AYUV:
  case DXGI_FORMAT_Y410:
  case DXGI_FORMAT_Y416:
  case DXGI_FORMAT_NV12:
  case DXGI_FORMAT_P010:
  case DXGI_FORMAT_P016:
  case DXGI_FORMAT_420_OPAQUE:
  case DXGI_FORMAT_YUY2:
  case DXGI_FORMAT_Y210:
  case DXGI_FORMAT_Y216:
  case DXGI_FORMAT_NV11:
  case DXGI_FORMAT_AI44:
  case DXGI_FORMAT_IA44:
  case DXGI_FORMAT_P8:
  case DXGI_FORMAT_A8P8:
  case DXGI_FORMAT_B4G4R4A4_UNORM:
  case DXGI_FORMAT_P208:
  case DXGI_FORMAT_V208:
  case DXGI_FORMAT_V408:
  case DXGI_FORMAT_FORCE_UINT:
  case DXGI_FORMAT_UNKNOWN:
  default:
    return E_FAIL;
  }

  return S_OK;
}

uint32_t
MTLGetTexelSize(MTL::PixelFormat format) {
  switch (format) {
  case MTL::PixelFormatInvalid:
    return 0;
  case MTL::PixelFormatA8Unorm:
  case MTL::PixelFormatR8Unorm:
  case MTL::PixelFormatR8Unorm_sRGB:
  case MTL::PixelFormatR8Snorm:
  case MTL::PixelFormatR8Uint:
  case MTL::PixelFormatR8Sint:
    return 1;
  case MTL::PixelFormatR16Unorm:
  case MTL::PixelFormatR16Snorm:
  case MTL::PixelFormatR16Uint:
  case MTL::PixelFormatR16Sint:
  case MTL::PixelFormatR16Float:
  case MTL::PixelFormatRG8Unorm:
  case MTL::PixelFormatRG8Unorm_sRGB:
  case MTL::PixelFormatRG8Snorm:
  case MTL::PixelFormatRG8Uint:
  case MTL::PixelFormatRG8Sint:
  case MTL::PixelFormatB5G6R5Unorm:
  case MTL::PixelFormatA1BGR5Unorm:
  case MTL::PixelFormatABGR4Unorm:
  case MTL::PixelFormatBGR5A1Unorm:
    return 2;
  case MTL::PixelFormatR32Uint:
  case MTL::PixelFormatR32Sint:
  case MTL::PixelFormatR32Float:
  case MTL::PixelFormatRG16Unorm:
  case MTL::PixelFormatRG16Snorm:
  case MTL::PixelFormatRG16Uint:
  case MTL::PixelFormatRG16Sint:
  case MTL::PixelFormatRG16Float:
  case MTL::PixelFormatRGBA8Unorm:
  case MTL::PixelFormatRGBA8Unorm_sRGB:
  case MTL::PixelFormatRGBA8Snorm:
  case MTL::PixelFormatRGBA8Uint:
  case MTL::PixelFormatRGBA8Sint:
  case MTL::PixelFormatBGRA8Unorm:
  case MTL::PixelFormatBGRA8Unorm_sRGB:
  case MTL::PixelFormatRGB10A2Unorm:
  case MTL::PixelFormatRGB10A2Uint:
  case MTL::PixelFormatRG11B10Float:
  case MTL::PixelFormatRGB9E5Float:
  case MTL::PixelFormatBGR10A2Unorm:
  case MTL::PixelFormatBGR10_XR:
  case MTL::PixelFormatBGR10_XR_sRGB:
    return 4;
  case MTL::PixelFormatRG32Uint:
  case MTL::PixelFormatRG32Sint:
  case MTL::PixelFormatRG32Float:
  case MTL::PixelFormatRGBA16Unorm:
  case MTL::PixelFormatRGBA16Snorm:
  case MTL::PixelFormatRGBA16Uint:
  case MTL::PixelFormatRGBA16Sint:
  case MTL::PixelFormatRGBA16Float:
  case MTL::PixelFormatBGRA10_XR:
  case MTL::PixelFormatBGRA10_XR_sRGB:
    return 8;
  case MTL::PixelFormatRGBA32Uint:
  case MTL::PixelFormatRGBA32Sint:
  case MTL::PixelFormatRGBA32Float:
  case MTL::PixelFormatBC1_RGBA:
  case MTL::PixelFormatBC1_RGBA_sRGB:
  case MTL::PixelFormatBC2_RGBA:
  case MTL::PixelFormatBC2_RGBA_sRGB:
  case MTL::PixelFormatBC3_RGBA:
  case MTL::PixelFormatBC3_RGBA_sRGB:
  case MTL::PixelFormatBC4_RUnorm:
  case MTL::PixelFormatBC4_RSnorm:
  case MTL::PixelFormatBC5_RGUnorm:
  case MTL::PixelFormatBC5_RGSnorm:
  case MTL::PixelFormatBC6H_RGBFloat:
  case MTL::PixelFormatBC6H_RGBUfloat:
  case MTL::PixelFormatBC7_RGBAUnorm:
  case MTL::PixelFormatBC7_RGBAUnorm_sRGB:
    return 16;
  case MTL::PixelFormatPVRTC_RGB_2BPP:
  case MTL::PixelFormatPVRTC_RGB_2BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGB_4BPP:
  case MTL::PixelFormatPVRTC_RGB_4BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGBA_2BPP:
  case MTL::PixelFormatPVRTC_RGBA_2BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGBA_4BPP:
  case MTL::PixelFormatPVRTC_RGBA_4BPP_sRGB:
  case MTL::PixelFormatEAC_R11Unorm:
  case MTL::PixelFormatEAC_R11Snorm:
  case MTL::PixelFormatEAC_RG11Unorm:
  case MTL::PixelFormatEAC_RG11Snorm:
  case MTL::PixelFormatEAC_RGBA8:
  case MTL::PixelFormatEAC_RGBA8_sRGB:
  case MTL::PixelFormatETC2_RGB8:
  case MTL::PixelFormatETC2_RGB8_sRGB:
  case MTL::PixelFormatETC2_RGB8A1:
  case MTL::PixelFormatETC2_RGB8A1_sRGB:
  case MTL::PixelFormatASTC_4x4_sRGB:
  case MTL::PixelFormatASTC_5x4_sRGB:
  case MTL::PixelFormatASTC_5x5_sRGB:
  case MTL::PixelFormatASTC_6x5_sRGB:
  case MTL::PixelFormatASTC_6x6_sRGB:
  case MTL::PixelFormatASTC_8x5_sRGB:
  case MTL::PixelFormatASTC_8x6_sRGB:
  case MTL::PixelFormatASTC_8x8_sRGB:
  case MTL::PixelFormatASTC_10x5_sRGB:
  case MTL::PixelFormatASTC_10x6_sRGB:
  case MTL::PixelFormatASTC_10x8_sRGB:
  case MTL::PixelFormatASTC_10x10_sRGB:
  case MTL::PixelFormatASTC_12x10_sRGB:
  case MTL::PixelFormatASTC_12x12_sRGB:
  case MTL::PixelFormatASTC_4x4_LDR:
  case MTL::PixelFormatASTC_5x4_LDR:
  case MTL::PixelFormatASTC_5x5_LDR:
  case MTL::PixelFormatASTC_6x5_LDR:
  case MTL::PixelFormatASTC_6x6_LDR:
  case MTL::PixelFormatASTC_8x5_LDR:
  case MTL::PixelFormatASTC_8x6_LDR:
  case MTL::PixelFormatASTC_8x8_LDR:
  case MTL::PixelFormatASTC_10x5_LDR:
  case MTL::PixelFormatASTC_10x6_LDR:
  case MTL::PixelFormatASTC_10x8_LDR:
  case MTL::PixelFormatASTC_10x10_LDR:
  case MTL::PixelFormatASTC_12x10_LDR:
  case MTL::PixelFormatASTC_12x12_LDR:
  case MTL::PixelFormatASTC_4x4_HDR:
  case MTL::PixelFormatASTC_5x4_HDR:
  case MTL::PixelFormatASTC_5x5_HDR:
  case MTL::PixelFormatASTC_6x5_HDR:
  case MTL::PixelFormatASTC_6x6_HDR:
  case MTL::PixelFormatASTC_8x5_HDR:
  case MTL::PixelFormatASTC_8x6_HDR:
  case MTL::PixelFormatASTC_8x8_HDR:
  case MTL::PixelFormatASTC_10x5_HDR:
  case MTL::PixelFormatASTC_10x6_HDR:
  case MTL::PixelFormatASTC_10x8_HDR:
  case MTL::PixelFormatASTC_10x10_HDR:
  case MTL::PixelFormatASTC_12x10_HDR:
  case MTL::PixelFormatASTC_12x12_HDR:
    assert(0 && "");
    return 0;
  case MTL::PixelFormatGBGR422:
  case MTL::PixelFormatBGRG422:
    return 1;
  case MTL::PixelFormatDepth16Unorm:
    return 2;
  case MTL::PixelFormatDepth32Float:
    return 4;
  case MTL::PixelFormatStencil8:
  case MTL::PixelFormatDepth24Unorm_Stencil8:
  case MTL::PixelFormatX24_Stencil8:
    return 4;
  case MTL::PixelFormatDepth32Float_Stencil8:
  case MTL::PixelFormatX32_Stencil8:
    return 8;
  case MTL::PixelFormatCount:
    break;
  }
  return 0;
};

MTL::PixelFormat
MTLGetUnsignedIntegerFormat(MTL::PixelFormat format) {
  switch (format) {
  case MTL::PixelFormatA8Unorm:
  case MTL::PixelFormatR8Unorm:
  case MTL::PixelFormatR8Unorm_sRGB:
  case MTL::PixelFormatR8Snorm:
  case MTL::PixelFormatR8Uint:
  case MTL::PixelFormatR8Sint:
    return MTL::PixelFormatR8Uint;
  case MTL::PixelFormatR16Unorm:
  case MTL::PixelFormatR16Snorm:
  case MTL::PixelFormatR16Uint:
  case MTL::PixelFormatR16Sint:
  case MTL::PixelFormatR16Float:
    return MTL::PixelFormatR16Uint;
  case MTL::PixelFormatRG8Unorm:
  case MTL::PixelFormatRG8Unorm_sRGB:
  case MTL::PixelFormatRG8Snorm:
  case MTL::PixelFormatRG8Uint:
  case MTL::PixelFormatRG8Sint:
    return MTL::PixelFormatRG8Uint;
  case MTL::PixelFormatB5G6R5Unorm:
  case MTL::PixelFormatA1BGR5Unorm:
  case MTL::PixelFormatABGR4Unorm:
  case MTL::PixelFormatBGR5A1Unorm:
    break;
  case MTL::PixelFormatR32Uint:
  case MTL::PixelFormatR32Sint:
  case MTL::PixelFormatR32Float:
    return MTL::PixelFormatR32Uint;
  case MTL::PixelFormatRG16Unorm:
  case MTL::PixelFormatRG16Snorm:
  case MTL::PixelFormatRG16Uint:
  case MTL::PixelFormatRG16Sint:
  case MTL::PixelFormatRG16Float:
    return MTL::PixelFormatRG16Uint;
  case MTL::PixelFormatRGBA8Unorm:
  case MTL::PixelFormatRGBA8Unorm_sRGB:
  case MTL::PixelFormatRGBA8Snorm:
  case MTL::PixelFormatRGBA8Uint:
  case MTL::PixelFormatRGBA8Sint:
  case MTL::PixelFormatBGRA8Unorm:
  case MTL::PixelFormatBGRA8Unorm_sRGB:
    return MTL::PixelFormatRGBA8Uint;
  case MTL::PixelFormatRGB10A2Unorm:
  case MTL::PixelFormatRGB10A2Uint:
    return MTL::PixelFormatRGB10A2Uint;
  case MTL::PixelFormatRG11B10Float:
  case MTL::PixelFormatRGB9E5Float:
  case MTL::PixelFormatBGR10A2Unorm:
  case MTL::PixelFormatBGR10_XR:
  case MTL::PixelFormatBGR10_XR_sRGB:
    break;
  case MTL::PixelFormatRG32Uint:
  case MTL::PixelFormatRG32Sint:
  case MTL::PixelFormatRG32Float:
    return MTL::PixelFormatRG32Uint;
  case MTL::PixelFormatRGBA16Unorm:
  case MTL::PixelFormatRGBA16Snorm:
  case MTL::PixelFormatRGBA16Uint:
  case MTL::PixelFormatRGBA16Sint:
  case MTL::PixelFormatRGBA16Float:
    return MTL::PixelFormatRGBA16Uint;
  case MTL::PixelFormatBGRA10_XR:
  case MTL::PixelFormatBGRA10_XR_sRGB:
    break;
  case MTL::PixelFormatRGBA32Uint:
  case MTL::PixelFormatRGBA32Sint:
  case MTL::PixelFormatRGBA32Float:
    return MTL::PixelFormatRGBA32Uint;
  case MTL::PixelFormatBC1_RGBA:
  case MTL::PixelFormatBC1_RGBA_sRGB:
  case MTL::PixelFormatBC2_RGBA:
  case MTL::PixelFormatBC2_RGBA_sRGB:
  case MTL::PixelFormatBC3_RGBA:
  case MTL::PixelFormatBC3_RGBA_sRGB:
  case MTL::PixelFormatBC4_RUnorm:
  case MTL::PixelFormatBC4_RSnorm:
  case MTL::PixelFormatBC5_RGUnorm:
  case MTL::PixelFormatBC5_RGSnorm:
  case MTL::PixelFormatBC6H_RGBFloat:
  case MTL::PixelFormatBC6H_RGBUfloat:
  case MTL::PixelFormatBC7_RGBAUnorm:
  case MTL::PixelFormatBC7_RGBAUnorm_sRGB:
  case MTL::PixelFormatPVRTC_RGB_2BPP:
  case MTL::PixelFormatPVRTC_RGB_2BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGB_4BPP:
  case MTL::PixelFormatPVRTC_RGB_4BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGBA_2BPP:
  case MTL::PixelFormatPVRTC_RGBA_2BPP_sRGB:
  case MTL::PixelFormatPVRTC_RGBA_4BPP:
  case MTL::PixelFormatPVRTC_RGBA_4BPP_sRGB:
  case MTL::PixelFormatEAC_R11Unorm:
  case MTL::PixelFormatEAC_R11Snorm:
  case MTL::PixelFormatEAC_RG11Unorm:
  case MTL::PixelFormatEAC_RG11Snorm:
  case MTL::PixelFormatEAC_RGBA8:
  case MTL::PixelFormatEAC_RGBA8_sRGB:
  case MTL::PixelFormatETC2_RGB8:
  case MTL::PixelFormatETC2_RGB8_sRGB:
  case MTL::PixelFormatETC2_RGB8A1:
  case MTL::PixelFormatETC2_RGB8A1_sRGB:
  case MTL::PixelFormatASTC_4x4_sRGB:
  case MTL::PixelFormatASTC_5x4_sRGB:
  case MTL::PixelFormatASTC_5x5_sRGB:
  case MTL::PixelFormatASTC_6x5_sRGB:
  case MTL::PixelFormatASTC_6x6_sRGB:
  case MTL::PixelFormatASTC_8x5_sRGB:
  case MTL::PixelFormatASTC_8x6_sRGB:
  case MTL::PixelFormatASTC_8x8_sRGB:
  case MTL::PixelFormatASTC_10x5_sRGB:
  case MTL::PixelFormatASTC_10x6_sRGB:
  case MTL::PixelFormatASTC_10x8_sRGB:
  case MTL::PixelFormatASTC_10x10_sRGB:
  case MTL::PixelFormatASTC_12x10_sRGB:
  case MTL::PixelFormatASTC_12x12_sRGB:
  case MTL::PixelFormatASTC_4x4_LDR:
  case MTL::PixelFormatASTC_5x4_LDR:
  case MTL::PixelFormatASTC_5x5_LDR:
  case MTL::PixelFormatASTC_6x5_LDR:
  case MTL::PixelFormatASTC_6x6_LDR:
  case MTL::PixelFormatASTC_8x5_LDR:
  case MTL::PixelFormatASTC_8x6_LDR:
  case MTL::PixelFormatASTC_8x8_LDR:
  case MTL::PixelFormatASTC_10x5_LDR:
  case MTL::PixelFormatASTC_10x6_LDR:
  case MTL::PixelFormatASTC_10x8_LDR:
  case MTL::PixelFormatASTC_10x10_LDR:
  case MTL::PixelFormatASTC_12x10_LDR:
  case MTL::PixelFormatASTC_12x12_LDR:
  case MTL::PixelFormatASTC_4x4_HDR:
  case MTL::PixelFormatASTC_5x4_HDR:
  case MTL::PixelFormatASTC_5x5_HDR:
  case MTL::PixelFormatASTC_6x5_HDR:
  case MTL::PixelFormatASTC_6x6_HDR:
  case MTL::PixelFormatASTC_8x5_HDR:
  case MTL::PixelFormatASTC_8x6_HDR:
  case MTL::PixelFormatASTC_8x8_HDR:
  case MTL::PixelFormatASTC_10x5_HDR:
  case MTL::PixelFormatASTC_10x6_HDR:
  case MTL::PixelFormatASTC_10x8_HDR:
  case MTL::PixelFormatASTC_10x10_HDR:
  case MTL::PixelFormatASTC_12x10_HDR:
  case MTL::PixelFormatASTC_12x12_HDR:
  case MTL::PixelFormatGBGR422:
  case MTL::PixelFormatBGRG422:
  case MTL::PixelFormatDepth16Unorm:
  case MTL::PixelFormatDepth32Float:
  case MTL::PixelFormatStencil8:
  case MTL::PixelFormatDepth24Unorm_Stencil8:
  case MTL::PixelFormatDepth32Float_Stencil8:
  case MTL::PixelFormatX32_Stencil8:
  case MTL::PixelFormatX24_Stencil8:
  case MTL::PixelFormatCount:
  default:
    break;
  };
  return MTL::PixelFormatInvalid;
};

} // namespace dxmt