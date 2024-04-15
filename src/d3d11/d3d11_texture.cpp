#include "Foundation/NSRange.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_texture.hpp"
#include "dxgi_format.hpp"
#include "log/log.hpp"

namespace dxmt {

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

Obj<MTL::TextureDescriptor>
getTextureDescriptor(D3D11_RESOURCE_DIMENSION Dimension, UINT Width,
                     UINT Height, UINT Depth, UINT ArraySize, UINT SampleCount,
                     UINT BindFlags, UINT CPUAccessFlags, UINT MiscFlags,
                     D3D11_USAGE Usage, UINT MipLevels, DXGI_FORMAT FORMAT) {
  auto desc = transfer(MTL::TextureDescriptor::alloc()->init());

  auto format = g_metal_format_map[FORMAT];
  WARN("DXGI FORMAT:", FORMAT, ",METAL FORMAT:", format.pixel_format);
  desc->setPixelFormat(format.pixel_format);

  MTL::TextureUsage metal_usage = 0; // actually corresponding to BindFlags

  if (BindFlags & (D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_VERTEX_BUFFER |
                   D3D11_BIND_INDEX_BUFFER | D3D11_BIND_STREAM_OUTPUT)) {
    return nullptr;
  } else {
    if (BindFlags & D3D11_BIND_SHADER_RESOURCE)
      metal_usage |= MTL::TextureUsageShaderRead;
    if (BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL))
      metal_usage |= MTL::TextureUsageRenderTarget;
    if (BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      metal_usage |= MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite;
    // decoder not supported: D3D11_BIND_DECODER, D3D11_BIND_VIDEO_ENCODER

    // TODO: if format is TYPELESS
  }
  desc->setUsage(metal_usage);

  MTL::ResourceOptions options = 0; // actually corresponding to Usage ...
  switch (Usage) {
  case D3D11_USAGE_DEFAULT:
    options |= MTL::ResourceStorageModeManaged;
    break;
  case D3D11_USAGE_IMMUTABLE:
    options |= MTL::ResourceStorageModeManaged | // FIXME: switch to
                                                 // ResourceStorageModePrivate
               MTL::ResourceHazardTrackingModeUntracked;
    break;
  case D3D11_USAGE_DYNAMIC:
    options |=
        MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined;
    break;
  case D3D11_USAGE_STAGING:
    options |= MTL::ResourceStorageModeShared;
    break;
  }

  desc->setResourceOptions(options);

  desc->setMipmapLevelCount(MipLevels);

  switch (Dimension) {
  default:
    return nullptr; // nonsense
  case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    if (ArraySize > 1) {
      desc->setTextureType(MTL::TextureType1DArray);
    } else {
      desc->setTextureType(MTL::TextureType1D);
    }
    break;
  case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    if (MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) {
      if (ArraySize > 1) {
        desc->setTextureType(MTL::TextureTypeCubeArray);
      } else {
        desc->setTextureType(MTL::TextureTypeCube);
      }
    } else {
      if (SampleCount > 1) {
        if (ArraySize > 1) {
          desc->setTextureType(MTL::TextureType2DMultisampleArray);
        } else {
          desc->setTextureType(MTL::TextureType2DMultisample);
        }
        desc->setSampleCount(SampleCount);
      } else {
        if (ArraySize > 1) {
          desc->setTextureType(MTL::TextureType2DArray);
        } else {
          desc->setTextureType(MTL::TextureType2D);
        }
      }
    }
    break;
  case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
    desc->setTextureType(MTL::TextureType3D);
    break;
  }
  desc->setWidth(Width);
  desc->setHeight(Height);
  desc->setDepth(Depth);

  return desc;
};

template <>
Obj<MTL::TextureDescriptor>
getTextureDescriptor(const D3D11_TEXTURE1D_DESC *desc) {
  return getTextureDescriptor(D3D11_RESOURCE_DIMENSION_TEXTURE2D, desc->Width,
                              1, 1, desc->ArraySize, 0, desc->BindFlags,
                              desc->CPUAccessFlags, desc->MiscFlags,
                              desc->Usage, desc->MipLevels, desc->Format);
}

template <>
Obj<MTL::TextureDescriptor>
getTextureDescriptor(const D3D11_TEXTURE2D_DESC *desc) {
  return getTextureDescriptor(D3D11_RESOURCE_DIMENSION_TEXTURE2D, desc->Width,
                              desc->Height, 1, desc->ArraySize,
                              desc->SampleDesc.Count, desc->BindFlags,
                              desc->CPUAccessFlags, desc->MiscFlags,
                              desc->Usage, desc->MipLevels, desc->Format);
}

template <>
Obj<MTL::TextureDescriptor>
getTextureDescriptor(const D3D11_TEXTURE3D_DESC *desc) {
  return getTextureDescriptor(D3D11_RESOURCE_DIMENSION_TEXTURE2D, desc->Width,
                              desc->Height, desc->Depth, 1, 1, desc->BindFlags,
                              desc->CPUAccessFlags, desc->MiscFlags,
                              desc->Usage, desc->MipLevels, desc->Format);
}

} // namespace dxmt