#include "d3d12_command_allocator.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

#define CATRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "CmdAlloc::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

MTLD3D12CommandAllocator::MTLD3D12CommandAllocator(MTLD3D12Device *device,
                                                   D3D12_COMMAND_LIST_TYPE type)
    : m_device(device), m_type(type) {
  m_device->AddRef();
}

MTLD3D12CommandAllocator::~MTLD3D12CommandAllocator() {
  m_device->Release();
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocator::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12CommandAllocator) {
    *ppvObject = ref(this);
    return S_OK;
  }
  CATRACE("QI unknown IID %s -> E_NOINTERFACE", str::format(riid).c_str());
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12CommandAllocator::AddRef() {
  return ++m_refCount;
}

ULONG STDMETHODCALLTYPE MTLD3D12CommandAllocator::Release() {
  uint32_t rc = --m_refCount;
  if (!rc) {
    uint32_t rp = --m_refPrivate;
    if (!rp) {
      m_refPrivate += 0x80000000;
      delete this;
    }
  }
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocator::GetPrivateData(REFGUID guid, UINT *data_size,
                                          void *data) {
  CATRACE("GetPrivateData E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocator::SetPrivateData(REFGUID guid, UINT data_size,
                                         const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocator::SetPrivateDataInterface(REFGUID guid,
                                                  const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocator::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocator::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

HRESULT STDMETHODCALLTYPE MTLD3D12CommandAllocator::Reset() {
  return S_OK;
}

} // namespace dxmt
