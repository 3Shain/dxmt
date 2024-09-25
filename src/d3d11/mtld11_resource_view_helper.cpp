#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

template <>
HRESULT CreateMTLTextureView<D3D11_SHADER_RESOURCE_VIEW_DESC1>(
    IMTLD3D11Device *pDevice, MTL::Texture *pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pViewDesc, MTL::Texture **ppView) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pViewDesc->Format, &metal_format))) {
    ERR("Failed to create SRV due to unsupported format ", pViewDesc->Format);
    return E_FAIL;
  }
  auto texture_type = pResource->textureType();
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
          NS::Range::Make(0, 1));
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
                          array_size));
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
          NS::Range::Make(0, 1));
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
          NS::Range::Make(pViewDesc->Texture2DArray.MostDetailedMip,
                          pViewDesc->Texture2DArray.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture2DArray.MostDetailedMip
                              : pViewDesc->Texture2DArray.MipLevels),
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
          NS::Range::Make(pViewDesc->Texture2DArray.MostDetailedMip,
                          pViewDesc->Texture2DArray.MipLevels == 0xffffffffu
                              ? pResource->mipmapLevelCount() -
                                    pViewDesc->Texture2DArray.MostDetailedMip
                              : pViewDesc->Texture2DArray.MipLevels),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMS: {
    if (texture_type == MTL::TextureType2DMultisample) {
      *ppView = pResource->newTextureView(metal_format.PixelFormat);
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
          NS::Range::Make(0, 1));
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
          NS::Range::Make(0, 6));
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
          NS::Range::Make(0, 6));
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
                          pViewDesc->TextureCubeArray.NumCubes * 6));
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
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pViewDesc->Format, &metal_format))) {
    return E_FAIL;
  }
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
                          MTL_RENDER_TARGET_VIEW_DESC *pMTLDesc) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pViewDesc->Format, &metal_format))) {
    return E_FAIL;
  }
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
      pMTLDesc->Slice = 0; // use the original texture if format is the same?
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = 0;
      pMTLDesc->RenderTargetArrayLength = array_size;
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
      pMTLDesc->Slice = 0; // use the original texture if format is the same?
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = 0;
      pMTLDesc->RenderTargetArrayLength = 0;
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
      pMTLDesc->Slice = 0; // use the original texture if format is the same?
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = 0;
      pMTLDesc->RenderTargetArrayLength = 1;
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
      pMTLDesc->Slice = 0; // use the original texture if format is the same?
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = 0;
      pMTLDesc->RenderTargetArrayLength = array_size;
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
      pMTLDesc->Slice = 0; // use the original texture if format is the same?
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = 0;
      pMTLDesc->RenderTargetArrayLength = array_size;
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
      pMTLDesc->Slice = 0; // use the original texture if format is the same?
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = 0;
      pMTLDesc->RenderTargetArrayLength = array_size;
      return S_OK;
    }
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DMS: {
    if (texture_type == MTL::TextureType2DMultisample) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DMultisample,
          NS::Range::Make(0, 1), NS::Range::Make(0, 1));
      pMTLDesc->Slice = 0;
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = 0;
      pMTLDesc->RenderTargetArrayLength = 0;
      return S_OK;
    }
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY: {
    if (texture_type == MTL::TextureType2DMultisample) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DMultisampleArray,
          NS::Range::Make(0, 1), NS::Range::Make(0, 1));
      pMTLDesc->Slice = 0;
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = 0;
      pMTLDesc->RenderTargetArrayLength = 1;
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
        pMTLDesc->Slice = 0;
        pMTLDesc->Level = 0;
        pMTLDesc->DepthPlane = pViewDesc->Texture3D.FirstWSlice;
        pMTLDesc->RenderTargetArrayLength = 0; // FIXME: really?
        return S_OK;
      }
      if ((pViewDesc->Texture3D.WSize == pResource->depth() ||
           (pViewDesc->Texture3D.WSize == 0xffffffff)) &&
          pViewDesc->Texture3D.FirstWSlice == 0) {
        *ppView = pResource->newTextureView(
            metal_format.PixelFormat, MTL::TextureType3D,
            NS::Range::Make(pViewDesc->Texture3D.MipSlice, 1),
            NS::Range::Make(0, 1));
        pMTLDesc->Slice = 0;
        pMTLDesc->Level = 0;
        pMTLDesc->DepthPlane = 0;
        pMTLDesc->RenderTargetArrayLength = pResource->depth();
        return S_OK;
      }
      ERR("tex3d rtv creation not properly handled: ",
          pViewDesc->Texture3D.FirstWSlice, ":", pViewDesc->Texture3D.WSize,
          ":", pResource->depth());
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType3D,
          NS::Range::Make(pViewDesc->Texture3D.MipSlice, 1),
          NS::Range::Make(0, 1));
      pMTLDesc->Slice = 0;
      pMTLDesc->Level = 0;
      pMTLDesc->DepthPlane = pViewDesc->Texture3D.FirstWSlice;
      pMTLDesc->RenderTargetArrayLength = 0; // FIXME: really?
      return S_OK;
    }
    break;
  }
  }
  ERR("Unhandled rtv creation: \n Source: ", texture_type,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

template <>
HRESULT CreateMTLTextureView<D3D11_DEPTH_STENCIL_VIEW_DESC>(
    IMTLD3D11Device *pDevice, MTL::Texture *pResource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDesc, MTL::Texture **ppView) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pViewDesc->Format, &metal_format))) {
    ERR("Failed to create DSV due to unsupported format ", pViewDesc->Format);
    return E_FAIL;
  }
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
    auto array_size = pViewDesc->Texture2DArray.ArraySize == 0xffffffff
                          ? 6 * pResource->arrayLength() -
                                pViewDesc->Texture2DArray.FirstArraySlice
                          : pViewDesc->Texture2DArray.ArraySize;
    if (texture_type == MTL::TextureType2D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      return S_OK;
    }
    if (texture_type == MTL::TextureType2DArray) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2DArray,
          NS::Range::Make(pViewDesc->Texture2DArray.MipSlice, 1),
          NS::Range::Make(pViewDesc->Texture2DArray.FirstArraySlice,
                          array_size));
      return S_OK;
    }
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE2DMS: {
    if (texture_type == MTL::TextureType2DMultisample) {
      *ppView = pResource->newTextureView(metal_format.PixelFormat);
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
HRESULT CreateMTLTextureView<D3D11_SHADER_RESOURCE_VIEW_DESC1>(
    IMTLD3D11Device *pDevice, MTL::Buffer *pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pViewDesc, MTL::Texture **ppView) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pViewDesc->Format, &metal_format))) {
    ERR("Failed to create SRV due to unsupported format ", pViewDesc->Format);
    return E_FAIL;
  }
  // TODO: check PixelFormat is ordinary/packed
  auto bytes_per_pixel = metal_format.BytesPerTexel;
  switch (pViewDesc->ViewDimension) {
  case D3D11_SRV_DIMENSION_BUFFEREX: {
    if (pViewDesc->BufferEx.Flags) {
      return E_INVALIDARG;
    }
    [[fallthrough]];
  }
  case D3D11_SRV_DIMENSION_BUFFER: {
    auto first_element = pViewDesc->Buffer.FirstElement;
    auto num_elements = pViewDesc->Buffer.NumElements;
    auto pool = transfer(NS::AutoreleasePool::alloc()->init());
    auto texture_desc = MTL::TextureDescriptor::textureBufferDescriptor(
        metal_format.PixelFormat, num_elements, pResource->resourceOptions(),
        MTL::TextureUsageShaderRead);
    // TODO: check offset is aligned with
    // minimumLinearTextureAlignmentForPixelFormat
    *ppView =
        pResource->newTexture(texture_desc, bytes_per_pixel * first_element,
                              bytes_per_pixel * num_elements);
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
HRESULT CreateMTLTextureView<D3D11_UNORDERED_ACCESS_VIEW_DESC1>(
    IMTLD3D11Device *pDevice, MTL::Buffer *pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pViewDesc, MTL::Texture **ppView) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pViewDesc->Format, &metal_format))) {
    return E_FAIL;
  }
  // TODO: check PixelFormat is ordinary/packed
  auto bytes_per_pixel = metal_format.BytesPerTexel;
  switch (pViewDesc->ViewDimension) {
  case D3D11_UAV_DIMENSION_BUFFER: {
    if (pViewDesc->Buffer.Flags) {
      return E_INVALIDARG;
    }
    auto first_element = pViewDesc->Buffer.FirstElement;
    auto num_elements = pViewDesc->Buffer.NumElements;
    auto pool = transfer(NS::AutoreleasePool::alloc()->init());
    auto texture_desc = MTL::TextureDescriptor::textureBufferDescriptor(
        metal_format.PixelFormat, num_elements, pResource->resourceOptions(),
        MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);
    // TODO: check offset is aligned with
    // minimumLinearTextureAlignmentForPixelFormat
    *ppView =
        pResource->newTexture(texture_desc, bytes_per_pixel * first_element,
                              bytes_per_pixel * num_elements);
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