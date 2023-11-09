#pragma once
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLResource.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_view.hpp"
#include "mtld11_resource.hpp"
#include "d3d11_device_child.h"
#include "d3d11_private.h"
#include "d3d11_resource.hpp"
#include <ostream>
#include <winerror.h>
#include <winnt.h>

namespace dxmt {

template <typename SRV_D>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const SRV_D *s);

template <typename TEXTURE_DESC>
void initWithSubresourceData(MTL::Texture *target, const TEXTURE_DESC *desc,
                             const D3D11_SUBRESOURCE_DATA *subresources);

inline Obj<MTL::TextureDescriptor> TranslateTextureDescriptor(
    D3D11_RESOURCE_DIMENSION Dimension, UINT Width, UINT Height, UINT Depth,
    UINT ArraySize, UINT SampleCount, UINT BindFlags, UINT CPUAccessFlags,
    UINT MiscFlags, D3D11_USAGE Usage, UINT MipLevels, DXGI_FORMAT FORMAT) {
  auto desc = transfer(MTL::TextureDescriptor::alloc()->init());

  auto format = g_metal_format_map[FORMAT];
  WARN("DXGI FORMAT:", FORMAT, ",METAL FORMAT:", format.pixel_format);
  str ::format(MTL::PixelFormatRGBA32Uint);
  desc->setPixelFormat(format.pixel_format);

  MTL::TextureUsage metalUsage = 0; // actually corresponding to BindFlags

  if (BindFlags & (D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_VERTEX_BUFFER |
                   D3D11_BIND_INDEX_BUFFER | D3D11_BIND_STREAM_OUTPUT)) {

    // doesn't make sense here.
    // I know the spec does allow a texture created with
    // D3D11_BIND_CONSTANT_BUFFER but who the hell is gonna use in this way
    return nullptr;
  } else {
    if (BindFlags & D3D11_BIND_SHADER_RESOURCE)
      metalUsage |= MTL::TextureUsageShaderRead;
    if (BindFlags & (D3D11_BIND_RENDER_TARGET | D3D11_BIND_DEPTH_STENCIL))
      metalUsage |= MTL::TextureUsageRenderTarget;
    if (BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      metalUsage |= MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite;
    // decoder not supported: D3D11_BIND_DECODER, D3D11_BIND_VIDEO_ENCODER

    // TODO: if format is TYPELESS
  }
  desc->setUsage(metalUsage);

  MTL::ResourceOptions options = 0; // actually corresponding to Usage ...
  switch (Usage) {
  case D3D11_USAGE_DEFAULT:
    options |= MTL::ResourceStorageModePrivate;
    break;
  case D3D11_USAGE_IMMUTABLE:
    options |= MTL::ResourceStorageModePrivate |
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

inline Obj<MTL::TextureDescriptor>
TranslateTextureDescriptor(const D3D11_TEXTURE2D_DESC *desc) {
  return TranslateTextureDescriptor(
      D3D11_RESOURCE_DIMENSION_TEXTURE2D, desc->Width, desc->Height, 1,
      desc->ArraySize, desc->SampleDesc.Count, desc->BindFlags,
      desc->CPUAccessFlags, desc->MiscFlags, desc->Usage, desc->MipLevels,
      desc->Format);
}

class StagingTexture2D {
public:
  static const D3D11_RESOURCE_DIMENSION Dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  static const D3D11_USAGE Usage = D3D11_USAGE_STAGING;
  typedef D3D11_TEXTURE2D_DESC Description;
  typedef ID3D11Texture2D Interface;
  typedef void *Data;

  StagingTexture2D(IMTLD3D11Device *pDevice, Data, const Description *desc,
                   const D3D11_SUBRESOURCE_DATA *pInit);

  Obj<MTL::Texture> staging_texture_;
};

using InitializeView = std::function<MTL::Texture *(DXMTCommandStream *)>;

class ImmutableTexture2D {
public:
  static const D3D11_RESOURCE_DIMENSION Dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  static const D3D11_USAGE Usage = D3D11_USAGE_IMMUTABLE;
  typedef D3D11_TEXTURE2D_DESC Description;
  typedef ID3D11Texture2D Interface;
  typedef void *Data;

  IMTLD3D11Device *__device__;
  Interface *__resource__;

  ImmutableTexture2D(IMTLD3D11Device *pDevice, Data data,
                     const Description *desc,
                     const D3D11_SUBRESOURCE_DATA *pInit);

  Obj<MTL::Texture> texture_;
  Obj<MTL::Texture> staging_;
  bool initailzed = false;

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
                                   ID3D11ShaderResourceView **ppView);

  void EnsureInitialized(DXMTCommandStream *cs) {
    if (!initailzed) {
      cs->EmitPreCommand(
          MTLInitializeTextureCommand(std::move(staging_), texture_.ptr()));
      initailzed = true;
    }
  }
};

class DefaultTexture2D {
public:
  static const D3D11_RESOURCE_DIMENSION Dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  static const D3D11_USAGE Usage = D3D11_USAGE_DEFAULT;
  typedef D3D11_TEXTURE2D_DESC Description;
  typedef ID3D11Texture2D Interface;
  typedef void *Data;

  IMTLD3D11Device *__device__;
  Interface *__resource__;

  DefaultTexture2D(IMTLD3D11Device *pDevice, Data data, const Description *desc,
                   const D3D11_SUBRESOURCE_DATA *pInit);

  Obj<MTL::Texture> texture_;
  Obj<MTL::Texture> staging_;
  bool initailzed = false;

  // HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC
  // *desc,
  //                                  ID3D11ShaderResourceView **ppView);

  HRESULT CreateDepthStencilView(const D3D11_DEPTH_STENCIL_VIEW_DESC *desc,
                                 ID3D11DepthStencilView **ppView);

  void EnsureInitialized(DXMTCommandStream *cs) {
    if (!initailzed) {
      cs->EmitPreCommand(
          MTLInitializeTextureCommand(std::move(staging_), texture_.ptr()));
      initailzed = true;
    }
  }
};

} // namespace dxmt