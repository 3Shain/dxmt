#include "dxmt_format.hpp"
#include "mtld11_resource.hpp"

namespace dxmt {

void FixDepthStencilFormat(Texture *pTexture, MTL_DXGI_FORMAT_DESC &Desc) {
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
HRESULT
InitializeAndNormalizeViewDescriptor(
    MTLD3D11Device *pDevice, unsigned MiplevelCount, unsigned ArraySize, Texture* pTexture,
    D3D11_SHADER_RESOURCE_VIEW_DESC1 &ViewDesc, TextureViewDescriptor &Descriptor
) {
  MTL_DXGI_FORMAT_DESC metal_format;
  if (FAILED(MTLQueryDXGIFormat(pDevice->GetMTLDevice(), ViewDesc.Format, metal_format))) {
    ERR("Failed to create SRV due to unsupported format ", ViewDesc.Format);
    return E_FAIL;
  }

  FixDepthStencilFormat(pTexture, metal_format);
  MTL::TextureType TextureType = pTexture->textureType();
  switch (ViewDesc.ViewDimension) {
  default:
    break;
  case D3D_SRV_DIMENSION_TEXTURE1D: {
    if (~ViewDesc.Texture1D.MipLevels == 0)
      ViewDesc.Texture1D.MipLevels = MiplevelCount - ViewDesc.Texture1D.MostDetailedMip;
    if (TextureType == MTL::TextureType1D || TextureType == MTL::TextureType1DArray) {
      Descriptor.type = MTL::TextureType1D;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture1D.MostDetailedMip;
      Descriptor.miplevelCount = ViewDesc.Texture1D.MipLevels;
      Descriptor.firstArraySlice = 0;
      Descriptor.arraySize = 1;
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE1DARRAY: {
    if (~ViewDesc.Texture1DArray.MipLevels == 0)
      ViewDesc.Texture1DArray.MipLevels = MiplevelCount - ViewDesc.Texture1DArray.MostDetailedMip;
    if (~ViewDesc.Texture1DArray.ArraySize == 0)
      ViewDesc.Texture1DArray.ArraySize = ArraySize - ViewDesc.Texture1DArray.FirstArraySlice;
    if (TextureType == MTL::TextureType1D || TextureType == MTL::TextureType1DArray) {
      Descriptor.type = MTL::TextureType1DArray;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture1DArray.MostDetailedMip;
      Descriptor.miplevelCount = ViewDesc.Texture1DArray.MipLevels;
      Descriptor.firstArraySlice = ViewDesc.Texture1DArray.FirstArraySlice;
      Descriptor.arraySize = ViewDesc.Texture1DArray.ArraySize;
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2D: {
    if (~ViewDesc.Texture2D.MipLevels == 0)
      ViewDesc.Texture2D.MipLevels = MiplevelCount - ViewDesc.Texture2D.MostDetailedMip;

    if (TextureType == MTL::TextureType2D) {
      Descriptor.type = MTL::TextureType2D;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture2D.MostDetailedMip;
      Descriptor.miplevelCount = ViewDesc.Texture2D.MipLevels;
      Descriptor.firstArraySlice = 0;
      Descriptor.arraySize = 1;
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DARRAY: {
    if (~ViewDesc.Texture2DArray.MipLevels == 0)
      ViewDesc.Texture2DArray.MipLevels = MiplevelCount - ViewDesc.Texture2DArray.MostDetailedMip;
    if (~ViewDesc.Texture2DArray.ArraySize == 0)
      ViewDesc.Texture2DArray.ArraySize = ArraySize - ViewDesc.Texture2DArray.FirstArraySlice;
    if (TextureType == MTL::TextureType2D || TextureType == MTL::TextureType2DArray ||
        TextureType == MTL::TextureTypeCube || TextureType == MTL::TextureTypeCubeArray) {
      Descriptor.type = MTL::TextureType2DArray;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture2DArray.MostDetailedMip;
      Descriptor.miplevelCount = ViewDesc.Texture2DArray.MipLevels;
      Descriptor.firstArraySlice = ViewDesc.Texture2DArray.FirstArraySlice;
      Descriptor.arraySize = ViewDesc.Texture2DArray.ArraySize;
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMS: {
    if (TextureType == MTL::TextureType2DMultisample) {
      Descriptor.type = MTL::TextureType2DMultisample;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = 0;
      Descriptor.miplevelCount = 1;
      Descriptor.firstArraySlice = 0;
      Descriptor.arraySize = 1;
      return S_OK;
    }
    if (TextureType == MTL::TextureType2D) {
      WARN("A texture2d view is created on multisampled texture");
      Descriptor.type = MTL::TextureType2D;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = 0;
      Descriptor.miplevelCount = 1;
      Descriptor.firstArraySlice = 0;
      Descriptor.arraySize = 1;
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE2DMSARRAY: {
    if (~ViewDesc.Texture2DMSArray.ArraySize == 0)
      ViewDesc.Texture2DMSArray.ArraySize = ArraySize - ViewDesc.Texture2DMSArray.FirstArraySlice;
    // TODO
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURE3D: {
    if (~ViewDesc.Texture3D.MipLevels == 0)
      ViewDesc.Texture3D.MipLevels = MiplevelCount - ViewDesc.Texture3D.MostDetailedMip;
    if (TextureType == MTL::TextureType3D) {
      Descriptor.type = MTL::TextureType3D;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture3D.MostDetailedMip;
      Descriptor.miplevelCount =ViewDesc.Texture3D.MipLevels;
      Descriptor.firstArraySlice = 0;
      Descriptor.arraySize = 1;
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURECUBE: {
    if (~ViewDesc.TextureCube.MipLevels == 0)
      ViewDesc.TextureCube.MipLevels = MiplevelCount - ViewDesc.TextureCube.MostDetailedMip;
    if (TextureType == MTL::TextureTypeCube || TextureType == MTL::TextureTypeCubeArray) {
      Descriptor.type = MTL::TextureTypeCube;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.TextureCube.MostDetailedMip;
      Descriptor.miplevelCount = ~ViewDesc.TextureCube.MipLevels ? ViewDesc.TextureCube.MipLevels
                                                               : MiplevelCount - ViewDesc.TextureCube.MostDetailedMip;
      Descriptor.firstArraySlice = 0;
      Descriptor.arraySize = 6;
      return S_OK;
    }
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURECUBEARRAY: {
    D3D11_ASSERT((ViewDesc.TextureCubeArray.First2DArrayFace % 6) == 0);
    D3D11_ASSERT((ArraySize % 6) == 0);
    if (~ViewDesc.TextureCubeArray.MipLevels == 0)
      ViewDesc.TextureCubeArray.MipLevels = MiplevelCount - ViewDesc.TextureCubeArray.MostDetailedMip;
    if (~ViewDesc.TextureCubeArray.NumCubes == 0)
      ViewDesc.TextureCubeArray.NumCubes = (ArraySize - ViewDesc.TextureCubeArray.First2DArrayFace) / 6;
    if (TextureType == MTL::TextureTypeCube || TextureType == MTL::TextureTypeCubeArray) {
      Descriptor.type = MTL::TextureTypeCubeArray;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.TextureCubeArray.MostDetailedMip;
      Descriptor.miplevelCount = ViewDesc.TextureCubeArray.MipLevels;
      Descriptor.firstArraySlice = ViewDesc.TextureCubeArray.First2DArrayFace;
      Descriptor.arraySize = ViewDesc.TextureCubeArray.NumCubes * 6;
      return S_OK;
    }
    break;
  }
  }
  ERR("Unhandled srv creation: \n Source: ", TextureType, "\n Desired: ", ViewDesc.ViewDimension);
  return E_FAIL;
}

template <>
HRESULT
InitializeAndNormalizeViewDescriptor(
    MTLD3D11Device *pDevice, unsigned MiplevelCount, unsigned ArraySize, Texture *pTexture,
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 &ViewDesc, TextureViewDescriptor &Descriptor
) {
  MTL_DXGI_FORMAT_DESC metal_format;
  if (FAILED(MTLQueryDXGIFormat(pDevice->GetMTLDevice(), ViewDesc.Format,
                                metal_format))) {
    return E_FAIL;
  }

  FixDepthStencilFormat(pTexture, metal_format);
  MTL::TextureType TextureType = pTexture->textureType();
  switch (ViewDesc.ViewDimension) {
  default:
    break;
  case D3D11_UAV_DIMENSION_TEXTURE1D: {
    if (TextureType == MTL::TextureType1D || TextureType == MTL::TextureType1DArray) {
      Descriptor.type = MTL::TextureType1D;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture1D.MipSlice;
      Descriptor.miplevelCount = 1;
      Descriptor.firstArraySlice = 0;
      Descriptor.arraySize = 1;
      return S_OK;
    }
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE1DARRAY: {
    if (~ViewDesc.Texture1DArray.ArraySize == 0)
      ViewDesc.Texture1DArray.ArraySize = ArraySize - ViewDesc.Texture1DArray.FirstArraySlice;
    if (TextureType == MTL::TextureType1D || TextureType == MTL::TextureType1DArray) {
      Descriptor.type = MTL::TextureType1DArray;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture1DArray.MipSlice;
      Descriptor.miplevelCount = 1;
      Descriptor.firstArraySlice = ViewDesc.Texture1DArray.FirstArraySlice;
      Descriptor.arraySize = ViewDesc.Texture1DArray.ArraySize;
      return S_OK;
    }
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2D: {
    D3D11_ASSERT(ViewDesc.Texture2D.PlaneSlice == 0);
    if (TextureType == MTL::TextureType2D || TextureType == MTL::TextureType2DArray ||
        TextureType == MTL::TextureTypeCube) {
      Descriptor.type = MTL::TextureType2D;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture2D.MipSlice;
      Descriptor.miplevelCount = 1;
      Descriptor.firstArraySlice = 0;
      Descriptor.arraySize = 1;
      return S_OK;
    }
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: {
    if (~ViewDesc.Texture2DArray.ArraySize == 0)
      ViewDesc.Texture2DArray.ArraySize = ArraySize - ViewDesc.Texture2DArray.FirstArraySlice;
    if (TextureType == MTL::TextureType2D || TextureType == MTL::TextureType2DArray ||
        TextureType == MTL::TextureTypeCube || TextureType == MTL::TextureTypeCubeArray) {
      Descriptor.type = MTL::TextureType2DArray;
      Descriptor.format = metal_format.PixelFormat;
      Descriptor.firstMiplevel = ViewDesc.Texture2DArray.MipSlice;
      Descriptor.miplevelCount = 1;
      Descriptor.firstArraySlice = ViewDesc.Texture2DArray.FirstArraySlice;
      Descriptor.arraySize = ViewDesc.Texture2DArray.ArraySize;
      return S_OK;
    }
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE3D: {
    if (~ViewDesc.Texture3D.WSize == 0)
      ViewDesc.Texture3D.WSize = ArraySize - ViewDesc.Texture3D.FirstWSlice;
    if (TextureType == MTL::TextureType3D) {

      if ((ViewDesc.Texture3D.WSize == ArraySize) && ViewDesc.Texture3D.FirstWSlice == 0) {
        Descriptor.type = MTL::TextureType3D;
        Descriptor.format = metal_format.PixelFormat;
        Descriptor.firstMiplevel = ViewDesc.Texture3D.MipSlice;
        Descriptor.miplevelCount = 1;
        Descriptor.firstArraySlice = 0;
        Descriptor.arraySize = 1;
        return S_OK;
      }
      ERR("tex3d uav creation not properly handled: ", ViewDesc.Texture3D.FirstWSlice, ":", ViewDesc.Texture3D.WSize,
          ":", ArraySize);
    }
    break;
  }
  }
  ERR("Unhandled uav creation: \n Source: ", TextureType,
      "\n Desired: ", ViewDesc.ViewDimension);
  return E_FAIL;
}

HRESULT
CreateMTLRenderTargetView(MTLD3D11Device *pDevice, MTL::Texture *pResource,
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
    if (texture_type == MTL::TextureType1D) {
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType1D,
          NS::Range::Make(pViewDesc->Texture1D.MipSlice, 1),
          NS::Range::Make(0, 1));
      return S_OK;
    }
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
CreateMTLDepthStencilView(MTLD3D11Device *pDevice, MTL::Texture *pResource,
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
    if (texture_type == MTL::TextureType2DArray) {
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
    if (texture_type == MTL::TextureType2D) {
      WARN("A texture2d view is created on multisampled texture");
      *ppView = pResource->newTextureView(
          metal_format.PixelFormat, MTL::TextureType2D,
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