#include "d3d11_private.h"
#include "mtld11_resource.hpp"
#include "d3d11_1.h"
#include "objc_pointer.hpp"
#include "util_math.hpp"

namespace dxmt {

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_BUFFER_DESC,
                                             D3D11_SHADER_RESOURCE_VIEW_DESC>(
    const D3D11_BUFFER_DESC *pResourceDesc,
    D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescOut) {
  if (pResourceDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
    pViewDescOut->Format = DXGI_FORMAT_UNKNOWN;
    pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    pViewDescOut->Buffer.FirstElement = 0;
    pViewDescOut->Buffer.NumElements =
        pResourceDesc->ByteWidth / pResourceDesc->StructureByteStride;
  } else {
    return E_FAIL;
  }
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_BUFFER_DESC,
                                             D3D11_UNORDERED_ACCESS_VIEW_DESC>(
    const D3D11_BUFFER_DESC *pResourceDesc,
    D3D11_UNORDERED_ACCESS_VIEW_DESC *pViewDescOut) {
  if (pResourceDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
    pViewDescOut->Format = DXGI_FORMAT_UNKNOWN;
    pViewDescOut->ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    pViewDescOut->Buffer.FirstElement = 0;
    pViewDescOut->Buffer.NumElements =
        pResourceDesc->ByteWidth / pResourceDesc->StructureByteStride;
    pViewDescOut->Buffer.Flags = 0;
  } else {
    return E_FAIL;
  }
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE1D_DESC,
                                             D3D11_SHADER_RESOURCE_VIEW_DESC>(
    const D3D11_TEXTURE1D_DESC *pResourceDesc,
    D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescOut) {
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
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE1D_DESC,
                                             D3D11_UNORDERED_ACCESS_VIEW_DESC>(
    const D3D11_TEXTURE1D_DESC *pResourceDesc,
    D3D11_UNORDERED_ACCESS_VIEW_DESC *pViewDescOut) {
  pViewDescOut->Format = pResourceDesc->Format;

  if (pResourceDesc->ArraySize == 1) {
    pViewDescOut->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
    pViewDescOut->Texture1D.MipSlice = 0;
  } else {
    pViewDescOut->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
    pViewDescOut->Texture1DArray.MipSlice = 0;
    pViewDescOut->Texture1DArray.FirstArraySlice = 0;
    pViewDescOut->Texture1DArray.ArraySize = pResourceDesc->ArraySize;
  }
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE1D_DESC,
                                             D3D11_RENDER_TARGET_VIEW_DESC>(
    const D3D11_TEXTURE1D_DESC *pResourceDesc,
    D3D11_RENDER_TARGET_VIEW_DESC *pViewDescOut) {
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
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE1D_DESC,
                                             D3D11_DEPTH_STENCIL_VIEW_DESC>(
    const D3D11_TEXTURE1D_DESC *pResourceDesc,
    D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescOut) {
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
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE2D_DESC,
                                             D3D11_SHADER_RESOURCE_VIEW_DESC>(
    const D3D11_TEXTURE2D_DESC *pResourceDesc,
    D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescOut) {
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
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE2D_DESC,
                                             D3D11_UNORDERED_ACCESS_VIEW_DESC>(
    const D3D11_TEXTURE2D_DESC *pResourceDesc,
    D3D11_UNORDERED_ACCESS_VIEW_DESC *pViewDescOut) {
  pViewDescOut->Format = pResourceDesc->Format;

  if (pResourceDesc->SampleDesc.Count == 1) {
    if (pResourceDesc->ArraySize == 1) {
      pViewDescOut->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
      pViewDescOut->Texture2D.MipSlice = 0;
    } else {
      pViewDescOut->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
      pViewDescOut->Texture2DArray.MipSlice = 0;
      pViewDescOut->Texture2DArray.FirstArraySlice = 0;
      pViewDescOut->Texture2DArray.ArraySize = pResourceDesc->ArraySize;
    }
  } else {
    ERR("Invalid to create DSV from muiltisampled texture");
    return E_FAIL;
  }
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE2D_DESC,
                                             D3D11_DEPTH_STENCIL_VIEW_DESC>(
    const D3D11_TEXTURE2D_DESC *pResourceDesc,
    D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescOut) {
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
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE2D_DESC,
                                             D3D11_RENDER_TARGET_VIEW_DESC>(
    const D3D11_TEXTURE2D_DESC *pResourceDesc,
    D3D11_RENDER_TARGET_VIEW_DESC *pViewDescOut) {
  pViewDescOut->Format = pResourceDesc->Format;
  if (pResourceDesc->SampleDesc.Count == 1) {
    if (pResourceDesc->ArraySize == 1) {
      pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      pViewDescOut->Texture2D.MipSlice = 0;
      //   pViewDescOut->Texture2D.PlaneSlice = 0;
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
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE3D_DESC,
                                             D3D11_SHADER_RESOURCE_VIEW_DESC>(
    const D3D11_TEXTURE3D_DESC *pResourceDesc,
    D3D11_SHADER_RESOURCE_VIEW_DESC *pViewDescOut) {
  pViewDescOut->Format = pResourceDesc->Format;

  pViewDescOut->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
  pViewDescOut->Texture3D.MostDetailedMip = 0;
  pViewDescOut->Texture3D.MipLevels = pResourceDesc->MipLevels;
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE3D_DESC,
                                             D3D11_UNORDERED_ACCESS_VIEW_DESC>(
    const D3D11_TEXTURE3D_DESC *pResourceDesc,
    D3D11_UNORDERED_ACCESS_VIEW_DESC *pViewDescOut) {
  pViewDescOut->Format = pResourceDesc->Format;
  pViewDescOut->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
  pViewDescOut->Texture3D.MipSlice = 0;
  pViewDescOut->Texture3D.FirstWSlice = 0;
  pViewDescOut->Texture3D.WSize = pResourceDesc->Depth;
  return S_OK;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE3D_DESC,
                                             D3D11_DEPTH_STENCIL_VIEW_DESC>(
    const D3D11_TEXTURE3D_DESC *pResourceDesc,
    D3D11_DEPTH_STENCIL_VIEW_DESC *pViewDescOut) {
  ERR("Invalid to create DSV from Texture3D");
  return E_FAIL;
}

template <>
HRESULT ExtractEntireResourceViewDescription<D3D11_TEXTURE3D_DESC,
                                             D3D11_RENDER_TARGET_VIEW_DESC>(
    const D3D11_TEXTURE3D_DESC *pResourceDesc,
    D3D11_RENDER_TARGET_VIEW_DESC *pViewDescOut) {
  pViewDescOut->Format = pResourceDesc->Format;
  pViewDescOut->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
  pViewDescOut->Texture3D.MipSlice = 0;
  pViewDescOut->Texture3D.FirstWSlice = 0;
  pViewDescOut->Texture3D.WSize = pResourceDesc->Depth;
  return S_OK;
}

HRESULT
CreateMTLTextureDescriptorInternal(
    IMTLDXGIAdatper *pAdapter, D3D11_RESOURCE_DIMENSION Dimension, UINT Width,
    UINT Height, UINT Depth, UINT ArraySize, UINT SampleCount, UINT BindFlags,
    UINT CPUAccessFlags, UINT MiscFlags, D3D11_USAGE Usage, UINT MipLevels,
    DXGI_FORMAT Format, MTL::TextureDescriptor **ppDescOut) {
  auto desc = transfer(MTL::TextureDescriptor::alloc()->init());

  MTL_FORMAT_DESC metal_format;

  if (FAILED(pAdapter->QueryFormatDesc(Format, &metal_format))) {
    ERR("CreateMTLTextureDescriptorInternal: creating a texture of invalid "
        "format: ",
        Format);
    return E_FAIL;
  }

  MTL::TextureUsage metal_usage = 0; // actually corresponding to BindFlags

  // DIRTY HACK!
  if (Format == DXGI_FORMAT_R32_TYPELESS &&
      (BindFlags & D3D11_BIND_DEPTH_STENCIL)) {
    desc->setPixelFormat(MTL::PixelFormatDepth32Float);
  } else if (Format == DXGI_FORMAT_R16_TYPELESS &&
             (BindFlags & D3D11_BIND_DEPTH_STENCIL)) {
    desc->setPixelFormat(MTL::PixelFormatDepth16Unorm);
  } else {
    desc->setPixelFormat(metal_format.PixelFormat);
  }

  if (BindFlags & (D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_VERTEX_BUFFER |
                   D3D11_BIND_INDEX_BUFFER | D3D11_BIND_STREAM_OUTPUT)) {
    ERR("CreateMTLTextureDescriptorInternal: invalid bind flags");
    return E_FAIL;
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
    ERR("CreateMTLTextureDescriptorInternal: invalid texture dimension");
    return E_FAIL;
  }
  case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    if (ArraySize > 1) {
      desc->setTextureType(MTL::TextureType1DArray);
      desc->setArrayLength(ArraySize);
    } else {
      desc->setTextureType(MTL::TextureType1D);
    }
    break;
  case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    if (MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) {
      D3D11_ASSERT(ArraySize / 6);
      D3D11_ASSERT((ArraySize % 6) == 0);
      if ((ArraySize / 6) > 1) {
        desc->setTextureType(MTL::TextureTypeCubeArray);
        desc->setArrayLength(ArraySize / 6);
      } else {
        desc->setTextureType(MTL::TextureTypeCube);
      }
    } else {
      if (SampleCount > 1) {
        if (ArraySize > 1) {
          desc->setTextureType(MTL::TextureType2DMultisampleArray);
          desc->setArrayLength(ArraySize);
        } else {
          desc->setTextureType(MTL::TextureType2DMultisample);
        }
        desc->setSampleCount(SampleCount);
      } else {
        if (ArraySize > 1) {
          desc->setTextureType(MTL::TextureType2DArray);
          desc->setArrayLength(ArraySize);
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

  *ppDescOut = desc.takeOwnership();
  return S_OK;
};

template <>
HRESULT CreateMTLTextureDescriptor(IMTLD3D11Device *pDevice,
                                   const D3D11_TEXTURE1D_DESC *pDesc,
                                   D3D11_TEXTURE1D_DESC *pOutDesc,
                                   MTL::TextureDescriptor **pMtlDescOut) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  auto hr = CreateMTLTextureDescriptorInternal(
      adapter.ptr(), D3D11_RESOURCE_DIMENSION_TEXTURE1D, pDesc->Width, 1, 1,
      pDesc->ArraySize, 0, pDesc->BindFlags, pDesc->CPUAccessFlags,
      pDesc->MiscFlags, pDesc->Usage, pDesc->MipLevels, pDesc->Format,
      pMtlDescOut);
  if (SUCCEEDED(hr)) {
    *pOutDesc = *pDesc;
    pOutDesc->MipLevels = (*pMtlDescOut)->mipmapLevelCount();
  }
  return hr;
}

template <>
HRESULT CreateMTLTextureDescriptor(IMTLD3D11Device *pDevice,
                                   const D3D11_TEXTURE2D_DESC *pDesc,
                                   D3D11_TEXTURE2D_DESC *pOutDesc,
                                   MTL::TextureDescriptor **pMtlDescOut) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  auto hr = CreateMTLTextureDescriptorInternal(
      adapter.ptr(), D3D11_RESOURCE_DIMENSION_TEXTURE2D, pDesc->Width,
      pDesc->Height, 1, pDesc->ArraySize, pDesc->SampleDesc.Count,
      pDesc->BindFlags, pDesc->CPUAccessFlags, pDesc->MiscFlags, pDesc->Usage,
      pDesc->MipLevels, pDesc->Format, pMtlDescOut);
  if (SUCCEEDED(hr)) {
    *pOutDesc = *pDesc;
    pOutDesc->MipLevels = (*pMtlDescOut)->mipmapLevelCount();
  }
  return hr;
}

template <>
HRESULT CreateMTLTextureDescriptor(IMTLD3D11Device *pDevice,
                                   const D3D11_TEXTURE3D_DESC *pDesc,
                                   D3D11_TEXTURE3D_DESC *pOutDesc,
                                   MTL::TextureDescriptor **pMtlDescOut) {
  Com<IMTLDXGIAdatper> adapter;
  pDevice->GetAdapter(&adapter);
  auto hr = CreateMTLTextureDescriptorInternal(
      adapter.ptr(), D3D11_RESOURCE_DIMENSION_TEXTURE3D, pDesc->Width,
      pDesc->Height, pDesc->Depth, 1, 1, pDesc->BindFlags,
      pDesc->CPUAccessFlags, pDesc->MiscFlags, pDesc->Usage, pDesc->MipLevels,
      pDesc->Format, pMtlDescOut);
  if (SUCCEEDED(hr)) {
    *pOutDesc = *pDesc;
    pOutDesc->MipLevels = (*pMtlDescOut)->mipmapLevelCount();
  }
  return hr;
}

template <>
void GetMipmapSize(const D3D11_TEXTURE1D_DESC *pDesc, uint32_t level,
                   uint32_t *pWidth, uint32_t *pHeight, uint32_t *pDepth) {
  *pHeight = 1;
  *pDepth = 1;
  *pWidth = std::max(1u, pDesc->Width >> level);
}

template <>
void GetMipmapSize(const D3D11_TEXTURE2D_DESC *pDesc, uint32_t level,
                   uint32_t *pWidth, uint32_t *pHeight, uint32_t *pDepth) {
  *pDepth = 1;
  *pWidth = std::max(1u, pDesc->Width >> level);
  *pHeight = std::max(1u, pDesc->Height >> level);
}

template <>
void GetMipmapSize(const D3D11_TEXTURE3D_DESC *pDesc, uint32_t level,
                   uint32_t *pWidth, uint32_t *pHeight, uint32_t *pDepth) {
  *pWidth = std::max(1u, pDesc->Width >> level);
  *pHeight = std::max(1u, pDesc->Height >> level);
  *pDepth = std::max(1u, pDesc->Depth >> level);
}

template <>
HRESULT GetLinearTextureLayout(IMTLD3D11Device *pDevice,
                               const D3D11_TEXTURE1D_DESC *pDesc,
                               uint32_t level, uint32_t *pBytesPerRow,
                               uint32_t *pBytesPerImage,
                               uint32_t *pBytesPerSlice) {
  Com<IMTLDXGIAdatper> pAdapter;
  pDevice->GetAdapter(&pAdapter);
  auto metal = pDevice->GetMTLDevice();
  MTL_FORMAT_DESC metal_format;

  if (FAILED(pAdapter->QueryFormatDesc(pDesc->Format, &metal_format))) {
    ERR("GetLinearTextureLayout: invalid format: ", pDesc->Format);
    return E_FAIL;
  }
  if (metal_format.PixelFormat == MTL::PixelFormatInvalid) {
    ERR("GetLinearTextureLayout: invalid format: ", pDesc->Format);
    return E_FAIL;
  }
  if (metal_format.BytesPerTexel == 0) {
    ERR("GetLinearTextureLayout: not an ordinary or packed format: ",
        pDesc->Format);
    return E_FAIL;
  }
  uint32_t w, h, d;
  GetMipmapSize(pDesc, level, &w, &h, &d);
  auto bytes_per_row_unaligned = metal_format.BytesPerTexel * w;
  auto alignment = metal->minimumLinearTextureAlignmentForPixelFormat(
      metal_format.PixelFormat);
  auto aligned_bytes_per_row = align(bytes_per_row_unaligned, alignment);
  *pBytesPerRow = aligned_bytes_per_row;
  *pBytesPerImage = 0;
  *pBytesPerSlice = aligned_bytes_per_row;
  return S_OK;
};

template <>
HRESULT GetLinearTextureLayout(IMTLD3D11Device *pDevice,
                               const D3D11_TEXTURE2D_DESC *pDesc,
                               uint32_t level, uint32_t *pBytesPerRow,
                               uint32_t *pBytesPerImage,
                               uint32_t *pBytesPerSlice) {
  Com<IMTLDXGIAdatper> pAdapter;
  pDevice->GetAdapter(&pAdapter);
  auto metal = pDevice->GetMTLDevice();
  MTL_FORMAT_DESC metal_format;

  if (FAILED(pAdapter->QueryFormatDesc(pDesc->Format, &metal_format))) {
    ERR("GetLinearTextureLayout: invalid format: ", pDesc->Format);
    return E_FAIL;
  }
  if (metal_format.PixelFormat == MTL::PixelFormatInvalid) {
    ERR("GetLinearTextureLayout: invalid format: ", pDesc->Format);
    return E_FAIL;
  }
  if (metal_format.BytesPerTexel == 0) {
    ERR("GetLinearTextureLayout: not an ordinary or packed format: ",
        pDesc->Format);
    return E_FAIL;
  }
  uint32_t w, h, d;
  GetMipmapSize(pDesc, level, &w, &h, &d);
  if (metal_format.IsCompressed) {
    D3D11_ASSERT(w != 0 && h != 0);
    auto w_phisical = align(w, 4);
    auto h_phisical = align(h, 4);
    auto aligned_bytes_per_row = metal_format.BytesPerTexel * w_phisical >> 2;
    *pBytesPerRow = aligned_bytes_per_row; // 1 row of block is 4 row of pixel
    *pBytesPerImage = aligned_bytes_per_row * h_phisical >> 2;
    *pBytesPerSlice = aligned_bytes_per_row * h_phisical >> 2;
    return S_OK;
  }
  auto bytes_per_row_unaligned = metal_format.BytesPerTexel * w;
  auto alignment = metal->minimumLinearTextureAlignmentForPixelFormat(
      metal_format.PixelFormat);
  auto aligned_bytes_per_row = align(bytes_per_row_unaligned, alignment);
  *pBytesPerRow = aligned_bytes_per_row;
  *pBytesPerImage = aligned_bytes_per_row * h;
  *pBytesPerSlice = aligned_bytes_per_row * h;
  return S_OK;
};

template <>
HRESULT GetLinearTextureLayout(IMTLD3D11Device *pDevice,
                               const D3D11_TEXTURE3D_DESC *pDesc,
                               uint32_t level, uint32_t *pBytesPerRow,
                               uint32_t *pBytesPerImage,
                               uint32_t *pBytesPerSlice) {
  Com<IMTLDXGIAdatper> pAdapter;
  pDevice->GetAdapter(&pAdapter);
  auto metal = pDevice->GetMTLDevice();
  MTL_FORMAT_DESC metal_format;

  if (FAILED(pAdapter->QueryFormatDesc(pDesc->Format, &metal_format))) {
    ERR("GetLinearTextureLayout: invalid format: ", pDesc->Format);
    return E_FAIL;
  }
  if (metal_format.PixelFormat == MTL::PixelFormatInvalid) {
    ERR("GetLinearTextureLayout: invalid format: ", pDesc->Format);
    return E_FAIL;
  }
  if (metal_format.BytesPerTexel == 0) {
    ERR("GetLinearTextureLayout: not an ordinary or packed format: ",
        pDesc->Format);
    return E_FAIL;
  }
  uint32_t w, h, d;
  GetMipmapSize(pDesc, level, &w, &h, &d);
  auto bytes_per_row_unaligned = metal_format.BytesPerTexel * w;
  auto alignment = metal->minimumLinearTextureAlignmentForPixelFormat(
      metal_format.PixelFormat);
  auto aligned_bytes_per_row = align(bytes_per_row_unaligned, alignment);
  *pBytesPerRow = aligned_bytes_per_row;
  *pBytesPerImage = aligned_bytes_per_row * h;
  *pBytesPerSlice = aligned_bytes_per_row * h * d;
  return S_OK;
};

}; // namespace dxmt