#include "Foundation/NSRange.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_texture.hpp"

namespace dxmt {

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_ARRAY_DSV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType1D, NS::Range::Make(s->MipSlice, 1),
      NS::Range::Make(s->FirstArraySlice, s->ArraySize));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_ARRAY_RTV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType1D, NS::Range::Make(s->MipSlice, 1),
      NS::Range::Make(s->FirstArraySlice, s->ArraySize));
}

MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_ARRAY_UAV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType1DArray, NS::Range::Make(s->MipSlice, 1),
      NS::Range::Make(s->FirstArraySlice, s->ArraySize));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_SRV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType1D,
      NS::Range::Make(s->MostDetailedMip, s->MipLevels == 0xffffffffu
                                              ? source->mipmapLevelCount()
                                              : s->MipLevels),
      NS::Range::Make(0, 1));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX2D_SRV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType2D,
      NS::Range::Make(s->MostDetailedMip, s->MipLevels == 0xffffffffu
                                              ? source->mipmapLevelCount()
                                              : s->MipLevels),
      NS::Range::Make(0, 1));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX2D_DSV *s) {
  if (source->mipmapLevelCount() == 1) {
    source->retain();
    return source;
  }
  return source->newTextureView(newFormat, MTL::TextureType2D,
                                NS::Range::Make(s->MipSlice, 1),
                                NS::Range::Make(0, source->arrayLength()));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_ARRAY_SRV *s) {
  if (source->textureType() == MTL::TextureType1DArray &&
      s->MostDetailedMip == 0 &&
      (s->MipLevels == 0xffffffffu ||
       s->MipLevels == source->mipmapLevelCount()) &&
      s->FirstArraySlice == 0 && s->ArraySize == source->arrayLength()) {
    return source->retain();
  }
  throw "Not viable transition of texture view";
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX2D_ARRAY_SRV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType2D,
      NS::Range::Make(s->MostDetailedMip, s->MipLevels == 0xffffffffu
                                              ? source->mipmapLevelCount()
                                              : s->MipLevels),
      NS::Range::Make(s->FirstArraySlice, s->ArraySize));
}

template <>
void initWithSubresourceData(MTL::Texture *target,
                             const D3D11_TEXTURE3D_DESC *desc,
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
                             const D3D11_TEXTURE2D_DESC *desc,
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