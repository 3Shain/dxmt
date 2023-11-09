#pragma once
#include "dxgi_private.h"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLVertexDescriptor.hpp"
#include "Metal/MTLStageInputOutputDescriptor.hpp"

namespace dxmt {
struct MetalFormatInfo {
  MTL::PixelFormat pixel_format;
  MTL::VertexFormat vertex_format;
  MTL::AttributeFormat attribute_format;
  bool is_typeless;
  uint32_t stride;
  uint32_t alignment;
};

extern __attribute__((dllexport)) const struct MetalFormatInfo g_metal_format_map[DXGI_FORMAT_V408 + 1];

} // namespace dxmt
