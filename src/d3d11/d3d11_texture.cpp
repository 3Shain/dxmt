#include "dxmt_names.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_texture.hpp"
#include "d3d11_device.hpp"

namespace dxmt {

template <>
void initWithSubresourceData(MTL::Texture *target,
                             const D3D11_TEXTURE3D_DESC1 *desc,
                             const D3D11_SUBRESOURCE_DATA *subresources) {
  if (subresources == nullptr)
    return;
  uint32_t width = desc->Width;
  uint32_t height = desc->Height;
  uint32_t depth = desc->Depth;
  for (uint32_t level = 0; level < desc->MipLevels; level++) {
    auto region = MTL::Region::Make3D(0, 0, 0, width, height, depth);
    target->replaceRegion(region, level, 0, subresources[level].pSysMem,
                          subresources[level].SysMemPitch,
                          subresources[level].SysMemSlicePitch);
    width = std::max(1u, width >> 1);
    height = std::max(1u, height >> 1);
    depth = std::max(1u, depth >> 1);
  }
};

template <>
void initWithSubresourceData(MTL::Texture *target,
                             const D3D11_TEXTURE2D_DESC1 *desc,
                             const D3D11_SUBRESOURCE_DATA *subresources) {
  if (subresources == nullptr)
    return;
  uint32_t width = desc->Width;
  uint32_t height = desc->Height;
  for (uint32_t level = 0; level < desc->MipLevels; level++) {
    for (uint32_t slice = 0; slice < desc->ArraySize; slice++) {
      auto idx = D3D11CalcSubresource(level, slice, desc->MipLevels);
      auto region = MTL::Region::Make2D(0, 0, width, height);
      target->replaceRegion(region, level, slice, subresources[idx].pSysMem,
                            subresources[idx].SysMemPitch, 0);
    }
    width = std::max(1u, width >> 1);
    height = std::max(1u, height >> 1);
  }
};

template <>
void initWithSubresourceData(MTL::Texture *target,
                             const D3D11_TEXTURE1D_DESC *desc,
                             const D3D11_SUBRESOURCE_DATA *subresources) {
  if (subresources == nullptr)
    return;
  uint32_t width = desc->Width;
  for (uint32_t level = 0; level < desc->MipLevels; level++) {
    for (uint32_t slice = 0; slice < desc->ArraySize; slice++) {
      auto idx = D3D11CalcSubresource(level, slice, desc->MipLevels);
      auto region = MTL::Region::Make1D(0, width);
      target->replaceRegion(region, level, slice, subresources[idx].pSysMem, 0,
                            0);
    }
    width = std::max(1u, width >> 1);
  }
};

} // namespace dxmt