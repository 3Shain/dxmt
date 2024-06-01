#include "mtld11_resource.hpp"
#include "d3d11_1.h"

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

}; // namespace dxmt