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
#include "d3d11_surface.hpp"
#include "com/com_object.hpp"
#include "d3d11_resource.hpp"

namespace dxmt {

D3D11DXGISurface::D3D11DXGISurface(ID3D11Resource *pResource) :
    resource_(pResource),
    subresource_(0),
    surface_from_subresource_(false) {}

D3D11DXGISurface::D3D11DXGISurface(ID3D11Resource *pParentResource, UINT Subresource) :
    resource_(pParentResource),
    subresource_(Subresource),
    surface_from_subresource_(true) {}

ULONG STDMETHODCALLTYPE
D3D11DXGISurface::AddRef() {
  return resource_->AddRef();
}

ULONG STDMETHODCALLTYPE
D3D11DXGISurface::Release() {
  return resource_->Release();
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::QueryInterface(REFIID riid, void **ppvObject) {

  InitReturnPtr(ppvObject);

  // Only a subset of interfaces are available for subresource surfaces
  if (surface_from_subresource_) {
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGISurface) || riid == __uuidof(IDXGISurface1) || riid == __uuidof(IDXGISurface2)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  return resource_->QueryInterface(riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) {
  return resource_->GetPrivateData(Name, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) {
  return resource_->SetPrivateData(Name, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) {
  return resource_->SetPrivateDataInterface(Name, pUnknown);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetParent(REFIID riid, void **ppParent) {
  return GetDevice(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetDevice(REFIID riid, void **ppDevice) {
  Com<ID3D11Device> device;
  resource_->GetDevice(&device);
  return device->QueryInterface(riid, ppDevice);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetDesc(DXGI_SURFACE_DESC *pDesc) {
  auto d3d11_resource = GetResourceCommon(resource_);

  if (!pDesc)
    return DXGI_ERROR_INVALID_CALL;

  union {
    D3D11_BUFFER_DESC Buffer;
    D3D11_TEXTURE1D_DESC Texture1D;
    D3D11_TEXTURE2D_DESC Texture2D;
    D3D11_TEXTURE3D_DESC Texture3D;
  } desc;
  D3D11_RESOURCE_DIMENSION dimension;
  d3d11_resource->GetDesc(&desc);
  d3d11_resource->GetType(&dimension);

  if (surface_from_subresource_) {
    if (d3d11_resource->texture()) {
      switch (dimension) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        pDesc->Width = std::max(1u, desc.Texture1D.Width >> (subresource_ % desc.Texture1D.MipLevels));
        pDesc->Height = 1U;
        pDesc->Format = desc.Texture1D.Format;
        pDesc->SampleDesc = {1, 0};
        break;
      }
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        pDesc->Width = std::max(1u, desc.Texture2D.Width >> (subresource_ % desc.Texture2D.MipLevels));
        pDesc->Height = std::max(1u, desc.Texture2D.Height >> (subresource_ % desc.Texture2D.MipLevels));
        pDesc->Format = desc.Texture2D.Format;
        pDesc->SampleDesc = desc.Texture2D.SampleDesc;
        break;
      }
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        pDesc->Width = std::max(1u, desc.Texture3D.Width >> (subresource_ % desc.Texture3D.MipLevels));
        pDesc->Height = std::max(1u, desc.Texture3D.Height >> (subresource_ % desc.Texture3D.MipLevels));
        pDesc->Format = desc.Texture3D.Format;
        pDesc->SampleDesc = {1, 0};
        break;
      }
      default:
        return DXGI_ERROR_INVALID_CALL;
      }
      return S_OK;
    } else if (d3d11_resource->buffer()) {
      pDesc->Width = desc.Buffer.ByteWidth;
      pDesc->Height = 1;
      pDesc->Format = DXGI_FORMAT_UNKNOWN;
      pDesc->SampleDesc = {1, 0};
      return S_OK;
    } else {
      return DXGI_ERROR_INVALID_CALL;
    }
  } else if (d3d11_resource->texture()) {
    switch (dimension) {
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      pDesc->Width = desc.Texture1D.Width;
      pDesc->Height = 1U;
      pDesc->Format = desc.Texture1D.Format;
      pDesc->SampleDesc = {1, 0};
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
      pDesc->Width = desc.Texture2D.Width;
      pDesc->Height = desc.Texture2D.Height;
      pDesc->Format = desc.Texture2D.Format;
      pDesc->SampleDesc = desc.Texture2D.SampleDesc;
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
      pDesc->Width = desc.Texture3D.Width;
      pDesc->Height = desc.Texture3D.Height;
      pDesc->Format = desc.Texture3D.Format;
      pDesc->SampleDesc = {1, 0};
      break;
    }
    default:
      return DXGI_ERROR_INVALID_CALL;
    }
    return S_OK;
  } else {
    return DXGI_ERROR_INVALID_CALL;
  }
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::Map(DXGI_MAPPED_RECT *pLockedRect, UINT MapFlags) {
  Com<ID3D11Device> device;
  Com<ID3D11DeviceContext> context;

  resource_->GetDevice(&device);
  device->GetImmediateContext(&context);

  if (pLockedRect) {
    pLockedRect->Pitch = 0;
    pLockedRect->pBits = nullptr;
  }

  D3D11_MAP map_type;

  if (MapFlags & DXGI_MAP_READ && MapFlags & DXGI_MAP_WRITE)
    map_type = D3D11_MAP_READ_WRITE;
  else if (MapFlags & DXGI_MAP_READ)
    map_type = D3D11_MAP_READ;
  else if (MapFlags & DXGI_MAP_WRITE && MapFlags & DXGI_MAP_DISCARD)
    map_type = D3D11_MAP_WRITE_DISCARD;
  else if (MapFlags & DXGI_MAP_WRITE)
    map_type = D3D11_MAP_WRITE;
  else
    return DXGI_ERROR_INVALID_CALL;

  D3D11_MAPPED_SUBRESOURCE sr;
  HRESULT hr = context->Map(resource_, subresource_, map_type, 0, pLockedRect ? &sr : nullptr);

  if (hr != S_OK)
    return hr;

  pLockedRect->Pitch = sr.RowPitch;
  pLockedRect->pBits = reinterpret_cast<unsigned char *>(sr.pData);
  return hr;
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::Unmap() {
  Com<ID3D11Device> device;
  Com<ID3D11DeviceContext> context;

  resource_->GetDevice(&device);
  device->GetImmediateContext(&context);

  context->Unmap(resource_, subresource_);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetDC(BOOL Discard, HDC *phdc) {
  // TODO(gdi-surface)
  return DXGI_ERROR_INVALID_CALL;
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::ReleaseDC(RECT *pDirtyRect) {
  // TODO(gdi-surface)
  return DXGI_ERROR_INVALID_CALL;
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetResource(REFIID riid, void **ppParentResource, UINT *pSubresourceIndex) {
  HRESULT hr;

  if (!ppParentResource)
    return E_POINTER;

  InitReturnPtr(ppParentResource);
  hr = resource_->QueryInterface(riid, ppParentResource);
  if (SUCCEEDED(hr))
    *pSubresourceIndex = subresource_;

  return hr;
}

}; // namespace dxmt