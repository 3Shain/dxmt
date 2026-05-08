#include "d3d12_query_heap.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

namespace dxmt {

MTLD3D12QueryHeap::MTLD3D12QueryHeap(MTLD3D12Device *device,
                                     const D3D12_QUERY_HEAP_DESC &desc)
    : m_device(device), m_desc(desc) {
  m_device->AddRef();
  m_data.resize(desc.Count, 0);
  Logger::info(str::format("D3D12QueryHeap: type=", desc.Type,
                            " count=", desc.Count));
}

MTLD3D12QueryHeap::~MTLD3D12QueryHeap() { m_device->Release(); }

HRESULT STDMETHODCALLTYPE
MTLD3D12QueryHeap::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12QueryHeap) {
    *ppvObject = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12QueryHeap::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12QueryHeap::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12QueryHeap::GetPrivateData(REFGUID guid, UINT *data_size, void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12QueryHeap::SetPrivateData(REFGUID guid, UINT data_size,
                                  const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12QueryHeap::SetPrivateDataInterface(REFGUID guid,
                                           const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12QueryHeap::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12QueryHeap::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

} // namespace dxmt
