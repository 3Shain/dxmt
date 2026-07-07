/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "d3d12_device.hpp"

namespace dxmt {

template <>
HRESULT
ExtractEntireResourceViewDescription<D3D12_DEPTH_STENCIL_VIEW_DESC>(
    const D3D12_RESOURCE_DESC &ResourceDesc, D3D12_DEPTH_STENCIL_VIEW_DESC *pViewDescOut
) {
  pViewDescOut->Flags = D3D12_DSV_FLAG_NONE;
  pViewDescOut->Format = ResourceDesc.Format;
  switch (ResourceDesc.Dimension) {
  case D3D12_RESOURCE_DIMENSION_BUFFER: {
    ERR("Unsupported buffer DSV");
    return E_FAIL;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
    if (ResourceDesc.DepthOrArraySize > 1) {
      pViewDescOut->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
      pViewDescOut->Texture1DArray.MipSlice = 0;
      pViewDescOut->Texture1DArray.FirstArraySlice = 0;
      pViewDescOut->Texture1DArray.ArraySize = ResourceDesc.DepthOrArraySize;
    } else {
      pViewDescOut->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
      pViewDescOut->Texture1D.MipSlice = 0;
    }
    break;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
    if (ResourceDesc.SampleDesc.Count > 1) {
      if (ResourceDesc.DepthOrArraySize > 1) {
        pViewDescOut->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
        pViewDescOut->Texture2DMSArray.FirstArraySlice = 0;
        pViewDescOut->Texture2DMSArray.ArraySize = ResourceDesc.DepthOrArraySize;
      } else {
        pViewDescOut->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
      }
    } else {
      if (ResourceDesc.DepthOrArraySize > 1) {
        pViewDescOut->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        pViewDescOut->Texture2DArray.MipSlice = 0;
        pViewDescOut->Texture2DArray.FirstArraySlice = 0;
        pViewDescOut->Texture2DArray.ArraySize = ResourceDesc.DepthOrArraySize;
      } else {
        pViewDescOut->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        pViewDescOut->Texture2D.MipSlice = 0;
      }
    }
    break;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
    ERR("Unsupported 3d DSV");
    return E_FAIL;
  }
  default:
    return E_INVALIDARG;
  }

  return S_OK;
}

template <>
HRESULT
ExtractEntireResourceViewDescription<D3D12_RENDER_TARGET_VIEW_DESC>(
    const D3D12_RESOURCE_DESC &ResourceDesc, D3D12_RENDER_TARGET_VIEW_DESC *pViewDescOut
) {
  pViewDescOut->Format = ResourceDesc.Format;
  switch (ResourceDesc.Dimension) {
  case D3D12_RESOURCE_DIMENSION_BUFFER: {
    ERR("Unsupported buffer RTV");
    return E_FAIL;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
    if (ResourceDesc.DepthOrArraySize > 1) {
      pViewDescOut->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
      pViewDescOut->Texture1DArray.MipSlice = 0;
      pViewDescOut->Texture1DArray.FirstArraySlice = 0;
      pViewDescOut->Texture1DArray.ArraySize = ResourceDesc.DepthOrArraySize;
    } else {
      pViewDescOut->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
      pViewDescOut->Texture1D.MipSlice = 0;
    }
    break;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
    if (ResourceDesc.SampleDesc.Count > 1) {
      if (ResourceDesc.DepthOrArraySize > 1) {
        pViewDescOut->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
        pViewDescOut->Texture2DMSArray.FirstArraySlice = 0;
        pViewDescOut->Texture2DMSArray.ArraySize = ResourceDesc.DepthOrArraySize;
      } else {
        pViewDescOut->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
      }
    } else {
      if (ResourceDesc.DepthOrArraySize > 1) {
        pViewDescOut->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        pViewDescOut->Texture2DArray.MipSlice = 0;
        pViewDescOut->Texture2DArray.FirstArraySlice = 0;
        pViewDescOut->Texture2DArray.ArraySize = ResourceDesc.DepthOrArraySize;
        pViewDescOut->Texture2DArray.PlaneSlice = 0; // FIXME(resource-planar)
      } else {
        pViewDescOut->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        pViewDescOut->Texture2D.MipSlice = 0;
        pViewDescOut->Texture2D.PlaneSlice = 0; // FIXME(resource-planar)
      }
    }
    break;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
    pViewDescOut->ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
    pViewDescOut->Texture3D.FirstWSlice = 0;
    pViewDescOut->Texture3D.WSize = ResourceDesc.DepthOrArraySize;
    pViewDescOut->Texture3D.MipSlice = 0;
    break;
  }
  default:
    return E_INVALIDARG;
  }

  return S_OK;
}

template <>
HRESULT
ExtractEntireResourceViewDescription<D3D12_SHADER_RESOURCE_VIEW_DESC>(
    const D3D12_RESOURCE_DESC &ResourceDesc, D3D12_SHADER_RESOURCE_VIEW_DESC *pViewDescOut
) {
  pViewDescOut->Format = ResourceDesc.Format;
  switch (ResourceDesc.Dimension) {
  case D3D12_RESOURCE_DIMENSION_BUFFER: {
    ERR("Unsupported buffer SRV");
    return E_FAIL;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
    if (ResourceDesc.DepthOrArraySize > 1) {
      pViewDescOut->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
      pViewDescOut->Shader4ComponentMapping = kDefaultShader4Component;
      pViewDescOut->Texture1DArray.MostDetailedMip = 0;
      pViewDescOut->Texture1DArray.MipLevels = ResourceDesc.MipLevels;
      pViewDescOut->Texture1DArray.FirstArraySlice = 0;
      pViewDescOut->Texture1DArray.ArraySize = ResourceDesc.DepthOrArraySize;
      pViewDescOut->Texture1DArray.ResourceMinLODClamp = 0;
    } else {
      pViewDescOut->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
      pViewDescOut->Shader4ComponentMapping = kDefaultShader4Component;
      pViewDescOut->Texture1D.MostDetailedMip = 0;
      pViewDescOut->Texture1D.MipLevels = ResourceDesc.MipLevels;
      pViewDescOut->Texture1D.ResourceMinLODClamp = 0;
    }
    break;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
    if (ResourceDesc.SampleDesc.Count > 1) {
      if (ResourceDesc.DepthOrArraySize > 1) {
        pViewDescOut->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
        pViewDescOut->Shader4ComponentMapping = kDefaultShader4Component;
        pViewDescOut->Texture2DMSArray.FirstArraySlice = 0;
        pViewDescOut->Texture2DMSArray.ArraySize = ResourceDesc.DepthOrArraySize;
      } else {
        pViewDescOut->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        pViewDescOut->Shader4ComponentMapping = kDefaultShader4Component;
      }
    } else {
      if (ResourceDesc.DepthOrArraySize > 1) {
        pViewDescOut->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        pViewDescOut->Shader4ComponentMapping = kDefaultShader4Component;
        pViewDescOut->Texture2DArray.MostDetailedMip = 0;
        pViewDescOut->Texture2DArray.MipLevels = ResourceDesc.MipLevels;
        pViewDescOut->Texture2DArray.FirstArraySlice = 0;
        pViewDescOut->Texture2DArray.ArraySize = ResourceDesc.DepthOrArraySize;
        pViewDescOut->Texture2DArray.PlaneSlice = 0; // FIXME(resource-planar)
        pViewDescOut->Texture2DArray.ResourceMinLODClamp = 0;
      } else {
        pViewDescOut->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        pViewDescOut->Shader4ComponentMapping = kDefaultShader4Component;
        pViewDescOut->Texture2D.MostDetailedMip = 0;
        pViewDescOut->Texture2D.MipLevels = ResourceDesc.MipLevels;
        pViewDescOut->Texture2D.PlaneSlice = 0; // FIXME(resource-planar)
        pViewDescOut->Texture2D.ResourceMinLODClamp = 0;
      }
    }
    break;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
    pViewDescOut->ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    pViewDescOut->Shader4ComponentMapping = kDefaultShader4Component;
    pViewDescOut->Texture3D.MostDetailedMip = 0;
    pViewDescOut->Texture3D.MipLevels = ResourceDesc.MipLevels;
    pViewDescOut->Texture3D.ResourceMinLODClamp = 0;
    break;
  }
  default:
    return E_INVALIDARG;
  }

  return S_OK;
}

} // namespace dxmt