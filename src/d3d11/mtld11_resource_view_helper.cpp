#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

template <>
HRESULT CreateMTLTextureView<D3D11_SHADER_RESOURCE_VIEW_DESC>(
    IMTLD3D11Device *pDevice, MTL::Texture *pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDesc, MTL::Texture **ppView) {
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
    break;
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
    break;
  }
  case D3D_SRV_DIMENSION_TEXTURECUBEARRAY: {
    break;
  }
  }
  ERR("Unhandled srv creation: \n Source: ", texture_type,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

template <>
HRESULT CreateMTLTextureView<D3D11_UNORDERED_ACCESS_VIEW_DESC>(
    IMTLD3D11Device *pDevice, MTL::Texture *pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC *pViewDesc, MTL::Texture **ppView) {
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
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE2DARRAY: {
    break;
  }
  case D3D11_UAV_DIMENSION_TEXTURE3D: {
    break;
  }
  }
  ERR("Unhandled uav creation: \n Source: ", texture_type,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

template <>
HRESULT CreateMTLTextureView<D3D11_RENDER_TARGET_VIEW_DESC>(
    IMTLD3D11Device *pDevice, MTL::Texture *pResource,
    const D3D11_RENDER_TARGET_VIEW_DESC *pViewDesc, MTL::Texture **ppView) {
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
    break;
  }
  case D3D11_RTV_DIMENSION_TEXTURE2D: {
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
    break;
  }
  case D3D11_DSV_DIMENSION_TEXTURE2DMS: {
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
HRESULT CreateMTLTextureView<D3D11_SHADER_RESOURCE_VIEW_DESC>(
    IMTLD3D11Device *pDevice, MTL::Buffer *pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDesc, MTL::Texture **ppView) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pViewDesc->Format, &metal_format))) {
    ERR("Failed to create SRV due to unsupported format ", pViewDesc->Format);
    return E_FAIL;
  }
  // TODO: check PixelFormat is ordinary/packed
  auto bytes_per_pixel = metal_format.Stride;
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
  ERR("Unhandled srv creation: \n Source: ", metal_format.PixelFormat,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

template <>
HRESULT CreateMTLTextureView<D3D11_UNORDERED_ACCESS_VIEW_DESC>(
    IMTLD3D11Device *pDevice, MTL::Buffer *pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC *pViewDesc, MTL::Texture **ppView) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  MTL_FORMAT_DESC metal_format;
  if (FAILED(adapter->QueryFormatDesc(pViewDesc->Format, &metal_format))) {
    return E_FAIL;
  }
  // TODO: check PixelFormat is ordinary/packed
  auto bytes_per_pixel = metal_format.Stride;
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
  ERR("Unhandled uav creation: \n Source: ", metal_format.PixelFormat,
      "\n Desired: ", pViewDesc->ViewDimension);
  return E_FAIL;
}

} // namespace dxmt