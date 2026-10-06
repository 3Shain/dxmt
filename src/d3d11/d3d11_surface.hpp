#pragma once
#include "d3d11_device.hpp"

namespace dxmt {

class D3D11DXGISurface : public IDXGISurface2 {

public:
  D3D11DXGISurface(ID3D11Resource *pResource);

  D3D11DXGISurface(ID3D11Resource *pParentResource, UINT Subresource);

  ULONG STDMETHODCALLTYPE AddRef();

  ULONG STDMETHODCALLTYPE Release();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData);

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData);

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown);

  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent);

  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppDevice);

  HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SURFACE_DESC *pDesc);

  HRESULT STDMETHODCALLTYPE Map(DXGI_MAPPED_RECT *pLockedRect, UINT MapFlags);

  HRESULT STDMETHODCALLTYPE Unmap();

  HRESULT STDMETHODCALLTYPE GetDC(BOOL Discard, HDC *phdc);

  HRESULT STDMETHODCALLTYPE ReleaseDC(RECT *pDirtyRect);

  HRESULT STDMETHODCALLTYPE GetResource(REFIID riid, void **ppParentResource, UINT *pSubresourceIndex);

private:
  ID3D11Resource *resource_;
  UINT subresource_;
  bool surface_from_subresource_;
};

} // namespace dxmt