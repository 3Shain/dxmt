#include "dxmt_format.hpp"
#include "mtld11_resource.hpp"
#include "util_math.hpp"

namespace dxmt {

void FixDepthStencilFormat(MTL::Texture *pTexture, MTL_DXGI_FORMAT_DESC &Desc) {
  switch (pTexture->pixelFormat()) {
  case MTL::PixelFormatDepth16Unorm:
    // DXGI_FORMAT_R16_TYPELESS
    // DXGI_FORMAT_R16_UNORM
    Desc.PixelFormat = MTL::PixelFormatDepth16Unorm;
    break;
  case MTL::PixelFormatDepth32Float:
    // DXGI_FORMAT_R32_TYPELESS
    // DXGI_FORMAT_R32_FLOAT
    Desc.PixelFormat = MTL::PixelFormatDepth32Float;
    break;
  default:
    break;
  }
}

template <>
HRESULT CreateMTLTextureView<D3D11_SHADER_RESOURCE_VIEW_DESC1>(
    IMTLD3D11Device *pDevice, MTL::Texture *pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pViewDesc, MTL::Texture **ppView) {
  MTL_DXGI_FORMAT_DESC metal_format;
  if (FAILED(MTLQueryDXGIFormat(pDevice->GetMTLDevice(), pViewDesc->Format,
                                metal_format))) {
    ERR("Failed to create SRV due to unsupported format ", pViewDesc->Format);
    return E_FAIL;
  }
  FixDepthStencilFormat(pResource, metal_format);
  auto texture_type = pResource->textureType();
  MTL::TextureSwizzleChannels swizzle = {
      MTL::TextureSwizzleRed, MTL::TextureSwizzleGreen, MTL::TextureSwizzleBlue,
      MTL::TextureSwizzleAlpha};
  switch (metal_format.PixelFormat) {
  case MTL::PixelFormatDepth24Unorm_Stencil8:
  case MTL::PixelFormatDepth32Float_Stencil8:
  case MTL::PixelFormatDepth16Unorm:
  case MTL::PixelFormatDepth32Float:
  case MTL::PixelFormatStencil8:
    swizzle = {MTL::TextureSwizzleRed, MTL::TextureSwizzleZero,
               MTL::TextureSwizzleZero, MTL::TextureSwizzleOne};
    break;
  case MTL::PixelFormatX24_Stencil8:
  case MTL::PixelFormatX32_Stencil8:
    swizzle = {MTL::TextureSwizzleZero, MTL::TextureSwizzleRed,
               MTL::TextureSwizzleZero, MTL::TextureSwizzleOne};
    break;
  default:
    break;
  }
  switch (pViewDesc->ViewDimension) {
  default:
    break;
  case D3D_SRV_DIMENSION_TEXTURE1D: {
    if (texture_type == MTL::TextureType1D) {
      // pViewDesc->Texture1D.MipLevels;
    }
    if (texture_type == MTL::TextureType1DArray) {
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE1DARRAY: {
    if (texture_type == MTL::TextureType1D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType1DArray,
          NS::Range::Make(pViewDesc->Texture1DArray.MostDetailedMip,
                          pViewDesc->Texture1DArray.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture1DArray.MostDetailedMip
                              : pViewDesc->Texture1DArray.MipLevels),
          NS::Range::Make(0, 1), swizzle);
      return S_OK;
    }
    if (texture_type == MTL::TextureType1DArray) {
      auto array_size = pViewDesc->Texture1DArray.ArraySize == 0xffffffff
                            ? pResource->arrayLength() -
                                  pViewDesc->Texture1DArray.FirstArraySlice
                            : pViewDesc->Texture1DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType1DArray,
          NS::Range::Make(pViewDesc->Texture1DArray.MostDetailedMip,
                          pViewDesc->Texture1DArray.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture1DArray.MostDetailedMip
                              : pViewDesc->Texture1DArray.MipLevels),
          NS::Range::Make(pViewDesc->Texture1DArray.FirstArraySlice,
                          array_size),
          swizzle);
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2D: {
    if (texture_type == MTL::TextureType2D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2D,
          NS::Range::Make(pViewDesc->Texture2D.MostDetailedMip,
                          pViewDesc->Texture2D.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture2D.MostDetailedMip
                              : pViewDesc->Texture2D.MipLevels),
          NS::Range::Make(0, 1), swizzle);
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DARRAY: {
    if (texture_type == MTL::TextureType2D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2D.MostDetailedMip,
                          pViewDesc->Texture2D.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture2D.MostDetailedMip
                              : pViewDesc->Texture2D.MipLevels),
          NS::Range::Make(0, 1), swizzle);
      return S_OK;
    }
    if (texture_type == MTL::TextureType2DArray) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? pResource->arrayLength() -
                                  pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MostDetailedMip,
                          pViewDesc->Texture2DArray.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture2DArray.MostDetailedMip
                              : pViewDesc->Texture2DArray.MipLevels),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size),
          swizzle);
      return S_OK;
    }
    if (texture_type == MTL::TextureTypeCube) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? 6 - pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MostDetailedMip,
                          pViewDesc->Texture2DArray.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture2DArray.MostDetailedMip
                              : pViewDesc->Texture2DArray.MipLevels),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size),
          swizzle);
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMS: {
    if (texture_type == MTL::TextureType2DMultisample) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DMultisample,
          NS::Range::Make(0, 1), NS::Range::Make(0, 1), swizzle);
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY: {
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE3D: {
    if (texture_type == MTL::TextureType3D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType3D,
          NS::Range::Make(pViewDesc->Texture3D.MostDetailedMip,
                          pViewDesc->Texture3D.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture3D.MostDetailedMip
                              : pViewDesc->Texture3D.MipLevels),
          NS::Range::Make(0, 1), swizzle);
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURECUBE: {
    if (texture_type == MTL::TextureTypeCube) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureTypeCube,
          NS::Range::Make(pViewDesc->TextureCube.MostDetailedMip,
                          pViewDesc->TextureCube.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->TextureCube.MostDetailedMip
                              : pViewDesc->TextureCube.MipLevels),
          NS::Range::Make(0, 6), swizzle);
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURECUBEARRAY: {
    if (texture_type == MTL::TextureTypeCube) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureTypeCubeArray,
          NS::Range::Make(pViewDesc->TextureCube.MostDetailedMip,
                          pViewDesc->TextureCube.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->TextureCube.MostDetailedMip
                              : pViewDesc->TextureCube.MipLevels),
          NS::Range::Make(0, 6), swizzle);
      return S_OK;
    }
    if (texture_type == MTL::TextureTypeCubeArray) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureTypeCubeArray,
          NS::Range::Make(pViewDesc->TextureCubeArray.MostDetailedMip,
                          pViewDesc->TextureCubeArray.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->TextureCubeArray.MostDetailedMip
                              : pViewDesc->TextureCube.MipLevels),
          NS::Range::Make(pViewDesc->TextureCubeArray.First2DArrayFace,
                          pViewDesc->TextureCubeArray.NumCubes * 6),
          swizzle);
      return S_OK;
    }
    break;
  }
  }
  ERR("Unhandled srv creation: \n Source: ", texture_type,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

template <>
HRESULT CreateMTLTextureView<D3D11_UNORDERED_ACCESS_VIEW_DESC1>(
    IMTLD3D11Device *pDevice, MTL::Texture *pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pViewDesc, MTL::Texture **ppView) {
  MTL_DXGI_FORMAT_DESC metal_format;
  if (FAILED(MTLQueryDXGIFormat(pDevice->GetMTLDevice(), pViewDesc->Format,
                                metal_format))) {
    return E_FAIL;
  }
  FixDepthStencilFormat(pResource, metal_format);
  auto texture_type = pResource->textureType();
  switch (pViewDesc->ViewDimension) {
  default:
    break;
  case D3D11_UAV_DIMENSION_TEXTURE1D: {
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: {
    if (texture_type == MTL::TextureType1DArray) {
      auto array_size = pViewDesc->Texture1DArray.ArraySize == 0xffffffff
                            ? pResource->arrayLength() -
                                  pViewDesc->Texture1DArray.FirstArraySlice
                            : pViewDesc->Texture1DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType1DArray,
          NS::Range::Make(pViewDesc->Texture1DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture1DArray.FirstArraySlice,
                          array_size));
      return S_OK;
    }
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2D: {
    if (texture_type == MTL::TextureType2D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2D,
          NS::Range::Make(pViewDesc->Texture2D.MipSlice, 1),
          NS::Range::Make(0, 1));
      return S_OK;
    }
    if (texture_type == MTL::TextureTypeCube) {
      D3D11_ASSERT(pViewDesc->Texture2D.PlaneSlice == 0);
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2D,
          NS::Range::Make(pViewDesc->Texture2D.MipSlice, 1),
          NS::Range::Make(0, 1));
      return S_OK;
    }
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: {
    if (texture_type == MTL::TextureType2D) {
      D3D11_ASSERT(pViewDesc->Texture2D.PlaneSlice == 0);
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(0, 1));
      return S_OK;
    }
    if (texture_type == MTL::TextureType2DArray) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? pResource->arrayLength() -
                                  pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      return S_OK;
    }
    if (texture_type == MTL::TextureTypeCube) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? 6 - pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      return S_OK;
    }
    if (texture_type == MTL::TextureTypeCubeArray) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? 6 * pResource->arrayLength() -
                                  pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      return S_OK;
    }
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE3D: {
    if (texture_type == MTL::TextureType3D) {
      if ((pViewDesc->Texture3D.WSize == pResource->depth() ||
           (pViewDesc->Texture3D.WSize == 0xffffffff)) &&
          pViewDesc->Texture3D.FirstWSlice == 0) {
        *ppView = pResource->newTextureView(
            metal_format.PixelFormat, MTL::TextureType3D,
            NS::Range::Make(pViewDesc->Texture3D.MipSlice, 1),
            NS::Range::Make(0, 1));
        return S_OK;
      }
      ERR("tex3d uav creation not properly handled: ",
          pViewDesc->Texture3D.FirstWSlice, ":", pViewDesc->Texture3D.WSize,
          ":", pResource->depth());
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType3D,
          NS::Range::Make(pViewDesc->Texture3D.MipSlice, 1),
          NS::Range::Make(0, 1));
      return S_OK;
    }
    break;
  }
  }
  ERR("Unhandled uav creation: \n Source: ", texture_type,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

HRESULT
CreateMTLRenderTargetView(IMTLD3D11Device *pDevice, MTL::Texture *pResource,
                          const D3D11_RENDER_TARGET_VIEW_DESC1 *pViewDesc,
                          MTL::Texture **ppView,
                          MTL_RENDER_PASS_ATTACHMENT_DESC &AttachmentDesc) {
  MTL_DXGI_FORMAT_DESC metal_format;
  if (FAILED(MTLQueryDXGIFormat(pDevice->GetMTLDevice(), pViewDesc->Format,
                                metal_format))) {
    return E_FAIL;
  }

  AttachmentDesc.Slice = 0; // use the original texture if format is the same?
  AttachmentDesc.Level = 0;
  AttachmentDesc.DepthPlane = 0;
  AttachmentDesc.RenderTargetArrayLength = 0;
  AttachmentDesc.SampleCount = pResource->sampleCount();

  auto texture_type = pResource->textureType();
  switch (pViewDesc->ViewDimension) {
  default:
    break;
  case D3D11_RTV_DIMENSION_TEXTURE1D: {
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE1DARRAY: {
    if (texture_type == MTL::TextureType1DArray) {
      auto array_size = pViewDesc->Texture1DArray.ArraySize == 0xffffffff
                            ? pResource->arrayLength() -
                                  pViewDesc->Texture1DArray.FirstArraySlice
                            : pViewDesc->Texture1DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType1DArray,
          NS::Range::Make(pViewDesc->Texture1DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture1DArray.FirstArraySlice,
                          array_size));
      AttachmentDesc.RenderTargetArrayLength = array_size;
      return S_OK;
    }
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2D: {
    if (texture_type == MTL::TextureType2D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2D,
          NS::Range::Make(pViewDesc->Texture2D.MipSlice, 1),
          NS::Range::Make(0, 1));
      return S_OK;
    }
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: {
    if (texture_type == MTL::TextureType2D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(0, 1));
      AttachmentDesc.RenderTargetArrayLength = 1;
      return S_OK;
    }
    if (texture_type == MTL::TextureType2DArray) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? pResource->arrayLength() -
                                  pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      AttachmentDesc.RenderTargetArrayLength = array_size;
      return S_OK;
    }
    if (texture_type == MTL::TextureTypeCube) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? 6 - pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      AttachmentDesc.RenderTargetArrayLength = array_size;
      return S_OK;
    }
    if (texture_type == MTL::TextureTypeCubeArray) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? 6 * pResource->arrayLength() -
                                  pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      AttachmentDesc.RenderTargetArrayLength = array_size;
      return S_OK;
    }
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DMS: {
    if (texture_type == MTL::TextureType2DMultisample) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DMultisample,
          NS::Range::Make(0, 1), NS::Range::Make(0, 1));
      return S_OK;
    }
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: {
    if (texture_type == MTL::TextureType2DMultisample) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DMultisampleArray,
          NS::Range::Make(0, 1), NS::Range::Make(0, 1));
      AttachmentDesc.RenderTargetArrayLength = 1;
      return S_OK;
    }
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE3D: {
    if (texture_type == MTL::TextureType3D) {
      if (pViewDesc->Texture3D.WSize == 1) {
        *ppView = pResource->newTextureView(
            metal_format.PixelFormat, MTL::TextureType3D,
            NS::Range::Make(pViewDesc->Texture3D.MipSlice, 1),
            NS::Range::Make(0, 1));
        AttachmentDesc.DepthPlane = pViewDesc->Texture3D.FirstWSlice;
        return S_OK;
      }
      if (pViewDesc->Texture3D.WSize == pResource->depth() ||
          ((pViewDesc->Texture3D.WSize == 0xffffffff) &&
           pViewDesc->Texture3D.FirstWSlice == 0)) {
        *ppView = pResource->newTextureView(
            metal_format.PixelFormat, MTL::TextureType3D,
            NS::Range::Make(pViewDesc->Texture3D.MipSlice, 1),
            NS::Range::Make(0, 1));
        AttachmentDesc.RenderTargetArrayLength = pResource->depth();
        return S_OK;
      }
      ERR("tex3d rtv creation not properly handled: ",
          pViewDesc->Texture3D.FirstWSlice, ":", pViewDesc->Texture3D.WSize,
          ":", pResource->depth());
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType3D,
          NS::Range::Make(pViewDesc->Texture3D.MipSlice, 1),
          NS::Range::Make(0, 1));
      AttachmentDesc.DepthPlane = pViewDesc->Texture3D.FirstWSlice;
      return S_OK;
    }
    break;
  }
  }
  ERR("Unhandled rtv creation: \n Source: ", texture_type,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

HRESULT
CreateMTLDepthStencilView(IMTLD3D11Device *pDevice, MTL::Texture *pResource,
                          const D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDesc,
                          MTL::Texture **ppView,
                          MTL_RENDER_PASS_ATTACHMENT_DESC &AttachmentDesc) {
  MTL_DXGI_FORMAT_DESC metal_format;
  if (FAILED(MTLQueryDXGIFormat(pDevice->GetMTLDevice(), pViewDesc->Format,
                                metal_format))) {
    ERR("Failed to create DSV due to unsupported format ", pViewDesc->Format);
    return E_FAIL;
  }

  AttachmentDesc.Slice = 0; // use the original texture if format is the same?
  AttachmentDesc.Level = 0;
  AttachmentDesc.DepthPlane = 0;
  AttachmentDesc.RenderTargetArrayLength = 0;
  AttachmentDesc.SampleCount = pResource->sampleCount();

  auto texture_type = pResource->textureType();
  switch (pViewDesc->ViewDimension) {
  default:
    break;
  case D3D11_DSV_DIMENSION_TEXTURE1D: {
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE1DARRAY: {
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE2D: {
    if (texture_type == MTL::TextureType2D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2D,
          NS::Range::Make(pViewDesc->Texture2D.MipSlice, 1),
          NS::Range::Make(0, 1));
      return S_OK;
    }
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE2DARRAY: {
    if (texture_type == MTL::TextureType2D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(0, 1));
      AttachmentDesc.RenderTargetArrayLength = 1;
      return S_OK;
    }
    if (texture_type == MTL::TextureType2DArray) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? pResource->arrayLength() -
                                  pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      AttachmentDesc.RenderTargetArrayLength = array_size;
      return S_OK;
    }
    if (texture_type == MTL::TextureTypeCube) {
      auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                            ? 6 * pResource->arrayLength() -
                                  pViewDesc->Texture2DArray.FirstArraySlice
                            : pViewDesc->Texture2DArray.ArraySize;
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      AttachmentDesc.RenderTargetArrayLength = array_size;
      return S_OK;
    }
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE2DMS: {
    if (texture_type == MTL::TextureType2DMultisample) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DMultisample,
          NS::Range::Make(0, 1), NS::Range::Make(0, 1));
      return S_OK;
    }
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY: {
    break;
  }
  }
  ERR("Unhandled dsv creation: \n Source: ", texture_type,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

template <>
HRESULT CreateMTLTextureBufferView<D3D11_SHADER_RESOURCE_VIEW_DESC1>(
    IMTLD3D11Device *pDevice, const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pViewDesc,
    MTL::TextureDescriptor **ppViewDesc, MTL_TEXTURE_BUFFER_LAYOUT *pLayout) {
  MTL_DXGI_FORMAT_DESC metal_format;
  if (FAILED(MTLQueryDXGIFormat(pDevice->GetMTLDevice(), pViewDesc->Format,
                                metal_format))) {
    ERR("Failed to create SRV due to unsupported format ", pViewDesc->Format);
    return E_FAIL;
  }
  if (metal_format.Flag & MTL_DXGI_FORMAT_BC) {
    ERR("Failed to create texture buffer SRV: compressed format not supported");
    return E_INVALIDARG;
  }
  switch (pViewDesc->ViewDimension) {
  case D3D11_SRV_DIMENSION_BUFFEREX:
  case D3D11_SRV_DIMENSION_BUFFER: {
    auto bytes_per_pixel = metal_format.BytesPerTexel;
    auto first_element = pViewDesc->Buffer.FirstElement;
    auto num_elements = pViewDesc->Buffer.NumElements;
    auto byte_offset = first_element * bytes_per_pixel;
    auto byte_width = num_elements * bytes_per_pixel;
    auto minimum_alignment =
        pDevice->GetMTLDevice()->minimumTextureBufferAlignmentForPixelFormat(
            metal_format.PixelFormat);
    auto desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType(MTL::TextureTypeTextureBuffer);
    desc->setHeight(1);
    desc->setDepth(1);
    desc->setArrayLength(1);
    desc->setMipmapLevelCount(1);
    desc->setSampleCount(1);
    desc->setUsage(MTL::TextureUsageShaderRead);
    desc->setPixelFormat(metal_format.PixelFormat);

    auto offset_for_alignment =
        byte_offset - alignDown(byte_offset, minimum_alignment);
    auto element_offset = offset_for_alignment / bytes_per_pixel;
    desc->setWidth(element_offset + num_elements);

    *ppViewDesc = desc;
    *pLayout = {byte_offset, byte_width, element_offset,
                byte_offset - offset_for_alignment,
                byte_width + offset_for_alignment};

    return S_OK;
  }
  default:
    break;
  }
  ERR("Unhandled buffer srv creation: \n Source: ", metal_format.PixelFormat,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

template <>
HRESULT CreateMTLTextureBufferView<D3D11_UNORDERED_ACCESS_VIEW_DESC1>(
    IMTLD3D11Device *pDevice,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pViewDesc,
    MTL::TextureDescriptor **ppViewDesc, MTL_TEXTURE_BUFFER_LAYOUT *pLayout) {
  MTL_DXGI_FORMAT_DESC metal_format;
  if (FAILED(MTLQueryDXGIFormat(pDevice->GetMTLDevice(), pViewDesc->Format,
                                metal_format))) {
    return E_FAIL;
  }
  if (metal_format.Flag & MTL_DXGI_FORMAT_BC) {
    ERR("Failed to create texture buffer SRV: compressed format not supported");
    return E_INVALIDARG;
  }
  switch (pViewDesc->ViewDimension) {
  case D3D11_UAV_DIMENSION_BUFFER: {
    auto bytes_per_pixel = metal_format.BytesPerTexel;
    auto first_element = pViewDesc->Buffer.FirstElement;
    auto num_elements = pViewDesc->Buffer.NumElements;
    auto byte_offset = first_element * bytes_per_pixel;
    auto byte_width = num_elements * bytes_per_pixel;
    auto minimum_alignment =
        pDevice->GetMTLDevice()->minimumTextureBufferAlignmentForPixelFormat(
            metal_format.PixelFormat);
    auto desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType(MTL::TextureTypeTextureBuffer);
    desc->setHeight(1);
    desc->setDepth(1);
    desc->setArrayLength(1);
    desc->setMipmapLevelCount(1);
    desc->setSampleCount(1);
    desc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);
    desc->setPixelFormat(metal_format.PixelFormat);

    auto offset_for_alignment =
        byte_offset - alignDown(byte_offset, minimum_alignment);
    auto element_offset = offset_for_alignment / bytes_per_pixel;
    desc->setWidth(element_offset + num_elements);

    *ppViewDesc = desc;
    *pLayout = {byte_offset, byte_width, element_offset,
                byte_offset - offset_for_alignment,
                byte_width + offset_for_alignment};

    return S_OK;
  }
  default:
    break;
  }
  ERR("Unhandled uav buffer creation: \n Source: ", metal_format.PixelFormat,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

template <>
void DowngradeViewDescription(const D3D11_SHADER_RESOURCE_VIEW_DESC1 &src,
                              D3D11_SHADER_RESOURCE_VIEW_DESC *pDst) {
  pDst->ViewDimension = src.ViewDimension;
  pDst->Format = src.Format;
  switch (src.ViewDimension) {
  case D3D_SRV_DIMENSION_UNKNOWN:
    break;
  case D3D_SRV_DIMENSION_BUFFER:
    pDst->Buffer = src.Buffer;
    break;
  case D3D_SRV_DIMENSION_TEXTURE1D:
    pDst->Texture1D = src.Texture1D;
    break;
  case D3D_SRV_DIMENSION_TEXTURE1DARRAY:
    pDst->Texture1DArray = src.Texture1DArray;
    break;
  case D3D_SRV_DIMENSION_TEXTURE2D: {
    pDst->Texture2D.MipLevels = src.Texture2D.MipLevels;
    pDst->Texture2D.MostDetailedMip = src.Texture2D.MostDetailedMip;
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DARRAY: {
    pDst->Texture2DArray.ArraySize = src.Texture2DArray.ArraySize;
    pDst->Texture2DArray.FirstArraySlice = src.Texture2DArray.FirstArraySlice;
    pDst->Texture2DArray.MipLevels = src.Texture2DArray.MipLevels;
    pDst->Texture2DArray.MostDetailedMip = src.Texture2DArray.MostDetailedMip;
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMS:
    pDst->Texture2DMS = src.Texture2DMS;
    break;
  case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
    pDst->Texture2DMSArray = src.Texture2DMSArray;
    break;
  case D3D_SRV_DIMENSION_TEXTURE3D:
    pDst->Texture3D = src.Texture3D;
    break;
  case D3D_SRV_DIMENSION_TEXTURECUBE:
    pDst->TextureCube = src.TextureCube;
    break;
  case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
    pDst->TextureCubeArray = src.TextureCubeArray;
    break;
  case D3D_SRV_DIMENSION_BUFFEREX:
    pDst->BufferEx = src.BufferEx;
    break;
  }
}

template <>
void DowngradeViewDescription(const D3D11_RENDER_TARGET_VIEW_DESC1 &src,
                              D3D11_RENDER_TARGET_VIEW_DESC *pDst) {
  pDst->ViewDimension = src.ViewDimension;
  pDst->Format = src.Format;
  switch (src.ViewDimension) {
  case D3D11_RTV_DIMENSION_UNKNOWN:
    break;
  case D3D11_RTV_DIMENSION_BUFFER:
    pDst->Buffer = src.Buffer;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE1D:
    pDst->Texture1D = src.Texture1D;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
    pDst->Texture1DArray = src.Texture1DArray;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE2D: {
    pDst->Texture2D.MipSlice = src.Texture2D.MipSlice;
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: {
    pDst->Texture2DArray.ArraySize = src.Texture2DArray.ArraySize;
    pDst->Texture2DArray.FirstArraySlice = src.Texture2DArray.FirstArraySlice;
    pDst->Texture2DArray.MipSlice = src.Texture2DArray.MipSlice;
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DMS:
    pDst->Texture2DMS = src.Texture2DMS;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
    pDst->Texture2DMSArray = src.Texture2DMSArray;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE3D:
    pDst->Texture3D = src.Texture3D;
    break;
  }
}

template <>
void DowngradeViewDescription(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 &src,
                              D3D11_UNORDERED_ACCESS_VIEW_DESC *pDst) {
  pDst->ViewDimension = src.ViewDimension;
  pDst->Format = src.Format;
  switch (src.ViewDimension) {
  case D3D11_UAV_DIMENSION_UNKNOWN:
    break;
  case D3D11_UAV_DIMENSION_BUFFER:
    pDst->Buffer = src.Buffer;
    break;
  case D3D11_UAV_DIMENSION_TEXTURE1D:
    pDst->Texture1D = src.Texture1D;
    break;
  case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
    pDst->Texture1DArray = src.Texture1DArray;
    break;
  case D3D11_UAV_DIMENSION_TEXTURE2D: {
    pDst->Texture2D.MipSlice = src.Texture2D.MipSlice;
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: {
    pDst->Texture2DArray.MipSlice = src.Texture2DArray.MipSlice;
    pDst->Texture2DArray.FirstArraySlice = src.Texture2DArray.FirstArraySlice;
    pDst->Texture2DArray.ArraySize = src.Texture2DArray.ArraySize;
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE3D:
    pDst->Texture3D = src.Texture3D;
    break;
  }
}

template <>
void UpgradeViewDescription(const D3D11_SHADER_RESOURCE_VIEW_DESC *pSrc,
                            D3D11_SHADER_RESOURCE_VIEW_DESC1 &dst) {
  dst.Format = pSrc->Format;
  dst.ViewDimension = pSrc->ViewDimension;
  switch (pSrc->ViewDimension) {
  case D3D_SRV_DIMENSION_UNKNOWN:
    break;
  case D3D_SRV_DIMENSION_BUFFER:
    dst.Buffer = pSrc->Buffer;
    break;
  case D3D_SRV_DIMENSION_TEXTURE1D:
    dst.Texture1D = pSrc->Texture1D;
    break;
  case D3D_SRV_DIMENSION_TEXTURE1DARRAY:
    dst.Texture1DArray = pSrc->Texture1DArray;
    break;
  case D3D_SRV_DIMENSION_TEXTURE2D: {
    dst.Texture2D.MipLevels = pSrc->Texture2D.MipLevels;
    dst.Texture2D.MostDetailedMip = pSrc->Texture2D.MostDetailedMip;
    dst.Texture2D.PlaneSlice = 0;
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DARRAY: {
    dst.Texture2DArray.MipLevels = pSrc->Texture2DArray.MipLevels;
    dst.Texture2DArray.MostDetailedMip = pSrc->Texture2DArray.MostDetailedMip;
    dst.Texture2DArray.ArraySize = pSrc->Texture2DArray.ArraySize;
    dst.Texture2DArray.FirstArraySlice = pSrc->Texture2DArray.FirstArraySlice;
    dst.Texture2DArray.PlaneSlice = 0;
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMS:
    dst.Texture2DMS = pSrc->Texture2DMS;
    break;
  case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY:
    dst.Texture2DMSArray = pSrc->Texture2DMSArray;
    break;
  case D3D_SRV_DIMENSION_TEXTURE3D:
    dst.Texture3D = pSrc->Texture3D;
    break;
  case D3D_SRV_DIMENSION_TEXTURECUBE:
    dst.TextureCube = pSrc->TextureCube;
    break;
  case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
    dst.TextureCubeArray = pSrc->TextureCubeArray;
    break;
  case D3D_SRV_DIMENSION_BUFFEREX:
    dst.BufferEx = pSrc->BufferEx;
    break;
  }
};

template <>
void UpgradeViewDescription(const D3D11_UNORDERED_ACCESS_VIEW_DESC *pSrc,
                            D3D11_UNORDERED_ACCESS_VIEW_DESC1 &dst) {
  dst.Format = pSrc->Format;
  dst.ViewDimension = pSrc->ViewDimension;
  switch (pSrc->ViewDimension) {
  case D3D11_UAV_DIMENSION_UNKNOWN:
    break;
  case D3D11_UAV_DIMENSION_BUFFER:
    dst.Buffer = pSrc->Buffer;
    break;
  case D3D11_UAV_DIMENSION_TEXTURE1D:
    dst.Texture1D = pSrc->Texture1D;
    break;
  case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
    dst.Texture1DArray = pSrc->Texture1DArray;
    break;
  case D3D11_UAV_DIMENSION_TEXTURE2D: {
    dst.Texture2D.MipSlice = pSrc->Texture2D.MipSlice;
    dst.Texture2D.PlaneSlice = 0;
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: {
    dst.Texture2DArray.MipSlice = pSrc->Texture2DArray.MipSlice;
    dst.Texture2DArray.ArraySize = pSrc->Texture2DArray.ArraySize;
    dst.Texture2DArray.FirstArraySlice = pSrc->Texture2DArray.FirstArraySlice;
    dst.Texture2DArray.PlaneSlice = 0;
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE3D:
    dst.Texture3D = pSrc->Texture3D;
    break;
  }
};

template <>
void UpgradeViewDescription(const D3D11_RENDER_TARGET_VIEW_DESC *pSrc,
                            D3D11_RENDER_TARGET_VIEW_DESC1 &dst) {
  dst.Format = pSrc->Format;
  dst.ViewDimension = pSrc->ViewDimension;
  switch (pSrc->ViewDimension) {
  case D3D11_RTV_DIMENSION_UNKNOWN:
    break;
  case D3D11_RTV_DIMENSION_BUFFER:
    dst.Buffer = pSrc->Buffer;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE1D:
    dst.Texture1D = pSrc->Texture1D;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
    dst.Texture1DArray = pSrc->Texture1DArray;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE2D: {
    dst.Texture2D.MipSlice = pSrc->Texture2D.MipSlice;
    dst.Texture2D.PlaneSlice = 0;
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DARRAY: {
    dst.Texture2DArray.MipSlice = pSrc->Texture2DArray.MipSlice;
    dst.Texture2DArray.ArraySize = pSrc->Texture2DArray.ArraySize;
    dst.Texture2DArray.FirstArraySlice = pSrc->Texture2DArray.FirstArraySlice;
    dst.Texture2DArray.PlaneSlice = 0;
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DMS:
    dst.Texture2DMS = pSrc->Texture2DMS;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
    dst.Texture2DMSArray = pSrc->Texture2DMSArray;
    break;
  case D3D11_RTV_DIMENSION_TEXTURE3D:
    dst.Texture3D = pSrc->Texture3D;
    break;
  }
};

} // namespace dxmt