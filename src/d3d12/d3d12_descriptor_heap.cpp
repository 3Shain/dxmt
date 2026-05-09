#include "d3d12_descriptor_heap.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <cstring>

namespace dxmt {
static constexpr size_t DESC_SIZE = sizeof(D3D12Descriptor);
}

#define HTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "DescHeap::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

MTLD3D12DescriptorHeap::MTLD3D12DescriptorHeap(
    MTLD3D12Device *device, const D3D12_DESCRIPTOR_HEAP_DESC &desc)
    : m_device(device), m_desc(desc) {
  m_device->AddRef();
  HTRACE("DescriptorHeap ctor: type=%u num=%u desc_size=%zu allocating %llu bytes", desc.Type, desc.NumDescriptors,
         dxmt::DESC_SIZE, (unsigned long long)desc.NumDescriptors * dxmt::DESC_SIZE);
  try {
    m_descriptors.resize(desc.NumDescriptors);
    std::memset(m_descriptors.data(), 0,
                desc.NumDescriptors * sizeof(D3D12Descriptor));
    for (uint32_t i = 0; i < desc.NumDescriptors; i++) {
      m_descriptors[i].type = desc.Type;
    }
  } catch (...) {
    HTRACE("DescriptorHeap ctor: FAILED to allocate %u descriptors!", desc.NumDescriptors);
  }
  HTRACE("DescriptorHeap ctor: done, data=%p", (void *)m_descriptors.data());
  Logger::info(str::format("D3D12DescriptorHeap: type=", desc.Type,
                            " count=", desc.NumDescriptors,
                            " flags=", desc.Flags));
}

MTLD3D12DescriptorHeap::~MTLD3D12DescriptorHeap() { m_device->Release(); }

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12DescriptorHeap) {
    *ppvObject = ref(this);
    return S_OK;
  }
  HTRACE("QI unknown IID %s -> E_NOINTERFACE", str::format(riid).c_str());
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12DescriptorHeap::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetPrivateData(REFGUID guid, UINT *data_size,
                                        void *data) {
  HTRACE("GetPrivateData E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::SetPrivateData(REFGUID guid, UINT data_size,
                                       const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::SetPrivateDataInterface(REFGUID guid,
                                                const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

D3D12_DESCRIPTOR_HEAP_DESC *STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetDesc(D3D12_DESCRIPTOR_HEAP_DESC *__ret) {
  *__ret = m_desc;
  return __ret;
}

D3D12_CPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart(
    D3D12_CPU_DESCRIPTOR_HANDLE *__ret) {
  HTRACE("GetCPUDescriptorHandleForHeapStart");
  __ret->ptr = reinterpret_cast<SIZE_T>(m_descriptors.data());
  return __ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetGPUDescriptorHandleForHeapStart(
    D3D12_GPU_DESCRIPTOR_HANDLE *__ret) {
  HTRACE("GetGPUDescriptorHandleForHeapStart ptr=0x%llx", (unsigned long long)reinterpret_cast<UINT64>(m_descriptors.data()));
  __ret->ptr = reinterpret_cast<UINT64>(m_descriptors.data());
  return __ret;
}

} // namespace dxmt
