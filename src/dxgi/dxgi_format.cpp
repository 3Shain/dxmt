#include "dxgi_format.hpp"
#include "Metal/MTLPixelFormat.hpp"

namespace dxmt {

constexpr struct MetalFormatInfo INVALID = {MTL::PixelFormatInvalid,
                                            MTL::VertexFormatInvalid,
                                            MTL::AttributeFormatInvalid,
                                            false,
                                            0,
                                            0};

// All DXGI _TYPELESS format mapped to UInt
const struct MetalFormatInfo g_metal_format_map[DXGI_FORMAT_V408 + 1] = {
    // DXGI_FORMAT_UNKNOWN
    INVALID,
    // DXGI_FORMAT_R32G32B32A32_TYPELESS
    {MTL::PixelFormatRGBA32Uint, MTL::VertexFormatUInt4,
     MTL::AttributeFormatUInt4, true, 16, 16},
    // DXGI_FORMAT_R32G32B32A32_FLOAT
    {MTL::PixelFormatRGBA32Float, MTL::VertexFormatFloat4,
     MTL::AttributeFormatFloat4, false, 16, 16},
    // DXGI_FORMAT_R32G32B32A32_UINT
    {MTL::PixelFormatRGBA32Uint, MTL::VertexFormatUInt4,
     MTL::AttributeFormatUInt4, false, 16, 16},
    // DXGI_FORMAT_R32G32B32A32_SINT
    {MTL::PixelFormatRGBA32Sint, MTL::VertexFormatInt4,
     MTL::AttributeFormatInt4, false, 16, 16},
    // DXGI_FORMAT_R32G32B32_TYPELESS
    // no such pixel format PixelFormatRGB32Uint
    {MTL::PixelFormatRGBA32Uint, MTL::VertexFormatUInt3,
     MTL::AttributeFormatUInt, true, 12, 16},
    // DXGI_FORMAT_R32G32B32_FLOAT
    // no such pixel format PixelFormatRGB32Float
    {MTL::PixelFormatRGBA32Float, MTL::VertexFormatFloat3,
     MTL::AttributeFormatFloat3, false, 12, 16},
    // DXGI_FORMAT_R32G32B32_UINT
    // no such pixel format PixelFormatRGB32Uint
    {MTL::PixelFormatRGBA32Uint, MTL::VertexFormatUInt3,
     MTL::AttributeFormatUInt3, false, 12, 16},
    // DXGI_FORMAT_R32G32B32_SINT
    // no such pixel format PixelFormatRGB32Sint
    {MTL::PixelFormatRGBA32Sint, MTL::VertexFormatInt3,
     MTL::AttributeFormatInt3, false, 12, 16},
    // DXGI_FORMAT_R16G16B16A16_TYPELESS
    {MTL::PixelFormatRGBA16Uint, MTL::VertexFormatUShort4,
     MTL::AttributeFormatUShort4, true, 8, 8},
    // DXGI_FORMAT_R16G16B16A16_FLOAT
    {MTL::PixelFormatRGBA16Float, MTL::VertexFormatHalf4,
     MTL::AttributeFormatHalf4, false, 8, 8},
    // DXGI_FORMAT_R16G16B16A16_UNORM
    {MTL::PixelFormatRGBA16Unorm, MTL::VertexFormatUShort4Normalized,
     MTL::AttributeFormatUShort4Normalized, false, 8, 8},
    // DXGI_FORMAT_R16G16B16A16_UINT
    {MTL::PixelFormatRGBA16Uint, MTL::VertexFormatUShort4,
     MTL::AttributeFormatUShort4, false, 8, 8},
    // DXGI_FORMAT_R16G16B16A16_SNORM
    {MTL::PixelFormatRGBA16Snorm, MTL::VertexFormatShort4Normalized,
     MTL::AttributeFormatShort4Normalized, false, 8, 8},
    // DXGI_FORMAT_R16G16B16A16_SINT
    {MTL::PixelFormatRGBA16Sint, MTL::VertexFormatShort4,
     MTL::AttributeFormatShort4, false, 8, 8},
    // DXGI_FORMAT_R32G32_TYPELESS
    {MTL::PixelFormatRG32Uint, MTL::VertexFormatUInt2,
     MTL::AttributeFormatUInt2, true, 8, 8},
    // DXGI_FORMAT_R32G32_FLOAT
    {MTL::PixelFormatRG32Float, MTL::VertexFormatFloat2,
     MTL::AttributeFormatFloat2, false, 8, 8},
    // DXGI_FORMAT_R32G32_UINT
    {MTL::PixelFormatRG32Uint, MTL::VertexFormatUInt2,
     MTL::AttributeFormatUInt2, false, 8, 8},
    // DXGI_FORMAT_R32G32_SINT
    {MTL::PixelFormatRG32Sint, MTL::VertexFormatInt2, MTL::AttributeFormatInt2,
     false, 8, 8},
    // DXGI_FORMAT_R32G8X24_TYPELESS
    INVALID,
    // DXGI_FORMAT_D32_FLOAT_S8X24_UINT
    {MTL::PixelFormatDepth32Float_Stencil8, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS
    INVALID,
    // DXGI_FORMAT_X32_TYPELESS_G8X24_UINT
    INVALID,
    // DXGI_FORMAT_R10G10B10A2_TYPELESS
    {MTL::PixelFormatRGB10A2Uint, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, true, 4, 4},
    // DXGI_FORMAT_R10G10B10A2_UNORM
    {MTL::PixelFormatRGB10A2Unorm, MTL::VertexFormatUInt1010102Normalized,
     MTL::AttributeFormatUInt1010102Normalized, false, 4, 4},
    // DXGI_FORMAT_R10G10B10A2_UINT
    {MTL::PixelFormatRGB10A2Uint, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 4, 4},
    // DXGI_FORMAT_R11G11B10_FLOAT
    {MTL::PixelFormatRG11B10Float, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 4, 4},
    // DXGI_FORMAT_R8G8B8A8_TYPELESS
    {MTL::PixelFormatRGBA8Uint, MTL::VertexFormatUChar4,
     MTL::AttributeFormatUChar4, true, 4, 4},
    // DXGI_FORMAT_R8G8B8A8_UNORM
    {MTL::PixelFormatRGBA8Unorm, MTL::VertexFormatUChar4Normalized,
     MTL::AttributeFormatUChar4Normalized, false, 4, 4},
    // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    {MTL::PixelFormatRGBA8Unorm_sRGB, MTL::VertexFormatUChar4Normalized,
     MTL::AttributeFormatUChar4Normalized, false, 4, 4},
    // DXGI_FORMAT_R8G8B8A8_UINT
    {MTL::PixelFormatRGBA8Uint, MTL::VertexFormatUChar4,
     MTL::AttributeFormatUChar4, false, 4, 4},
    // DXGI_FORMAT_R8G8B8A8_SNORM
    {MTL::PixelFormatRGBA8Snorm, MTL::VertexFormatChar4Normalized,
     MTL::AttributeFormatChar4Normalized, false, 4, 4},
    // DXGI_FORMAT_R8G8B8A8_SINT
    {MTL::PixelFormatRGBA8Sint, MTL::VertexFormatChar4,
     MTL::AttributeFormatChar4, false, 4, 4},
    // DXGI_FORMAT_R16G16_TYPELESS
    {MTL::PixelFormatRG16Uint, MTL::VertexFormatUShort2,
     MTL::AttributeFormatUShort2, true, 4, 4},
    // DXGI_FORMAT_R16G16_FLOAT
    {MTL::PixelFormatRG16Float, MTL::VertexFormatHalf2,
     MTL::AttributeFormatHalf2, false, 4, 4},
    // DXGI_FORMAT_R16G16_UNORM
    {MTL::PixelFormatRG16Unorm, MTL::VertexFormatUShort2Normalized,
     MTL::AttributeFormatUShort2Normalized, false, 4, 4},
    // DXGI_FORMAT_R16G16_UINT
    {MTL::PixelFormatRG16Uint, MTL::VertexFormatUShort2,
     MTL::AttributeFormatUShort2, false, 4, 4},
    // DXGI_FORMAT_R16G16_SNORM
    {MTL::PixelFormatRG16Snorm, MTL::VertexFormatShort2Normalized,
     MTL::AttributeFormatShort2Normalized, false, 4, 4},
    // DXGI_FORMAT_R16G16_SINT
    {MTL::PixelFormatRG16Sint, MTL::VertexFormatShort2,
     MTL::AttributeFormatShort2, false, 4, 4},
    // DXGI_FORMAT_R32_TYPELESS
    {MTL::PixelFormatR32Uint, MTL::VertexFormatUInt, MTL::AttributeFormatUInt,
     true, 4, 4},
    // DXGI_FORMAT_D32_FLOAT
    {MTL::PixelFormatDepth32Float, MTL::VertexFormatFloat,
     MTL::AttributeFormatFloat, false, 4, 4},
    // DXGI_FORMAT_R32_FLOAT
    {MTL::PixelFormatR32Float, MTL::VertexFormatFloat,
     MTL::AttributeFormatFloat, false, 4, 4},
    // DXGI_FORMAT_R32_UINT
    {MTL::PixelFormatR32Uint, MTL::VertexFormatUInt, MTL::AttributeFormatUInt,
     false, 4, 4},
    // DXGI_FORMAT_R32_SINT
    {MTL::PixelFormatR32Sint, MTL::VertexFormatInt, MTL::AttributeFormatInt,
     false, 4, 4},
    // DXGI_FORMAT_R24G8_TYPELESS
    INVALID,
    // DXGI_FORMAT_D24_UNORM_S8_UINT
    // TODO: PixelFormatDepth24Unorm_Stencil8 not supported on apple silicon..
    {MTL::PixelFormatDepth32Float_Stencil8, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    INVALID,
    // DXGI_FORMAT_X24_TYPELESS_G8_UINT
    INVALID,
    // DXGI_FORMAT_R8G8_TYPELESS
    {MTL::PixelFormatRG8Uint, MTL::VertexFormatUChar2,
     MTL::AttributeFormatUChar2, true, 2, 2},
    // DXGI_FORMAT_R8G8_UNORM
    {MTL::PixelFormatRG8Unorm, MTL::VertexFormatUChar2Normalized,
     MTL::AttributeFormatUChar2Normalized, false, 2, 2},
    // DXGI_FORMAT_R8G8_UINT
    {MTL::PixelFormatRG8Uint, MTL::VertexFormatUChar2,
     MTL::AttributeFormatUChar2, false, 2, 2},
    // DXGI_FORMAT_R8G8_SNORM
    {MTL::PixelFormatRG8Snorm, MTL::VertexFormatChar2Normalized,
     MTL::AttributeFormatChar2Normalized, false, 2, 2},
    // DXGI_FORMAT_R8G8_SINT
    {MTL::PixelFormatRG8Sint, MTL::VertexFormatChar2, MTL::AttributeFormatChar2,
     false, 2, 2},
    // DXGI_FORMAT_R16_TYPELESS
    {MTL::PixelFormatR16Uint, MTL::VertexFormatUShort,
     MTL::AttributeFormatUShort, true, 2, 2},
    // DXGI_FORMAT_R16_FLOAT
    {MTL::PixelFormatR16Float, MTL::VertexFormatHalf, MTL::AttributeFormatHalf,
     false, 2, 2},
    // DXGI_FORMAT_D16_UNORM
    {MTL::PixelFormatDepth16Unorm,
     MTL::VertexFormatUShortNormalized, // TODO: verify it
     MTL::AttributeFormatUShortNormalized, false, 2, 2},
    // DXGI_FORMAT_R16_UNORM
    {MTL::PixelFormatR16Unorm, MTL::VertexFormatUShortNormalized,
     MTL::AttributeFormatUShortNormalized, false, 2, 2},
    // DXGI_FORMAT_R16_UINT
    {MTL::PixelFormatR16Uint, MTL::VertexFormatUShort,
     MTL::AttributeFormatUShort, false, 2, 2},
    // DXGI_FORMAT_R16_SNORM
    {MTL::PixelFormatR16Snorm, MTL::VertexFormatShortNormalized,
     MTL::AttributeFormatShortNormalized, false, 2, 2},
    // DXGI_FORMAT_R16_SINT
    {MTL::PixelFormatR16Sint, MTL::VertexFormatShort, MTL::AttributeFormatShort,
     false, 2, 2},
    // DXGI_FORMAT_R8_TYPELESS
    {MTL::PixelFormatR8Uint, MTL::VertexFormatUChar, MTL::AttributeFormatUChar,
     true, 1, 1},
    // DXGI_FORMAT_R8_UNORM
    {MTL::PixelFormatR8Unorm, MTL::VertexFormatUCharNormalized,
     MTL::AttributeFormatUCharNormalized, false, 1, 1},
    // DXGI_FORMAT_R8_UINT
    {MTL::PixelFormatR8Uint, MTL::VertexFormatUChar, MTL::AttributeFormatUChar,
     false, 1, 1},
    // DXGI_FORMAT_R8_SNORM
    {MTL::PixelFormatR8Snorm, MTL::VertexFormatCharNormalized,
     MTL::AttributeFormatCharNormalized, false, 1, 1},
    // DXGI_FORMAT_R8_SINT
    {MTL::PixelFormatR8Sint, MTL::VertexFormatChar, MTL::AttributeFormatChar,
     false, 1, 1},
    // DXGI_FORMAT_A8_UNORM
    {MTL::PixelFormatA8Unorm, MTL::VertexFormatUCharNormalized,
     MTL::AttributeFormatUCharNormalized, false, 1, 1},
    // DXGI_FORMAT_R1_UNORM
    INVALID,
    // DXGI_FORMAT_R9G9B9E5_SHAREDEXP
    {MTL::PixelFormatRGB9E5Float, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_R8G8_B8G8_UNORM
    INVALID,
    // DXGI_FORMAT_G8R8_G8B8_UNORM
    INVALID,
    // DXGI_FORMAT_BC1_TYPELESS
    {MTL::PixelFormatBC1_RGBA, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, true, 0, 0},
    // DXGI_FORMAT_BC1_UNORM
    {MTL::PixelFormatBC1_RGBA, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_BC1_UNORM_SRGB
    {MTL::PixelFormatBC1_RGBA_sRGB, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_BC2_TYPELESS
    {},
    // DXGI_FORMAT_BC2_UNORM
    {},
    // DXGI_FORMAT_BC2_UNORM_SRGB
    {},
    // DXGI_FORMAT_BC3_TYPELESS
    {},
    // DXGI_FORMAT_BC3_UNORM
    {},
    // DXGI_FORMAT_BC3_UNORM_SRGB
    {},
    // DXGI_FORMAT_BC4_TYPELESS
    {},
    // DXGI_FORMAT_BC4_UNORM
    {},
    // DXGI_FORMAT_BC4_SNORM
    {},
    // DXGI_FORMAT_BC5_TYPELESS
    {},
    // DXGI_FORMAT_BC5_UNORM
    {},
    // DXGI_FORMAT_BC5_SNORM
    {},
    // DXGI_FORMAT_B5G6R5_UNORM
    {MTL::PixelFormatB5G6R5Unorm, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_B5G5R5A1_UNORM
    {MTL::PixelFormatBGR5A1Unorm, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_B8G8R8A8_UNORM
    {MTL::PixelFormatBGRA8Unorm, MTL::VertexFormatUChar4Normalized_BGRA,
     MTL::AttributeFormatUChar4Normalized_BGRA, false, 4, 4},
    // DXGI_FORMAT_B8G8R8X8_UNORM
    {},
    // DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM
    {MTL::PixelFormatBGR10_XR, // TODO: verify it
     MTL::VertexFormatInvalid, MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_B8G8R8A8_TYPELESS
    {MTL::PixelFormatBGRA8Unorm, MTL::VertexFormatUChar4Normalized_BGRA,
     MTL::AttributeFormatUChar4Normalized_BGRA, true, 4, 4},
    // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
    {MTL::PixelFormatBGRA8Unorm_sRGB, MTL::VertexFormatUChar4Normalized_BGRA,
     MTL::AttributeFormatUChar4Normalized_BGRA, false, 4, 4},
    // DXGI_FORMAT_B8G8R8X8_TYPELESS
    {},
    // DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
    {},
    // DXGI_FORMAT_BC6H_TYPELESS
    {MTL::PixelFormatBC6H_RGBUfloat, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, true, 0, 0},
    // DXGI_FORMAT_BC6H_UF16
    {MTL::PixelFormatBC6H_RGBUfloat, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_BC6H_SF16
    {MTL::PixelFormatBC6H_RGBFloat, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_BC7_TYPELESS
    {MTL::PixelFormatBC7_RGBAUnorm, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, true, 0, 0},
    // DXGI_FORMAT_BC7_UNORM
    {MTL::PixelFormatBC7_RGBAUnorm, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_BC7_UNORM_SRGB
    {MTL::PixelFormatBC7_RGBAUnorm_sRGB, MTL::VertexFormatInvalid,
     MTL::AttributeFormatInvalid, false, 0, 0},
    // DXGI_FORMAT_AYUV
    INVALID,
    // DXGI_FORMAT_Y410
    INVALID,
    // DXGI_FORMAT_Y416
    INVALID,
    // DXGI_FORMAT_NV12
    INVALID,
    // DXGI_FORMAT_P010
    INVALID,
    // DXGI_FORMAT_P016
    INVALID,
    // DXGI_FORMAT_420_OPAQUE
    INVALID,
    // DXGI_FORMAT_YUY2
    INVALID,
    // DXGI_FORMAT_Y210
    INVALID,
    // DXGI_FORMAT_Y216
    INVALID,
    // DXGI_FORMAT_NV11
    INVALID,
    // DXGI_FORMAT_AI44
    INVALID,
    // DXGI_FORMAT_IA44
    INVALID,
    // DXGI_FORMAT_P8
    INVALID,
    // DXGI_FORMAT_A8P8
    INVALID,
    // DXGI_FORMAT_B4G4R4A4_UNORM
    INVALID,
    // padding 0x74~0x81
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    INVALID,
    // DXGI_FORMAT_P208
    INVALID,
    // DXGI_FORMAT_V208
    INVALID,
    // DXGI_FORMAT_V408
    INVALID,
};
} // namespace dxmt