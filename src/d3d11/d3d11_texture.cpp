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

Obj<MTL::TextureDescriptor>
getTextureDescriptor(IMTLDXGIAdatper *pAdapter,
                     D3D11_RESOURCE_DIMENSION Dimension, UINT Width,
                     UINT Height, UINT Depth, UINT ArraySize, UINT SampleCount,
                     UINT BindFlags, UINT CPUAccessFlags, UINT MiscFlags,
                     D3D11_USAGE Usage, UINT MipLevels, DXGI_FORMAT Format) {
  auto desc = transfer(MTL::TextureDescriptor::alloc()->init());

  MTL_FORMAT_DESC metal_format;

  if (FAILED(pAdapter->QueryFormatDesc(Format, &metal_format))) {
    ERR("getTextureDescriptor: creating a texture of invalid format: ", Format);
    return nullptr;
  }
  desc->setPixelFormat(metal_format.PixelFormat);

  MTL::TextureUsage metal_usage = 0; // actually corresponding to BindFlags

  if (BindFlags & (D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_VERTEX_BUFFER |
                   D3D11_BIND_INDEX_BUFFER | D3D11_BIND_STREAM_OUTPUT)) {
    ERR("getTextureDescriptor: invalid bind flags");
    return nullptr;
  } else {
    if (BindFlags & D3D11_BIND_SHADER_RESOURCE)
      metal_usage |= MTL::TextureUsageShaderRead;
    if (BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL))
      metal_usage |= MTL::TextureUsageRenderTarget;
    if (BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      metal_usage |= MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite;
    // decoder not supported: D3D11_BIND_DECODER, D3D11_BIND_VIDEO_ENCODER
  }
  desc->setUsage(metal_usage);

  MTL::ResourceOptions options = 0;
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

  if (MipLevels == 0) {
    desc->setMipmapLevelCount(32 - __builtin_clz(Width | Height));
  } else {
    desc->setMipmapLevelCount(MipLevels);
  }

  switch (Dimension) {
  default: {
    ERR("getTextureDescriptor: invalid texture dimension");
    return nullptr; // nonsense
  }
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
getTextureDescriptor(IMTLD3D11Device *pDevice,
                     const D3D11_TEXTURE1D_DESC *desc) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  return getTextureDescriptor(
      adapter.ptr(), D3D11_RESOURCE_DIMENSION_TEXTURE2D, desc->Width, 1, 1,
      desc->ArraySize, 0, desc->BindFlags, desc->CPUAccessFlags,
      desc->MiscFlags, desc->Usage, desc->MipLevels, desc->Format);
}

template <>
Obj<MTL::TextureDescriptor>
getTextureDescriptor(IMTLD3D11Device *pDevice,
                     const D3D11_TEXTURE2D_DESC *desc) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  return getTextureDescriptor(adapter.ptr(), D3D11_RESOURCE_DIMENSION_TEXTURE2D,
                              desc->Width, desc->Height, 1, desc->ArraySize,
                              desc->SampleDesc.Count, desc->BindFlags,
                              desc->CPUAccessFlags, desc->MiscFlags,
                              desc->Usage, desc->MipLevels, desc->Format);
}

template <>
Obj<MTL::TextureDescriptor>
getTextureDescriptor(IMTLD3D11Device *pDevice,
                     const D3D11_TEXTURE3D_DESC *desc) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  return getTextureDescriptor(
      adapter.ptr(), D3D11_RESOURCE_DIMENSION_TEXTURE2D, desc->Width,
      desc->Height, desc->Depth, 1, 1, desc->BindFlags, desc->CPUAccessFlags,
      desc->MiscFlags, desc->Usage, desc->MipLevels, desc->Format);
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE1D_DESC,
                                 D3D11_DEPTH_STENCIL_VIEW_DESC>(
    const D3D11_TEXTURE1D_DESC *pResourceDesc,
    const D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescIn,
    D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    return;
  }
  pViewDescOut->Format = pResourceDesc->Format;

  if (pResourceDesc->ArraySize == 1) {
    pViewDescOut->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
    pViewDescOut->Texture1D.MipSlice = 0;
  } else {
    pViewDescOut->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
    pViewDescOut->Texture1DArray.MipSlice = 0;
    pViewDescOut->Texture1DArray.FirstArraySlice = 0;
    pViewDescOut->Texture1DArray.ArraySize = pResourceDesc->ArraySize;
  }
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE3D_DESC,
                                 D3D11_DEPTH_STENCIL_VIEW_DESC>(
    const D3D11_TEXTURE3D_DESC *pResourceDesc,
    const D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescIn,
    D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescOut) {
  assert(0 && "TODO: proper error handling");
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE1D_DESC,
                                 D3D11_SHADER_RESOURCE_VIEW_DESC>(
    const D3D11_TEXTURE1D_DESC *pResourceDesc,
    const D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescIn,
    D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    return;
  }
  pViewDescOut->Format = pResourceDesc->Format;

  if (pResourceDesc->ArraySize == 1) {
    pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    pViewDescOut->Texture1D.MostDetailedMip = 0;
    pViewDescOut->Texture1D.MipLevels = pResourceDesc->MipLevels;
  } else {
    pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
    pViewDescOut->Texture1DArray.MostDetailedMip = 0;
    pViewDescOut->Texture1DArray.MipLevels = pResourceDesc->MipLevels;
    pViewDescOut->Texture1DArray.FirstArraySlice = 0;
    pViewDescOut->Texture1DArray.ArraySize = pResourceDesc->ArraySize;
  }
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE2D_DESC,
                                 D3D11_SHADER_RESOURCE_VIEW_DESC>(
    const D3D11_TEXTURE2D_DESC *pResourceDesc,
    const D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescIn,
    D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    return;
  }
  pViewDescOut->Format = pResourceDesc->Format;

  if (pResourceDesc->SampleDesc.Count == 1) {
    if (pResourceDesc->ArraySize == 1) {
      pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      pViewDescOut->Texture2D.MostDetailedMip = 0;
      pViewDescOut->Texture2D.MipLevels = pResourceDesc->MipLevels;
      // pViewDescOut->Texture2D.PlaneSlice      = 0;
    } else {
      pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
      pViewDescOut->Texture2DArray.MostDetailedMip = 0;
      pViewDescOut->Texture2DArray.MipLevels = pResourceDesc->MipLevels;
      pViewDescOut->Texture2DArray.FirstArraySlice = 0;
      pViewDescOut->Texture2DArray.ArraySize = pResourceDesc->ArraySize;
      // pViewDescOut->Texture2DArray.PlaneSlice      = 0;
    }
  } else {
    if (pResourceDesc->ArraySize == 1) {
      pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
    } else {
      pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
      pViewDescOut->Texture2DMSArray.FirstArraySlice = 0;
      pViewDescOut->Texture2DMSArray.ArraySize = pResourceDesc->ArraySize;
    }
  }
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE3D_DESC,
                                 D3D11_SHADER_RESOURCE_VIEW_DESC>(
    const D3D11_TEXTURE3D_DESC *pResourceDesc,
    const D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescIn,
    D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    return;
  }
  pViewDescOut->Format = pResourceDesc->Format;

  pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
  pViewDescOut->Texture3D.MostDetailedMip = 0;
  pViewDescOut->Texture3D.MipLevels = pResourceDesc->MipLevels;
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE2D_DESC,
                                 D3D11_DEPTH_STENCIL_VIEW_DESC>(
    const D3D11_TEXTURE2D_DESC *pResourceDesc,
    const D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescIn,
    D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    return;
  }
  pViewDescOut->Format = pResourceDesc->Format;
  if (pResourceDesc->SampleDesc.Count == 1) {
    if (pResourceDesc->ArraySize == 1) {
      pViewDescOut->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
      pViewDescOut->Texture2D.MipSlice = 0;
    } else {
      pViewDescOut->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
      pViewDescOut->Texture2DArray.MipSlice = 0;
      pViewDescOut->Texture2DArray.FirstArraySlice = 0;
      pViewDescOut->Texture2DArray.ArraySize = pResourceDesc->ArraySize;
    }
  } else {
    if (pResourceDesc->ArraySize == 1) {
      pViewDescOut->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
    } else {
      pViewDescOut->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
      pViewDescOut->Texture2DMSArray.FirstArraySlice = 0;
      pViewDescOut->Texture2DMSArray.ArraySize = pResourceDesc->ArraySize;
    }
  }
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE2D_DESC,
                                 D3D11_RENDER_TARGET_VIEW_DESC>(
    const D3D11_TEXTURE2D_DESC *pResourceDesc,
    const D3D11_RENDER_TARGET_VIEW_DESC *pViewDescIn,
    D3D11_RENDER_TARGET_VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    return;
  }
  pViewDescOut->Format = pResourceDesc->Format;
  if (pResourceDesc->SampleDesc.Count == 1) {
    if (pResourceDesc->ArraySize == 1) {
      pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      pViewDescOut->Texture2D.MipSlice = 0;
      // pViewDescOut->Texture2D.PlaneSlice = 0;
    } else {
      pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
      pViewDescOut->Texture2DArray.MipSlice = 0;
      pViewDescOut->Texture2DArray.FirstArraySlice = 0;
      pViewDescOut->Texture2DArray.ArraySize = pResourceDesc->ArraySize;
      // pViewDescOut->Texture2DArray.PlaneSlice = 0;
    }
  } else {
    if (pResourceDesc->ArraySize == 1) {
      pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
    } else {
      pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
      pViewDescOut->Texture2DMSArray.FirstArraySlice = 0;
      pViewDescOut->Texture2DMSArray.ArraySize = pResourceDesc->ArraySize;
    }
  }
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE1D_DESC,
                                 D3D11_RENDER_TARGET_VIEW_DESC>(
    const D3D11_TEXTURE1D_DESC *pResourceDesc,
    const D3D11_RENDER_TARGET_VIEW_DESC *pViewDescIn,
    D3D11_RENDER_TARGET_VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    return;
  }
  pViewDescOut->Format = pResourceDesc->Format;
  if (pResourceDesc->ArraySize == 1) {
    pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
    pViewDescOut->Texture1D.MipSlice = 0;
  } else {
    pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
    pViewDescOut->Texture1DArray.MipSlice = 0;
    pViewDescOut->Texture1DArray.FirstArraySlice = 0;
    pViewDescOut->Texture1DArray.ArraySize = pResourceDesc->ArraySize;
  }
}

template <>
void getViewDescFromResourceDesc<D3D11_TEXTURE3D_DESC,
                                 D3D11_RENDER_TARGET_VIEW_DESC>(
    const D3D11_TEXTURE3D_DESC *pResourceDesc,
    const D3D11_RENDER_TARGET_VIEW_DESC *pViewDescIn,
    D3D11_RENDER_TARGET_VIEW_DESC *pViewDescOut) {
  if (pViewDescIn) {
    *pViewDescOut = *pViewDescIn;
    return;
  }
  pViewDescOut->Format = pResourceDesc->Format;
  pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
  pViewDescOut->Texture3D.MipSlice = 0;
  pViewDescOut->Texture3D.FirstWSlice = 0;
  pViewDescOut->Texture3D.WSize = pResourceDesc->Depth;
}

} // namespace dxmt