#include "d3d12_heap.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

#define HTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "Heap::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

MTLD3D12Heap::MTLD3D12Heap(MTLD3D12Device *device, const D3D12_HEAP_DESC &desc)
    : m_device(device), m_desc(desc) {
  m_device->AddRef();
  HTRACE("ctor: size=%llu alignment=%llu type=%u flags=0x%x",
    (unsigned long long)desc.SizeInBytes, (unsigned long long)desc.Alignment,
    desc.Properties.Type, desc.Flags);
}

MTLD3D12Heap::~MTLD3D12Heap() {
  HTRACE("dtor");
  m_device->Release();
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Heap::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12Heap) {
    *ppvObject = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12Heap::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12Heap::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Heap::GetPrivateData(REFGUID guid, UINT *data_size, void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Heap::SetPrivateData(REFGUID guid, UINT data_size,
                              const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Heap::SetPrivateDataInterface(REFGUID guid, const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Heap::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Heap::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

D3D12_HEAP_DESC *STDMETHODCALLTYPE
MTLD3D12Heap::GetDesc(D3D12_HEAP_DESC *__ret) {
  *__ret = m_desc;
  return __ret;
}

} // namespace dxmt
