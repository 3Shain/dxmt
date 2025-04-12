#ifndef _WINEMETAL_H
#define _WINEMETAL_H

#ifdef __cplusplus
#include <cstdint>
#include <cstdbool>
#define STATIC_ASSERT(x) static_assert(x)
#else
#include <stdint.h>
#include <stdbool.h>
#define STATIC_ASSERT(x)
#endif

#ifndef WINEMETAL_API
#ifdef __cplusplus
#define WINEMETAL_API extern "C" __declspec(dllimport)
#else
#define WINEMETAL_API __declspec(dllimport)
#endif
#endif

typedef uint64_t obj_handle_t;

#define NULL_OBJECT_HANDLE 0

WINEMETAL_API void NSObject_retain(obj_handle_t obj);

WINEMETAL_API void NSObject_release(obj_handle_t obj);

WINEMETAL_API obj_handle_t NSArray_object(obj_handle_t array, uint64_t index);

WINEMETAL_API uint64_t NSArray_count(obj_handle_t array);

WINEMETAL_API obj_handle_t WMTCopyAllDevices();

enum WMTStringEncoding : uint64_t {
  WMTASCIIStringEncoding = 1,
  WMTNEXTSTEPStringEncoding = 2,
  WMTJapaneseEUCStringEncoding = 3,
  WMTUTF8StringEncoding = 4,
  WMTWMTISOLatin1StringEncoding = 5,
  SymbolStringEncoding = 6,
  WMTNonLossyASCIIStringEncoding = 7,
  WMTShiftJISStringEncoding = 8,
  WMTISOLatin2StringEncoding = 9,
  WMTUnicodeStringEncoding = 10,
  WMTWindowsCP1251StringEncoding = 11,
  WMTWindowsCP1252StringEncoding = 12,
  WMTWindowsCP1253StringEncoding = 13,
  WMTWindowsCP1254StringEncoding = 14,
  WMTWindowsCP1250StringEncoding = 15,
  WMTISO2022JPStringEncoding = 21,
  WMTMacOSRomanStringEncoding = 30,

  WMTUTF16StringEncoding = WMTUnicodeStringEncoding,

  WMTUTF16BigEndianStringEncoding = 0x90000100,
  WMTUTF16LittleEndianStringEncoding = 0x94000100,

  WMTUTF32StringEncoding = 0x8c000100,
  WMTUTF32BigEndianStringEncoding = 0x98000100,
  WMTUTF32LittleEndianStringEncoding = 0x9c000100
};

WINEMETAL_API uint64_t MTLDevice_recommendedMaxWorkingSetSize(obj_handle_t device);

WINEMETAL_API uint64_t MTLDevice_currentAllocatedSize(obj_handle_t device);

WINEMETAL_API obj_handle_t MTLDevice_name(obj_handle_t device);

WINEMETAL_API uint32_t
NSString_getCString(obj_handle_t str, char *buffer, uint64_t maxLength, enum WMTStringEncoding encoding);

WINEMETAL_API obj_handle_t MTLDevice_newCommandQueue(obj_handle_t device, uint64_t maxCommandBufferCount);

WINEMETAL_API obj_handle_t NSAutoreleasePool_alloc_init();

WINEMETAL_API obj_handle_t MTLCommandQueue_commandBuffer(obj_handle_t queue);

WINEMETAL_API void MTLCommandBuffer_commit(obj_handle_t cmdbuf);

WINEMETAL_API void MTLCommandBuffer_waitUntilCompleted(obj_handle_t cmdbuf);

enum WMTCommandBufferStatus : uint64_t {
  WMTCommandBufferStatusNotEnqueued = 0,
  WMTCommandBufferStatusEnqueued = 1,
  WMTCommandBufferStatusCommitted = 2,
  WMTCommandBufferStatusScheduled = 3,
  WMTCommandBufferStatusCompleted = 4,
  WMTCommandBufferStatusError = 5,
};

WINEMETAL_API enum WMTCommandBufferStatus MTLCommandBuffer_status(obj_handle_t cmdbuf);

WINEMETAL_API obj_handle_t MTLDevice_newSharedEvent(obj_handle_t device);

WINEMETAL_API uint64_t MTLSharedEvent_signaledValue(obj_handle_t event);

WINEMETAL_API void MTLCommandBuffer_encodeSignalEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value);

enum WMTResourceOptions : uint64_t {
  WMTResourceCPUCacheModeDefaultCache = 0,
  WMTResourceCPUCacheModeWriteCombined = 1,
  WMTResourceStorageModeShared = 0,
  WMTResourceStorageModeManaged = 16,
  WMTResourceStorageModePrivate = 32,
  WMTResourceStorageModeMemoryless = 48,
  WMTResourceHazardTrackingModeDefault = 0,
  WMTResourceHazardTrackingModeUntracked = 256,
  WMTResourceHazardTrackingModeTracked = 512,
  WMTResourceOptionCPUCacheModeDefault = 0,
  WMTResourceOptionCPUCacheModeWriteCombined = 1,
};

struct WMTMemoryPointer {
  void *ptr;
#if defined(__i386__)
  uint32_t high_part;
#endif

#ifdef __cplusplus
  void
  set(void *p) {
#if defined(__i386__)
    high_part = 0;
#endif
    ptr = p;
  }

  void *
  get() {
#if defined(__i386__)
    assert(!high_part && "inaccessible 64-bit pointer");
#endif
    return ptr;
  }
#endif
};

STATIC_ASSERT(sizeof(WMTMemoryPointer) == 8);

struct WMTBufferInfo {
  uint64_t length;                 // in
  enum WMTResourceOptions options; // in
  struct WMTMemoryPointer memory;  // inout
  uint64_t gpu_address;            // out
};

STATIC_ASSERT(sizeof(WMTBufferInfo) == 32);

WINEMETAL_API obj_handle_t MTLDevice_newBuffer(obj_handle_t device, struct WMTBufferInfo *info);

enum WMTSamplerBorderColor : uint8_t {
  WMTSamplerBorderColorTransparentBlack = 0,
  WMTSamplerBorderColorOpaqueBlack = 1,
  WMTSamplerBorderColorOpaqueWhite = 2,
};

enum WMTSamplerAddressMode : uint8_t {
  WMTSamplerAddressModeClampToEdge = 0,
  WMTSamplerAddressModeMirrorClampToEdge = 1,
  WMTSamplerAddressModeRepeat = 2,
  WMTSamplerAddressModeMirrorRepeat = 3,
  WMTSamplerAddressModeClampToZero = 4,
  WMTSamplerAddressModeClampToBorderColor = 5,
};

enum WMTSamplerMipFilter : uint8_t {
  WMTSamplerMipFilterNotMipmapped = 0,
  WMTSamplerMipFilterNearest = 1,
  WMTSamplerMipFilterLinear = 2,
};

enum WMTSamplerMinMagFilter : uint8_t {
  WMTSamplerMinMagFilterNearest = 0,
  WMTSamplerMinMagFilterLinear = 1,
};

enum WMTCompareFunction : uint8_t {
  WMTCompareFunctionNever = 0,
  WMTCompareFunctionLess = 1,
  WMTCompareFunctionEqual = 2,
  WMTCompareFunctionLessEqual = 3,
  WMTCompareFunctionGreater = 4,
  WMTCompareFunctionNotEqual = 5,
  WMTCompareFunctionGreaterEqual = 6,
  WMTCompareFunctionAlways = 7,
};

struct WMTSamplerInfo {
  enum WMTSamplerMinMagFilter min_filter;
  enum WMTSamplerMinMagFilter mag_filter;
  enum WMTSamplerMipFilter mip_filter;
  enum WMTSamplerAddressMode r_address_mode;
  enum WMTSamplerAddressMode s_address_mode;
  enum WMTSamplerAddressMode t_address_mode;
  enum WMTSamplerBorderColor border_color;
  enum WMTCompareFunction compare_function;
  float lod_min_clamp;
  float lod_max_clamp;
  uint32_t max_anisotroy;
  bool normalized_coords;
  bool lod_average;
  bool support_argument_buffers;
  uint64_t gpu_resource_id; // out
};

STATIC_ASSERT(sizeof(WMTSamplerInfo) == 32);

WINEMETAL_API obj_handle_t MTLDevice_newSamplerState(obj_handle_t device, struct WMTSamplerInfo *info);

enum WMTStencilOperation : uint8_t {
  WMTStencilOperationKeep = 0,
  WMTStencilOperationZero = 1,
  WMTStencilOperationReplace = 2,
  WMTStencilOperationIncrementClamp = 3,
  WMTStencilOperationDecrementClamp = 4,
  WMTStencilOperationInvert = 5,
  WMTStencilOperationIncrementWrap = 6,
  WMTStencilOperationDecrementWrap = 7,
};

struct WMTStencilInfo {
  bool enabled;
  enum WMTStencilOperation depth_stencil_pass_op;
  enum WMTStencilOperation stencil_fail_op;
  enum WMTStencilOperation depth_fail_op;
  enum WMTCompareFunction stencil_compare_function;
  uint8_t write_mask;
  uint8_t read_mask;
};

struct WMTDepthStencilInfo {
  enum WMTCompareFunction depth_compare_function;
  bool depth_write_enabled;
  struct WMTStencilInfo front_stencil;
  struct WMTStencilInfo back_stencil;
};

WINEMETAL_API obj_handle_t MTLDevice_newDepthStencilState(obj_handle_t device, struct WMTDepthStencilInfo *info);

enum WMTPixelFormat : uint32_t {
  WMTPixelFormatInvalid = 0,
  WMTPixelFormatA8Unorm = 1,
  WMTPixelFormatR8Unorm = 10,
  WMTPixelFormatR8Unorm_sRGB = 11,
  WMTPixelFormatR8Snorm = 12,
  WMTPixelFormatR8Uint = 13,
  WMTPixelFormatR8Sint = 14,
  WMTPixelFormatR16Unorm = 20,
  WMTPixelFormatR16Snorm = 22,
  WMTPixelFormatR16Uint = 23,
  WMTPixelFormatR16Sint = 24,
  WMTPixelFormatR16Float = 25,
  WMTPixelFormatRG8Unorm = 30,
  WMTPixelFormatRG8Unorm_sRGB = 31,
  WMTPixelFormatRG8Snorm = 32,
  WMTPixelFormatRG8Uint = 33,
  WMTPixelFormatRG8Sint = 34,
  WMTPixelFormatB5G6R5Unorm = 40,
  WMTPixelFormatA1BGR5Unorm = 41,
  WMTPixelFormatABGR4Unorm = 42,
  WMTPixelFormatBGR5A1Unorm = 43,
  WMTPixelFormatR32Uint = 53,
  WMTPixelFormatR32Sint = 54,
  WMTPixelFormatR32Float = 55,
  WMTPixelFormatRG16Unorm = 60,
  WMTPixelFormatRG16Snorm = 62,
  WMTPixelFormatRG16Uint = 63,
  WMTPixelFormatRG16Sint = 64,
  WMTPixelFormatRG16Float = 65,
  WMTPixelFormatRGBA8Unorm = 70,
  WMTPixelFormatRGBA8Unorm_sRGB = 71,
  WMTPixelFormatRGBA8Snorm = 72,
  WMTPixelFormatRGBA8Uint = 73,
  WMTPixelFormatRGBA8Sint = 74,
  WMTPixelFormatBGRA8Unorm = 80,
  WMTPixelFormatBGRA8Unorm_sRGB = 81,
  WMTPixelFormatRGB10A2Unorm = 90,
  WMTPixelFormatRGB10A2Uint = 91,
  WMTPixelFormatRG11B10Float = 92,
  WMTPixelFormatRGB9E5Float = 93,
  WMTPixelFormatBGR10A2Unorm = 94,
  WMTPixelFormatBGR10_XR = 554,
  WMTPixelFormatBGR10_XR_sRGB = 555,
  WMTPixelFormatRG32Uint = 103,
  WMTPixelFormatRG32Sint = 104,
  WMTPixelFormatRG32Float = 105,
  WMTPixelFormatRGBA16Unorm = 110,
  WMTPixelFormatRGBA16Snorm = 112,
  WMTPixelFormatRGBA16Uint = 113,
  WMTPixelFormatRGBA16Sint = 114,
  WMTPixelFormatRGBA16Float = 115,
  WMTPixelFormatBGRA10_XR = 552,
  WMTPixelFormatBGRA10_XR_sRGB = 553,
  WMTPixelFormatRGBA32Uint = 123,
  WMTPixelFormatRGBA32Sint = 124,
  WMTPixelFormatRGBA32Float = 125,
  WMTPixelFormatBC1_RGBA = 130,
  WMTPixelFormatBC1_RGBA_sRGB = 131,
  WMTPixelFormatBC2_RGBA = 132,
  WMTPixelFormatBC2_RGBA_sRGB = 133,
  WMTPixelFormatBC3_RGBA = 134,
  WMTPixelFormatBC3_RGBA_sRGB = 135,
  WMTPixelFormatBC4_RUnorm = 140,
  WMTPixelFormatBC4_RSnorm = 141,
  WMTPixelFormatBC5_RGUnorm = 142,
  WMTPixelFormatBC5_RGSnorm = 143,
  WMTPixelFormatBC6H_RGBFloat = 150,
  WMTPixelFormatBC6H_RGBUfloat = 151,
  WMTPixelFormatBC7_RGBAUnorm = 152,
  WMTPixelFormatBC7_RGBAUnorm_sRGB = 153,
  WMTPixelFormatPVRTC_RGB_2BPP = 160,
  WMTPixelFormatPVRTC_RGB_2BPP_sRGB = 161,
  WMTPixelFormatPVRTC_RGB_4BPP = 162,
  WMTPixelFormatPVRTC_RGB_4BPP_sRGB = 163,
  WMTPixelFormatPVRTC_RGBA_2BPP = 164,
  WMTPixelFormatPVRTC_RGBA_2BPP_sRGB = 165,
  WMTPixelFormatPVRTC_RGBA_4BPP = 166,
  WMTPixelFormatPVRTC_RGBA_4BPP_sRGB = 167,
  WMTPixelFormatEAC_R11Unorm = 170,
  WMTPixelFormatEAC_R11Snorm = 172,
  WMTPixelFormatEAC_RG11Unorm = 174,
  WMTPixelFormatEAC_RG11Snorm = 176,
  WMTPixelFormatEAC_RGBA8 = 178,
  WMTPixelFormatEAC_RGBA8_sRGB = 179,
  WMTPixelFormatETC2_RGB8 = 180,
  WMTPixelFormatETC2_RGB8_sRGB = 181,
  WMTPixelFormatETC2_RGB8A1 = 182,
  WMTPixelFormatETC2_RGB8A1_sRGB = 183,
  WMTPixelFormatASTC_4x4_sRGB = 186,
  WMTPixelFormatASTC_5x4_sRGB = 187,
  WMTPixelFormatASTC_5x5_sRGB = 188,
  WMTPixelFormatASTC_6x5_sRGB = 189,
  WMTPixelFormatASTC_6x6_sRGB = 190,
  WMTPixelFormatASTC_8x5_sRGB = 192,
  WMTPixelFormatASTC_8x6_sRGB = 193,
  WMTPixelFormatASTC_8x8_sRGB = 194,
  WMTPixelFormatASTC_10x5_sRGB = 195,
  WMTPixelFormatASTC_10x6_sRGB = 196,
  WMTPixelFormatASTC_10x8_sRGB = 197,
  WMTPixelFormatASTC_10x10_sRGB = 198,
  WMTPixelFormatASTC_12x10_sRGB = 199,
  WMTPixelFormatASTC_12x12_sRGB = 200,
  WMTPixelFormatASTC_4x4_LDR = 204,
  WMTPixelFormatASTC_5x4_LDR = 205,
  WMTPixelFormatASTC_5x5_LDR = 206,
  WMTPixelFormatASTC_6x5_LDR = 207,
  WMTPixelFormatASTC_6x6_LDR = 208,
  WMTPixelFormatASTC_8x5_LDR = 210,
  WMTPixelFormatASTC_8x6_LDR = 211,
  WMTPixelFormatASTC_8x8_LDR = 212,
  WMTPixelFormatASTC_10x5_LDR = 213,
  WMTPixelFormatASTC_10x6_LDR = 214,
  WMTPixelFormatASTC_10x8_LDR = 215,
  WMTPixelFormatASTC_10x10_LDR = 216,
  WMTPixelFormatASTC_12x10_LDR = 217,
  WMTPixelFormatASTC_12x12_LDR = 218,
  WMTPixelFormatASTC_4x4_HDR = 222,
  WMTPixelFormatASTC_5x4_HDR = 223,
  WMTPixelFormatASTC_5x5_HDR = 224,
  WMTPixelFormatASTC_6x5_HDR = 225,
  WMTPixelFormatASTC_6x6_HDR = 226,
  WMTPixelFormatASTC_8x5_HDR = 228,
  WMTPixelFormatASTC_8x6_HDR = 229,
  WMTPixelFormatASTC_8x8_HDR = 230,
  WMTPixelFormatASTC_10x5_HDR = 231,
  WMTPixelFormatASTC_10x6_HDR = 232,
  WMTPixelFormatASTC_10x8_HDR = 233,
  WMTPixelFormatASTC_10x10_HDR = 234,
  WMTPixelFormatASTC_12x10_HDR = 235,
  WMTPixelFormatASTC_12x12_HDR = 236,
  WMTPixelFormatGBGR422 = 240,
  WMTPixelFormatBGRG422 = 241,
  WMTPixelFormatDepth16Unorm = 250,
  WMTPixelFormatDepth32Float = 252,
  WMTPixelFormatStencil8 = 253,
  WMTPixelFormatDepth24Unorm_Stencil8 = 255,
  WMTPixelFormatDepth32Float_Stencil8 = 260,
  WMTPixelFormatX32_Stencil8 = 261,
  WMTPixelFormatX24_Stencil8 = 262,
};

enum WMTTextureType : uint8_t {
  WMTTextureType1D = 0,
  WMTTextureType1DArray = 1,
  WMTTextureType2D = 2,
  WMTTextureType2DArray = 3,
  WMTTextureType2DMultisample = 4,
  WMTTextureTypeCube = 5,
  WMTTextureTypeCubeArray = 6,
  WMTTextureType3D = 7,
  WMTTextureType2DMultisampleArray = 8,
  WMTTextureTypeTextureBuffer = 9,
};

enum WMTTextureUsage : uint8_t {
  WMTTextureUsageUnknown = 0,
  WMTTextureUsageShaderRead = 1,
  WMTTextureUsageShaderWrite = 2,
  WMTTextureUsageRenderTarget = 4,
  WMTTextureUsagePixelFormatView = 16,
  WMTTextureUsageShaderAtomic = 32,
};

enum WMTTextureSwizzle : uint8_t {
  WMTTextureSwizzleZero = 0,
  WMTTextureSwizzleOne = 1,
  WMTTextureSwizzleRed = 2,
  WMTTextureSwizzleGreen = 3,
  WMTTextureSwizzleBlue = 4,
  WMTTextureSwizzleAlpha = 5,
};

struct WMTTextureSwizzleChannels {
  enum WMTTextureSwizzle r;
  enum WMTTextureSwizzle g;
  enum WMTTextureSwizzle b;
  enum WMTTextureSwizzle a;
};

struct WMTTextureInfo {
  enum WMTPixelFormat pixel_format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t array_length;
  enum WMTTextureType type;
  uint8_t mipmap_level_count;
  uint8_t sample_count;
  enum WMTTextureUsage usage;
  enum WMTResourceOptions options;
  uint64_t gpu_resource_id; // out
};

WINEMETAL_API obj_handle_t MTLDevice_newTexture(obj_handle_t device, struct WMTTextureInfo *info);

WINEMETAL_API obj_handle_t
MTLBuffer_newTexture(obj_handle_t buffer, struct WMTTextureInfo *info, uint64_t offset, uint64_t bytes_per_row);

WINEMETAL_API obj_handle_t MTLTexture_newTextureView(
    obj_handle_t texture, enum WMTPixelFormat format, enum WMTTextureType texture_type, uint16_t level_start,
    uint16_t level_count, uint16_t slice_start, uint16_t slice_count, struct WMTTextureSwizzleChannels swizzle,
    uint64_t *out_gpu_resource_id
);

WINEMETAL_API uint64_t
MTLDevice_minimumLinearTextureAlignmentForPixelFormat(obj_handle_t device, enum WMTPixelFormat format);

enum WMTAttributeFormat : uint32_t {
  WMTAttributeFormatInvalid = 0,
  WMTAttributeFormatUChar2 = 1,
  WMTAttributeFormatUChar3 = 2,
  WMTAttributeFormatUChar4 = 3,
  WMTAttributeFormatChar2 = 4,
  WMTAttributeFormatChar3 = 5,
  WMTAttributeFormatChar4 = 6,
  WMTAttributeFormatUChar2Normalized = 7,
  WMTAttributeFormatUChar3Normalized = 8,
  WMTAttributeFormatUChar4Normalized = 9,
  WMTAttributeFormatChar2Normalized = 10,
  WMTAttributeFormatChar3Normalized = 11,
  WMTAttributeFormatChar4Normalized = 12,
  WMTAttributeFormatUShort2 = 13,
  WMTAttributeFormatUShort3 = 14,
  WMTAttributeFormatUShort4 = 15,
  WMTAttributeFormatShort2 = 16,
  WMTAttributeFormatShort3 = 17,
  WMTAttributeFormatShort4 = 18,
  WMTAttributeFormatUShort2Normalized = 19,
  WMTAttributeFormatUShort3Normalized = 20,
  WMTAttributeFormatUShort4Normalized = 21,
  WMTAttributeFormatShort2Normalized = 22,
  WMTAttributeFormatShort3Normalized = 23,
  WMTAttributeFormatShort4Normalized = 24,
  WMTAttributeFormatHalf2 = 25,
  WMTAttributeFormatHalf3 = 26,
  WMTAttributeFormatHalf4 = 27,
  WMTAttributeFormatFloat = 28,
  WMTAttributeFormatFloat2 = 29,
  WMTAttributeFormatFloat3 = 30,
  WMTAttributeFormatFloat4 = 31,
  WMTAttributeFormatInt = 32,
  WMTAttributeFormatInt2 = 33,
  WMTAttributeFormatInt3 = 34,
  WMTAttributeFormatInt4 = 35,
  WMTAttributeFormatUInt = 36,
  WMTAttributeFormatUInt2 = 37,
  WMTAttributeFormatUInt3 = 38,
  WMTAttributeFormatUInt4 = 39,
  WMTAttributeFormatInt1010102Normalized = 40,
  WMTAttributeFormatUInt1010102Normalized = 41,
  WMTAttributeFormatUChar4Normalized_BGRA = 42,
  WMTAttributeFormatUChar = 45,
  WMTAttributeFormatChar = 46,
  WMTAttributeFormatUCharNormalized = 47,
  WMTAttributeFormatCharNormalized = 48,
  WMTAttributeFormatUShort = 49,
  WMTAttributeFormatShort = 50,
  WMTAttributeFormatUShortNormalized = 51,
  WMTAttributeFormatShortNormalized = 52,
  WMTAttributeFormatHalf = 53,
  WMTAttributeFormatFloatRG11B10 = 54,
  WMTAttributeFormatFloatRGB9E5 = 55,
};

WINEMETAL_API obj_handle_t MTLDevice_newLibrary(
    obj_handle_t device, struct WMTMemoryPointer bytecode, uint64_t bytecode_length, obj_handle_t *err_out
);

WINEMETAL_API obj_handle_t MTLLibrary_newFunction(obj_handle_t library, const char *name);

WINEMETAL_API uint64_t NSString_lengthOfBytesUsingEncoding(obj_handle_t str, enum WMTStringEncoding encoding);

WINEMETAL_API obj_handle_t NSError_description(obj_handle_t nserror);

WINEMETAL_API obj_handle_t
MTLDevice_newComputePipelineState(obj_handle_t device, obj_handle_t function, obj_handle_t *err_out);

#endif