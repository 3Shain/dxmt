#include "dxmt_names.hpp"
#include "Foundation/NSRange.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_texture.hpp"
#include "d3d11_device.hpp"
#include "d3d11_private.h"
#include "dxgi_interfaces.h"

namespace dxmt {

template <typename VIEW_DESC>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const VIEW_DESC *s);

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_SRV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType1D,
      NS::Range::Make(s->MostDetailedMip,
                      s->MipLevels == 0xffffffffu
                          ? source->mipmapLevelCount() - s->MostDetailedMip
                          : s->MipLevels),
      NS::Range::Make(0, 1));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_RTV *s) {
  return source->newTextureView(newFormat, MTL::TextureType1D,
                                NS::Range::Make(s->MipSlice, 1),
                                NS::Range::Make(0, 1));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_ARRAY_DSV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType1DArray, NS::Range::Make(s->MipSlice, 1),
      NS::Range::Make(s->FirstArraySlice, s->ArraySize));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX1D_ARRAY_RTV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType1DArray, NS::Range::Make(s->MipSlice, 1),
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
                             const D3D11_TEX2D_SRV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType2D,
      NS::Range::Make(s->MostDetailedMip,
                      s->MipLevels == 0xffffffffu
                          ? source->mipmapLevelCount() - s->MostDetailedMip
                          : s->MipLevels),
      NS::Range::Make(0, 1));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX2D_RTV *s) {
  if (source->mipmapLevelCount() == 1) {
    source->retain();
    return source;
  }
  return source->newTextureView(newFormat, MTL::TextureType2D,
                                NS::Range::Make(s->MipSlice, 1),
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
                                NS::Range::Make(0, 1));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEX2D_ARRAY_SRV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType2D,
      NS::Range::Make(s->MostDetailedMip,
                      s->MipLevels == 0xffffffffu
                          ? source->mipmapLevelCount() - s->MostDetailedMip
                          : s->MipLevels),
      NS::Range::Make(s->FirstArraySlice, s->ArraySize));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_TEXCUBE_SRV *s) {
  return source->newTextureView(
      newFormat, MTL::TextureType2D,
      NS::Range::Make(s->MostDetailedMip,
                      s->MipLevels == 0xffffffffu
                          ? source->mipmapLevelCount() - s->MostDetailedMip
                          : s->MipLevels),
      NS::Range::Make(0, 1));
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_DEPTH_STENCIL_VIEW_DESC *s) {
  switch (s->ViewDimension) {
  case D3D11_DSV_DIMENSION_UNKNOWN: {
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE1D: {
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE1DARRAY: {
    return newTextureView(source, newFormat, &s->Texture1DArray);
  }
  case D3D11_DSV_DIMENSION_TEXTURE2D: {
    return newTextureView(source, newFormat, &s->Texture2D);
  }
  case D3D11_DSV_DIMENSION_TEXTURE2DARRAY: {
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE2DMS: {
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY: {
    break;
  }
  }
  ERR("dsv ", s->ViewDimension);
  IMPLEMENT_ME
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_RENDER_TARGET_VIEW_DESC *s) {
  switch (s->ViewDimension) {

  case D3D11_RTV_DIMENSION_UNKNOWN: {
    break;
  }
  case D3D11_RTV_DIMENSION_BUFFER: {
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE1D: {
    return newTextureView(source, newFormat, &s->Texture1D);
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: {
    return newTextureView(source, newFormat, &s->Texture1DArray);
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2D: {
    return newTextureView(source, newFormat, &s->Texture2D);
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: {
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DMS: {
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: {
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE3D: {
    break;
  }
  }
  ERR("rtv ", s->ViewDimension);
  IMPLEMENT_ME
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_UNORDERED_ACCESS_VIEW_DESC *s) {
  switch (s->ViewDimension) {
  case D3D11_UAV_DIMENSION_UNKNOWN: {
    break;
  }
  case D3D11_UAV_DIMENSION_BUFFER: {
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE1D: {
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: {
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2D: {
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: {
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE3D: {
    break;
  }
  }
  ERR("uav ", s->ViewDimension);
  IMPLEMENT_ME;
}

template <>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const D3D11_SHADER_RESOURCE_VIEW_DESC *s) {
  switch (s->ViewDimension) {
  case D3D_SRV_DIMENSION_UNKNOWN: {
    break;
  }
  case D3D_SRV_DIMENSION_BUFFER: {
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE1D: {
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE1DARRAY: {
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2D: {
    return newTextureView(source, newFormat, &s->Texture2D);
  }
  case D3D_SRV_DIMENSION_TEXTURE2DARRAY: {
    return newTextureView(source, newFormat, &s->Texture2DArray);
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMS: {
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY: {
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE3D: {
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURECUBE: {
    return newTextureView(source, newFormat, &s->TextureCube);
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURECUBEARRAY: {
    break;
  }
  case D3D_SRV_DIMENSION_BUFFEREX: {
    break;
  }
  }
  ERR("srv ", s->ViewDimension);
  IMPLEMENT_ME;
}

template <typename VIEW_DESC>
MTL::Texture *newTextureView(IMTLD3D11Device *pDevice, MTL::Texture *source,
                             const VIEW_DESC *pDesc) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pDesc->Format, &metal_format))) {
    assert(0 && "TODO: handle invalid texture view format");
  }
  return newTextureView(source, metal_format.PixelFormat, pDesc);
};

#define NEW_TEXTURE_VIEW_INSTANTIATE(TYPE)                                     \
  template MTL::Texture *newTextureView<TYPE>(                                 \
      IMTLD3D11Device * pDevice, MTL::Texture * source, const TYPE *s);
NEW_TEXTURE_VIEW_INSTANTIATE(D3D11_DEPTH_STENCIL_VIEW_DESC);
NEW_TEXTURE_VIEW_INSTANTIATE(D3D11_RENDER_TARGET_VIEW_DESC);
NEW_TEXTURE_VIEW_INSTANTIATE(D3D11_SHADER_RESOURCE_VIEW_DESC);
NEW_TEXTURE_VIEW_INSTANTIATE(D3D11_UNORDERED_ACCESS_VIEW_DESC);

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