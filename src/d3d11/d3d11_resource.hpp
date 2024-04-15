#pragma once
#include "d3d11_1.h"

namespace dxmt {
class MTLDXGIResource : public IDXGIResource1 {
public:
  MTLDXGIResource(ID3D11Resource *pResource);
  ~MTLDXGIResource();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;

  ULONG STDMETHODCALLTYPE AddRef() final;

  ULONG STDMETHODCALLTYPE Release() final;

  HRESULT
  STDMETHODCALLTYPE
  SetPrivateData(REFGUID guid, UINT data_size, const void *data) final;

  HRESULT
  STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown *object) final;

  HRESULT
  STDMETHODCALLTYPE
  GetPrivateData(REFGUID guid, UINT *data_size, void *data) final;

  HRESULT
  STDMETHODCALLTYPE
  GetParent(REFIID riid, void **parent) final;

  HRESULT
  STDMETHODCALLTYPE
  GetDevice(REFIID riid, void **device) final;

  HRESULT
  STDMETHODCALLTYPE
  GetSharedHandle(HANDLE *pSharedHandle) final;

  HRESULT
  STDMETHODCALLTYPE
  GetUsage(DXGI_USAGE *pUsage) final;

  HRESULT
  STDMETHODCALLTYPE
  SetEvictionPriority(UINT EvictionPriority) final;

  HRESULT
  STDMETHODCALLTYPE
  GetEvictionPriority(UINT *pEvictionPriority) final;

  HRESULT
  STDMETHODCALLTYPE
  CreateSubresourceSurface(UINT index, IDXGISurface2 **surface) final;

  HRESULT
  STDMETHODCALLTYPE
  CreateSharedHandle(const SECURITY_ATTRIBUTES *attributes, DWORD access,
                     const WCHAR *name, HANDLE *handle) final;

private:
  ID3D11Resource *m_resource;
};
} // namespace dxmt