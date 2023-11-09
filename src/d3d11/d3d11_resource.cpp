#include "d3d11_resource.hpp"

namespace dxmt {

MTLDXGIResource::MTLDXGIResource(ID3D11Resource *pRes) : m_resource(pRes) {}

MTLDXGIResource::~MTLDXGIResource() {}

HRESULT
STDMETHODCALLTYPE
MTLDXGIResource::QueryInterface(REFIID riid, void **ppvObject) {
  return m_resource->QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE MTLDXGIResource::AddRef() {
  return m_resource->AddRef();
}

ULONG STDMETHODCALLTYPE MTLDXGIResource::Release() {
  return m_resource->Release();
}

HRESULT STDMETHODCALLTYPE MTLDXGIResource::SetPrivateData(REFGUID guid,
                                                          UINT data_size,
                                                          const void *data) {
  return m_resource->SetPrivateData(guid, data_size, data);
}

HRESULT STDMETHODCALLTYPE
MTLDXGIResource::SetPrivateDataInterface(REFGUID guid, const IUnknown *object) {
  return m_resource->SetPrivateDataInterface(guid, object);
}

HRESULT STDMETHODCALLTYPE MTLDXGIResource::GetPrivateData(REFGUID guid,
                                                          UINT *data_size,
                                                          void *data) {
  return m_resource->GetPrivateData(guid, data_size, data);
}

HRESULT STDMETHODCALLTYPE MTLDXGIResource::GetParent(REFIID riid,
                                                     void **parent) {
  return GetDevice(riid, parent);
}

HRESULT STDMETHODCALLTYPE MTLDXGIResource::GetDevice(REFIID riid,
                                                     void **ppDevice) {
  Com<ID3D11Device> device;
  m_resource->GetDevice(&device);
  return device->QueryInterface(riid, ppDevice);
}

HRESULT STDMETHODCALLTYPE
MTLDXGIResource::GetSharedHandle(HANDLE *pSharedHandle){IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE MTLDXGIResource::GetUsage(DXGI_USAGE *pUsage){
    IMPLEMENT_ME}

HRESULT STDMETHODCALLTYPE
    MTLDXGIResource::SetEvictionPriority(UINT EvictionPriority) {
  m_resource->SetEvictionPriority(EvictionPriority);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIResource::GetEvictionPriority(UINT *pEvictionPriority) {
  *pEvictionPriority = m_resource->GetEvictionPriority();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLDXGIResource::CreateSubresourceSurface(UINT index, IDXGISurface2 **surface) {
  ERR_ONCE("Stub");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE MTLDXGIResource::CreateSharedHandle(
    const SECURITY_ATTRIBUTES *attributes, DWORD access, const WCHAR *name,
    HANDLE *handle) {
  ERR_ONCE("Stub");
  return E_NOTIMPL;
}

} // namespace dxmt