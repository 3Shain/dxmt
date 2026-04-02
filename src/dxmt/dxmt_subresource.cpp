#include "dxmt_subresource.hpp"
#include "dxmt_texture.hpp"

namespace dxmt {

unsigned
getPlanarCount(WMTPixelFormat format) {
  switch (format) {
  case WMTPixelFormatX32G8X32:
  case WMTPixelFormatR32X8X32:
  case WMTPixelFormatDepth32Float_Stencil8:
  case WMTPixelFormatDepth24Unorm_Stencil8:
  case WMTPixelFormatX32_Stencil8:
  case WMTPixelFormatX24_Stencil8:
    return 2;
  default:
    break;
  }
  return 1;
}

unsigned
getPlanarMask(WMTPixelFormat format) {
  switch (format) {
  case WMTPixelFormatX32G8X32:
    return 0b10;
  case WMTPixelFormatR32X8X32:
    return 0b01;
  case WMTPixelFormatDepth32Float_Stencil8:
  case WMTPixelFormatDepth24Unorm_Stencil8:
    return 0b11;
  case WMTPixelFormatX32_Stencil8:
  case WMTPixelFormatX24_Stencil8:
    return 0b10;
  default:
    break;
  }
  return 1;
}

ResourceSubsetState::ResourceSubsetState(
    const TextureViewDescriptor *desc, uint32_t total_mip_count, uint32_t total_array_size, uint32_t ignore_planar_mask
) {
  auto total_planar = getPlanarCount(desc->format);
  auto planar_mask = getPlanarMask(desc->format) & ~ignore_planar_mask;
  if (total_planar * total_mip_count * total_array_size <= 62) {
    encoded_tag = 0b11;
    uint64_t bits = 0;
    for (auto planar = 0u; planar < total_planar; planar++) {
      // unsigned int bit-fields promote to int if narrower than int
      for (auto slice = desc->firstArraySlice; slice < unsigned(desc->firstArraySlice + desc->arraySize); slice++) {
        for (auto level = desc->firstMiplevel; level < unsigned(desc->firstMiplevel + desc->miplevelCount); level++) {
          if ((1 << planar) & planar_mask)
            bits |= 1ull << (planar * total_array_size * total_mip_count + slice * total_mip_count + level);
        }
      }
    }
    texture_bitmask.mask = bits;
  } else {
    encoded_tag = 0b10;

    texture.mip_start = desc->firstMiplevel;
    texture.mip_end = desc->firstMiplevel + desc->miplevelCount;
    texture.array_start = desc->firstArraySlice;
    texture.array_end = desc->firstArraySlice + desc->arraySize;
    texture.planar_mask = planar_mask;
  }
}

} // namespace dxmt
